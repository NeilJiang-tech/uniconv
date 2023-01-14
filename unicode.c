/* ********************************************************** */
/* -*- unicode.c -*- Unicode conversion/support functions -*- */
/* ********************************************************** */
/* Tyler Besselman (C) January 2023, licensed under GPLv2     */
/* ********************************************************** */

// For unicode function definitions
#include "unicode.h"

// For bzero. This could be replaced with memset if more portability is desired.
#include <strings.h>

// Do note that everything in this file can be made faster with CPU-specific optimizations.
// On modern systems, for instance, we have 64-bit native registers. Doing math in them or
//   accessing memory on 64-bit boundaries would be faster. I leave this up to the compiler/cpu
//   itself however.
// This code should be as optimized as it needs to be.
// Anything more specific would just be overkill.

// The only optimization I could see was if there was a fast way to do UTF8/16 conversion directly.
// On the other hand, I think converting to/from codepoints is much cleaner.

// Byte swap routine for 16, 32 bytes.
#define __byte_swap_16(i)   ({ uint16_t x = (i); (((x >> 8) & 0x00FF) | ((x << 8) & 0xFF00)); })
#define __byte_swap_32(i)   (__builtin_bswap32((i)))

/* ************************** */
/* -*- helpful constants -*- */
/* ************************** */

// UTF-16 Surrogate codepoint high region
static const unipoint_t SURROGATE_HIGH_START    = 0xD800;
static const unipoint_t SURROGATE_HIGH_END      = 0xDBFF;

// UTF-16 Surrogate codepoint low region
static const unipoint_t SURROGATE_LOW_START     = 0xDC00;
static const unipoint_t SURROGATE_LOW_END       = 0xDFFF;

// UTF-16 single char limit. Any codepoint > than this value uses surrogate bytes.
static const unipoint_t UTF16_ONE_CHAR_LIMIT    = 0x10000;

// UTF-8 single char limit. Any codepoint < this value uses 1 char.
static const unipoint_t UTF8_ONE_CHAR_LIMIT     = 0x80;

// UTF-8 two char limit. Any codepoint < this value uses 2 chars.
static const unipoint_t UTF8_TWO_CHAR_LIMIT     = 0x800;

// UTF-8 three char limit. Any codepoint < this value uses 3 chars.
// (anything greater than this uses 4 chars)
static const unipoint_t UTF8_THREE_CHAR_LIMIT   = 0x10000;

// This is the final unicode codepoint. Anything higher is invalid.
static const unipoint_t UNICODE_FINAL_POINT     = 0x10FFFF;

// The maximum number of characters in a UTF-8 sequence
static const size_t UTF8_SEQ_MAX_CHARS          = 4;

// Unicode replacement character. This is used to replace invalid sequences.
static const unipoint_t UNICODE_REPL_CHAR       = 0xFFFD;

// Number of bytes used to encode a single codepoint in UTF-8 indexed by the first byte.
// That is, the first byte of a UTF-8 encoded codepoint can be used as the index to this
//   table, and the resulting value is the number of following bytes needed to decode
//   the single codepoint in its entirety.
static const uint8_t UTF8_TRAILING_COUNT[0x100] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5
};

// Excess value to be subtracted from a codepoint decoded from UTF-8.
// This value is the metadata used to indicate the number of bytes in a single UTF-8
//   encoded codepoint. It should be subtracted from the decoded value.
static const unipoint_t UTF8_ENCODING_OVERFLOW[5] = {
                                                          (0), // '0' bytes (this isn't a thing)
                                                          (0), // 1 byte
                                  (0xC0 <<  6) | (0x80 <<  0), // 2 bytes
                   (0xE0 << 12) | (0x80 <<  6) | (0x80 <<  0), // 3 bytes
    (0xF0 << 18) | (0x80 << 12) | (0x80 <<  6) | (0x80 <<  0), // 4 bytes
};

// Bitmask giving the marker bits for the leading UTF-8 encoded byte.
// For a codepoint requiring n bytes in UTF-8, the initial byte should have
//   its low bits set and be or'd with this table indexed by n.
static const utf8_char_t UTF8_INITIAL_MASK[5] = {
    0b00000000, // '0' bytes (this isn't a thing)
    0b00000000, // 1 byte
    0b11000000, // 2 bytes
    0b11100000, // 3 bytes
    0b11110000, // 4 bytes
};

/* ************************ */
/* -*- static functions -*- */
/* ************************ */

// Validate the provided codepoint, returning 0 on success.
static inline int __codepoint_is_valid(unipoint_t codepoint);


// Calculate the number of characters needed to encode the provided codepoint.
static inline size_t __utf8_chars_for_codepoint(unipoint_t codepoint);

// Raw decode for UTF-8 given a sequence buffer and a char count.
static inline unipoint_t __utf8_decode(utf8_char_t *src, size_t cnt);

// Raw decode for UTF-16 given a leading and trailing char (these are expected to be surrogates).
static inline unipoint_t __utf16_decode(utf16_char_t leading_char, utf16_char_t trailing_char);


// Encode a single codepoint as UTF-8 in the provided buffer.
static inline size_t __utf8_from_codepoint(unipoint_t codepoint, utf8_char_t *dest, size_t dest_size, bool);

// Encode a single codepoint as UTF-16 in the provided buffer, swapping byte order if necessary.
static inline size_t __utf16_from_codepoint(unipoint_t codepoint, utf16_char_t *dest, size_t dest_size, bool swap);

// Encode a single codepoint as UTF-32 in the provided buffer, swapping byte order if necessary.
static inline size_t __utf32_from_codepoint(unipoint_t codepoint, utf32_char_t *dest, size_t dest_size, bool swap);


// Read a single codepoint from the provided UTF-8 buffer. Return the number of chars consumed in the `consumed` argument.
static inline unipoint_t __codepoint_from_utf8(utf8_char_t *src, size_t src_size, size_t *consumed, bool);

// Read a single codepoint from the provided UTF-16 buffer, swapping byte order if necessary. Return the number of chars consumed in the `consumed` argument.
static inline unipoint_t __codepoint_from_utf16(utf16_char_t *src, size_t src_size, size_t *consumed, bool swap);

// Read a codepoint from the provided UTF-32 buffer, byte swapping if necessary.
static inline unipoint_t __codepoint_from_utf32(utf32_char_t *src, size_t src_size, size_t *consumed, bool swap);

/* ********************************************** */
/* -*- static helper for codepoint validation -*- */
/* ********************************************** */

static inline int __codepoint_is_valid(unipoint_t codepoint)
{
    // Ensure this is not a high surrogate
    if (SURROGATE_HIGH_START <= codepoint && codepoint <= SURROGATE_HIGH_END)
        return 2;

    // Ensure this is not a low surrogate
    if (SURROGATE_LOW_START <= codepoint && codepoint <= SURROGATE_LOW_END)
        return 2;

    // Ensure this is not past the end of valid unicode points.
    if (codepoint > UNICODE_FINAL_POINT)
        return 3;

    // This codepoint seems to be valid.
    return 0;
}

/* ************************************************************************ */
/* -*- static helpers for conversion between code points and UTF8/16/32 -*- */
/* ************************************************************************ */

static inline size_t __utf8_chars_for_codepoint(unipoint_t codepoint)
{
    if (codepoint < UTF8_ONE_CHAR_LIMIT) {
        return 1;
    } else if (codepoint < UTF8_TWO_CHAR_LIMIT) {
        return 2;
    } else if (codepoint < UTF8_THREE_CHAR_LIMIT) {
        return 3;
    } else {
        return 4;
    }
}

static inline unipoint_t __utf8_decode(utf8_char_t *src, size_t cnt)
{
    // This is where we calculate the final codepoint.
    unipoint_t codepoint = 0;

    // First, simply sum all of the UTF-8 characters.
    // We adjust the result after.
    switch (cnt)
    {
        case 4: codepoint += (*src++); codepoint <<= 6;
        case 3: codepoint += (*src++); codepoint <<= 6;
        case 2: codepoint += (*src++); codepoint <<= 6;
        case 1: codepoint += (*src);
    }

    // Subtract the encoding metadata accumulated by each byte, returning the result.
    return (codepoint - UTF8_ENCODING_OVERFLOW[cnt]);
}

static inline unipoint_t __utf16_decode(utf16_char_t leading_char, utf16_char_t trailing_char)
{
    // Similarly to encoding, for a raw codepoint yyyyyyyyyyxxxxxxxxxx + 0x10000,
    //   the first char is 0xD800 | yyyyyyyyyy, and the second char is 0xDC00 | xxxxxxxxxx.
    // To decode, then, we clear 0xD800 from the first char, 0xDC00 from the second,
    //   and add 0x10000 to the result.
    // In fact, this is equivalent to simply masking the lower 10 bits of each char,
    //   shifting the leader char to the right 10, or'ing them together, and
    //   adding 0x10000 to the result.
    // This is what I do here.
    unipoint_t leading  = ((leading_char  & 0x3FF) << 10);
    unipoint_t trailing = ((trailing_char & 0x3FF) <<  0);

    // Simple as that.
    return (leading | trailing) + UTF16_ONE_CHAR_LIMIT;
}

static inline size_t __utf8_from_codepoint(unipoint_t codepoint, utf8_char_t *dest, size_t dest_size, bool)
{
    // Shortcut for one-char encoding
    if (codepoint < UTF8_ONE_CHAR_LIMIT)
    {
        // Just copy.
        (*dest) = codepoint;

        return 1;
    }

    // Determine how many chars we will need to encode this codepoint.
    size_t char_count = __utf8_chars_for_codepoint(codepoint);

    // Bounds check destination buffer.
    if (char_count > dest_size)
    {
        // Write NULL chars to the destination.
        bzero(dest, dest_size * sizeof(utf8_char_t));

        // Buffer out of space!
        return 0;
    }

    // Nice cascading switch statement to do encoding.
    // UTF-8 encoding has the form 0b10xxxxxx on all bytes but the initial.
    // The initial byte has a differing mask depending on the number of bytes total.
    // See the UTF8_INITIAL_MASK for a listing of these masks.
    switch (char_count)
    {
        case 4: dest[3] = (codepoint | 0b10000000) & 0b10111111; codepoint >>= 6;
        case 3: dest[2] = (codepoint | 0b10000000) & 0b10111111; codepoint >>= 6;
        case 2: dest[1] = (codepoint | 0b10000000) & 0b10111111; codepoint >>= 6;
                dest[0] =  codepoint | UTF8_INITIAL_MASK[char_count];
    }

    // Return however many characters we used.
    return char_count;
}

static inline size_t __utf16_from_codepoint(unipoint_t codepoint, utf16_char_t *dest, size_t dest_size, bool swap)
{
    // Check if we can encode in a single character.
    if (codepoint < UTF16_ONE_CHAR_LIMIT) {
        // One to one character to codepoint mapping in this range.
        (*dest) = ((swap) ? __byte_swap_16(codepoint) : codepoint);

        // We used only a single char
        return 1;
    } else {
        // Range check to ensure we can expand this codepoint into two UTF-16 chars.
        if (dest_size >= 2) {
            // Encode this codepoint as a high surrogate and a low surrogate.
            // For a codepoint of the form yyyyyyyyyyxxxxxxxxxx + 0x10000,
            //   the first char is encoded as 0xD800 | yyyyyyyyyy (10 bits)
            //   and the second char is encoded as 0xDC00 | xxxxxxxxxx (10 bits)
            codepoint -= UTF16_ONE_CHAR_LIMIT;

            // Store 10 bits with the high/low surrogate prefixes.
            dest[0] = ((codepoint >> 10) & 0x3FF) | SURROGATE_HIGH_START; // high 10 bits
            dest[1] = ((codepoint >>  0) & 0x3FF) | SURROGATE_LOW_START;  // low  10 bits

            // Byte swap if requested.
            if (swap)
            {
                dest[0] = __byte_swap_16(dest[0]);
                dest[1] = __byte_swap_16(dest[1]);
            }

            // We used 2 chars.
            return 2;
        } else {
            // Encode as a NULL char.
            (*dest) = 0;

            // Buffer out of space!
            return 1;
        }
    }
}

static inline size_t __utf32_from_codepoint(unipoint_t codepoint, utf32_char_t *dest, size_t dest_size, bool swap)
{
    // Just cast.
    utf32_char_t result = ((utf32_char_t)codepoint);

    // Byte swap if necessary
    (*dest) = ((swap) ? __byte_swap_32(result) : result);

    // We used one character
    return 1;
}

static inline unipoint_t __codepoint_from_utf8(utf8_char_t *src, size_t src_size, size_t *consumed, bool)
{
    // We always have at least one char available here.
    utf8_char_t leading_char = (*src);

    // Use the leading char to determine how many chars to consume.
    size_t char_count = UTF8_TRAILING_COUNT[leading_char] + 1;

    // Ensure the buffer holds a full codepoint based on the calculated number of chars.
    if (src_size < char_count)
    {
        // We don't have enough bytes...
        if (consumed)
            (*consumed) = src_size;

        // Return replacement character.
        return UNICODE_REPL_CHAR;
    }

    // Anything more than UTF8_SEQ_MAX_CHARS chars is a malformed codepoint.
    if (char_count > UTF8_SEQ_MAX_CHARS)
    {
        // This is malformed.
        if (consumed)
            (*consumed) = char_count;

        // Return replacement character.
        return UNICODE_REPL_CHAR;
    }

    // Decode the codepoint from the buffer with the given char count.
    unipoint_t codepoint = __utf8_decode(src, char_count);

    // We consumed the leading char + all trailing characters.
    if (consumed)
        (*consumed) = char_count;

    // Verify this is a valid unicode codepoint.
    if (__codepoint_is_valid(codepoint))
        return UNICODE_REPL_CHAR; // This was an invalid codepoint.

    // It is invalid to encode a codepoint in a less than optimal sequence.
    if (__utf8_chars_for_codepoint(codepoint) != char_count)
        return UNICODE_REPL_CHAR; // Return replacement character

    // Return the calculated result without encoding metadata.
    return codepoint;
}

static inline unipoint_t __codepoint_from_utf16(utf16_char_t *src, size_t src_size, size_t *consumed, bool swap)
{
    // We always have at least one char available here, swap if requested.
    utf16_char_t leading_char = ((swap) ? __byte_swap_16(*src) : (*src));
    src++; // We consumed one char.

    // If the first character is a high surrogate, we need to decode the following character as it's low surrogate.
    if (SURROGATE_HIGH_START <= leading_char && leading_char <= SURROGATE_HIGH_END) {
        // Ensure we have another character in the buffer.
        if (src_size < 2)
        {
            // This high surrogate has no corresponding low surrogate.
            if (consumed)
                (*consumed) = 1;

            // Return replacement character.
            return UNICODE_REPL_CHAR;
        }

        // Read the next character in, swapping if requested.
        // This should be a corresponding low surrogate.
        utf16_char_t trailing_char = ((swap) ? __byte_swap_16(*src) : (*src));

        // Check if the trailing char is outside the low surrogate range
        if (trailing_char <= SURROGATE_LOW_START || SURROGATE_LOW_END <= trailing_char)
        {
            // This high surrogate has no corresponding low surrogate.
            if (consumed)
                (*consumed) = 1;

            // Return replacement character.
            return UNICODE_REPL_CHAR;
        }

        // Decode the two character codepoint.
        unipoint_t codepoint = __utf16_decode(leading_char, trailing_char);

        // We used 2 chars.
        if (consumed)
            (*consumed) = 2;

        // Verify this is a valid unicode codepoint.
        if (__codepoint_is_valid(codepoint))
            return UNICODE_REPL_CHAR; // This was an invalid codepoint.

        // Everything is ok.
        return codepoint;
    } else if (SURROGATE_LOW_START <= leading_char && leading_char <= SURROGATE_LOW_END) {
        // It is illegal to encode a low surrogate with no preceding high surrogate.
        // These values are reserved codepoints in the unicode standard.
        if (consumed)
            (*consumed) = 1;

        // Return a replacement character.
        return UNICODE_REPL_CHAR;
    } else {
        if (consumed)
            (*consumed) = 1;

        // This codepoint takes <= 16 bits to encode.
        // It is encoded directly in UTF-16.
        return (unipoint_t)leading_char;
    }
}

static inline unipoint_t __codepoint_from_utf32(utf32_char_t *src, size_t src_size, size_t *consumed, bool swap)
{
    // UTF-32 is easy. Just bitswap if requested.
    unipoint_t codepoint = (unipoint_t)((swap) ? __byte_swap_32(*src) : (*src));

    // We always consume 1 char.
    if (consumed)
        (*consumed) = 1;

    // And ensure this is a valid codepoint
    if (__codepoint_is_valid(codepoint))
        return UNICODE_REPL_CHAR; // This was an invalid codepoint.

    // Just cast.
    return codepoint;
}

/* ************************************* */
/* -*- encoding conversion functions -*- */
/* ************************************* */

// Do UTF-X to UTF-Y conversion. These functions are all the same with bit widths changed.
#define UTFCONV(X, Y)                                                                                       \
    do {                                                                                                    \
        /* These are useful for calculating the number of chars consumed in each buffer */                  \
        utf ## Y ## _char_t *dest_ptr = dest;                                                               \
        utf ## X ## _char_t *src_ptr = src;                                                                 \
                                                                                                            \
        /* For range checking */                                                                            \
        utf ## Y ## _char_t *dest_end = dest + (*dest_size);                                                \
        utf ## X ## _char_t *src_end = src + src_size;                                                      \
                                                                                                            \
        /* Loop until one of the two buffers is exhausted. */                                               \
        while ((dest < dest_end) && (src < src_end))                                                        \
        {                                                                                                   \
            /* How many chars were consumed in this loop? */                                                \
            size_t consumed;                                                                                \
                                                                                                            \
            /* Read out the next codepoint from the src buffer. */                                          \
            unipoint_t codepoint = __codepoint_from_utf ## X(src, (src_end - src), &consumed, swap);        \
                                                                                                            \
            /* Write out the UTF-Y version of the read codepoint. */                                        \
            size_t dest_consumed = __utf ## Y ## _from_codepoint(codepoint, dest, (dest_end - dest), swap); \
                                                                                                            \
            /* The '0' codepoint is NULL and represents the end of a string. */                             \
            if (!codepoint)                                                                                 \
                break;                                                                                      \
                                                                                                            \
            /* We don't count the null terminator as being converted. */                                    \
            /* Increment these after the above check. */                                                    \
            dest += dest_consumed;                                                                          \
            src += consumed;                                                                                \
        }                                                                                                   \
                                                                                                            \
        /* We consumed the difference in the current dest pointer from the original value. */               \
        (*dest_size) = (dest - dest_ptr);                                                                   \
                                                                                                            \
        /* Similarly for the src buffer. */                                                                 \
        return (src - src_ptr);                                                                             \
    } while (0)

size_t enc_utf8_to_utf16(utf16_char_t *dest, size_t *dest_size, utf8_char_t *src, size_t src_size, bool swap)
{ UTFCONV(8, 16); }

size_t enc_utf8_to_utf32(utf32_char_t *dest, size_t *dest_size, utf8_char_t *src, size_t src_size, bool swap)
{ UTFCONV(8, 32); }

size_t enc_utf16_to_utf8(utf8_char_t *dest, size_t *dest_size, utf16_char_t *src, size_t src_size,  bool swap)
{ UTFCONV(16, 8); }

size_t enc_utf16_to_utf32(utf32_char_t *dest, size_t *dest_size, utf16_char_t *src, size_t src_size,  bool swap)
{ UTFCONV(16, 32); }

size_t enc_utf32_to_utf16(utf16_char_t *dest, size_t *dest_size, utf32_char_t *src, size_t src_size,  bool swap)
{ UTFCONV(32, 16); }

size_t enc_utf32_to_utf8(utf8_char_t *dest, size_t *dest_size, utf32_char_t *src, size_t src_size,  bool swap)
{ UTFCONV(32, 8); }

#undef UTFCONV

/* ******************************* */
/* -*- buffer sizing functions -*- */
/* ******************************* */

size_t utf8_in_utf16_len(utf8_char_t *str, bool)
{
    // This is the resulting UTF-16 length.
    size_t length = 0;

    // Loop through the string until we encounter a null-terminator.
    // `c` is the leading bit of each UTF-8 codepoint sequence.
    for (utf8_char_t c = (*str); c; c = (*str++))
    {
        // Lookup how many chars trail this one, skip to the next leading char.
        size_t trailing_chars = UTF8_TRAILING_COUNT[c];
        str += trailing_chars;

        // This just so happens to collide, which is nice.
        // Any codepoint that takes 4 chars in UTF-8 takes 2 chars in UTF-16.
        // All other codepoints take 1 char in UTF-16;
        length += ((trailing_chars == 3) ? 2 : 1);
    }

    // Return the calculated result.
    return length;
}

size_t utf8_in_utf32_len(utf8_char_t *str, bool)
{
    // This is the resulting UTF-32 length.
    size_t length = 0;

    // Loop through the string until we encounter a null-terminator.
    // `c` is the leading bit of each UTF-8 codepoint sequence.
    for (utf8_char_t c = (*str); c; c = (*str++))
    {
        // Lookup how many chars trail this one, skip to the next leading char.
        size_t trailing_chars = UTF8_TRAILING_COUNT[c];
        str += trailing_chars;

        // UTF-32 takes one char per codepoint.
        length++;
    }

    // Return the calculated result.
    return length;
}

size_t utf16_in_utf8_len(utf16_char_t *str, bool swap)
{
    // This is the resulting UTF-8 length.
    size_t length = 0;

    // Loop through the string until we encounter a null-terminator.
    // `c` is the leading bit of each UTF-16 sequence.
    for (utf16_char_t c = (*str); c; c = (*str++))
    {
        // Byte swap if requested
        if (swap)
            c = __byte_swap_16(c);

        // Codepoints which take 2 chars in UTF-16 will start with a high surrogate.
        if (SURROGATE_HIGH_START <= c && c <= SURROGATE_HIGH_END) {
            // This just so happens to collide, which is nice.
            // Any codepoint that takes 2 chars in UTF-16 takes 4 chars in UTF-16.
            length += 4;

            // Skip the succeeding low surrogate.
            str++;
        } else {
            // Otherwise, c is actually a raw codepoint and we have to figure out the
            //   specific range within which it falls.
            length += __utf8_chars_for_codepoint(c);
        }
    }

    // Return the calculated result.
    return length;
}

size_t utf16_in_utf32_len(utf16_char_t *str, bool swap)
{
    // This is the resulting UTF-32 length.
    size_t length = 0;

    // Loop through the string until we encounter a null-terminator.
    // `c` is the leading bit of each UTF-16 sequence.
    for (utf16_char_t c = (*str); c; c = (*str++))
    {
        // Byte swap if requested
        if (swap)
            c = __byte_swap_16(c);

        // Codepoints which take 2 chars in UTF-16 will start with a high surrogate.
        if (SURROGATE_HIGH_START <= c && c <= SURROGATE_HIGH_END)
            str++; // Skip the succeeding low surrogate.

        // UTF-32 takes one char per codepoint.
        length++;
    }

    // Return the calculated result.
    return length;
}

size_t utf32_in_utf8_len(utf32_char_t *str, bool swap)
{
    // This is the resulting UTF-8 length.
    size_t length = 0;

    // Loop through the string until we encounter a null-terminator.
    for (utf32_char_t c = (*str); c; c = (*str++))
    {
        // Byte swap if requested
        if (swap)
            c = __byte_swap_32(c);

        // Figure out how many chars this char needs in UTF-8 by finding which range it lies within.
        length += __utf8_chars_for_codepoint(c);
    }

    // Return the calculated result.
    return length;
}

size_t utf32_in_utf16_len(utf32_char_t *str, bool swap)
{
    // This is the resulting UTF-16 length.
    size_t length = 0;

    // Loop through the string until we encounter a null-terminator.
    for (utf32_char_t c = (*str); c; c = (*str++))
    {
        // Byte swap if requested
        if (swap)
            c = __byte_swap_32(c);

        // Codepoints in UTF-16 are either one or two chars depending on which plane they fall in.
        length += ((c > UTF16_ONE_CHAR_LIMIT) ? 2 : 1);
    }

    // Return the calculated result.
    return length;
}

/* *********************************** */
/* -*- string validation functions -*- */
/* *********************************** */

int utf8_validate(utf8_char_t *str, bool swap)
{
    // Loop through the string until we encounter a null-terminator.
    // `c` is the leading bit of each UTF-8 codepoint sequence.
    for (utf8_char_t c = (*str); c; c = (*str))
    {
        // Lookup how many chars trail this one, skip to the next leading char.
        size_t char_count = UTF8_TRAILING_COUNT[c] + 1;

        // UTF-8 does not support sequences of more than 4 chars.
        if (char_count > UTF8_SEQ_MAX_CHARS)
            return 6;

        // Ensure all trailing characters have a valid mark.
        // That is, they all start with a 1 bit followed by a 0 bit.
        for (size_t j = 1; j < char_count; j++)
        {
            // We either have an early null-terminator or a bad char.
            if (!str[j] || !(str[j] >> 7) || ((str[j] >> 6) & 1))
                return 4;
        }

        // Decode the next codepoint
        unipoint_t codepoint = __utf8_decode(str, char_count);

        // Check if the decoded codepoint is valid.
        int validity_result = __codepoint_is_valid(codepoint);

        // If validity_result is set, this codepoint was not valid.
        if (validity_result)
            return validity_result;

        // Codepoints must be encoded by their minimal sequence.
        if (char_count != __utf8_chars_for_codepoint(codepoint))
            return 6;

        // Skip past the last sequence
        str += char_count;
    }

    return 0;
}

int utf16_validate(utf16_char_t *str, bool swap)
{
    // Loop through the string until we encounter a null-terminator.
    // `c` is the leading bit of each UTF-16 sequence.
    for (utf16_char_t c = (*str); c; c = (*str++))
    {
        // Byte swap if requested
        if (swap)
            c = __byte_swap_16(c);

        // Codepoints which take 2 chars in UTF-16 will start with a high surrogate.
        if (SURROGATE_HIGH_START <= c && c <= SURROGATE_HIGH_END) {
            utf16_char_t next_char = (*str++);

            // Check we're not at the end of the string.
            if (!next_char)
                return 1; // This string is missing a low surrogate.

            if (next_char < SURROGATE_LOW_START || SURROGATE_LOW_END < next_char)
            {
                // This character isn't a low surrogate.
                return 1;
            }

            // Decode this codepoint
            unipoint_t codepoint = __utf16_decode(c, next_char);

            // Check if the decoded codepoint is valid.
            int validity_result = __codepoint_is_valid(codepoint);

            // If validity_result is set, this codepoint was not valid.
            if (validity_result)
                return validity_result;

            // This was a valid sequence.
        } else {
            // This codepoint is only a single char.
            unipoint_t codepoint = (unipoint_t)c;

            // Check if the decoded codepoint is valid.
            int validity_result = __codepoint_is_valid(codepoint);

            // If validity_result is set, this codepoint was not valid.
            if (validity_result)
                return validity_result;

            // This was a valid sequence.
        }
    }

    // This is a valid string
    return 0;
}

int utf32_validate(utf32_char_t *str, bool swap)
{
    // Loop through the string until we encounter a null-terminator.
    for (utf32_char_t c = (*str); c; c = (*str++))
    {
        // Byte swap if requested
        if (swap)
            c = __byte_swap_32(c);

        // Simply cast to codepoint and check if the underlying point is valid.
        int validity_result = __codepoint_is_valid((unipoint_t)c);

        // If validity_result is set, this codepoint was not valid.
        if (validity_result)
            return validity_result;
    }

    // This is a valid string
    return 0;
}

/* ******************************* */
/* -*- string length functions -*- */
/* ******************************* */

// UTF-X strlen implementation. Just the naive version.
#define STRLEN(X)                               \
    do {                                        \
        utf ## X ## _char_t *s = str;           \
                                                \
        /* Loop until (*s) == 0  */             \
        for ( ; (*s); s++) ;                    \
                                                \
        /* strlen is pointer to end - start */  \
        return (s - str);                       \
    } while (0)

size_t strlen_utf8(utf8_char_t *str)
{ STRLEN(8); }

size_t strlen_utf16(utf16_char_t *str)
{ STRLEN(16); }

size_t strlen_utf32(utf32_char_t *str)
{ STRLEN(32); }

#undef STRLEN
