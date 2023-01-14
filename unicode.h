/* ********************************************************** */
/* -*- unicode.h -*- Unicode conversion/support functions -*- */
/* ********************************************************** */
/* Tyler Besselman (C) January 2023, licensed under GPLv2     */
/* ********************************************************** */

#ifndef __UNICODE__
#define __UNICODE__ 1

// For bool, uintX_t
#include <stdbool.h>
#include <stdint.h>

// For size_t
#include <stddef.h>

// Note: All functions in this file assume char-length null terminated strings.
//   That is, for a UTF-XX encoded string, it is assumed there are XX 0 bits
//     following the end of the encoded string.
//   If this is not the case, you will likely find many segfaults.
//   You have been warned...

/* ********************************* */
/* -*- unicode/encoding typedefs -*- */
/* ********************************* */

// Unicode point type
typedef uint32_t unipoint_t;

// UTF-32 character type
typedef uint32_t utf32_char_t;

// UTF-16 character type
typedef uint16_t utf16_char_t;

// UTF-8 character type
typedef uint8_t utf8_char_t;

/* ************************************* */
/* -*- encoding conversion functions -*- */
/* ************************************* */

// Note that while these functions try to perform some level of validation of inputs,
//   they do not indicate errors to the caller, and may emit excess NULL bytes if
//   invalid sequences are encountered in the input.
// See the string validation functions below to check if invalid sequences appear
//   in your UTF-X strings.

// Translate UTF8 to UTFX, storing the # of consumed chars in dest_size. Byte swap if requested.
// Return the number of chars of the src buffer that were converted.
extern size_t enc_utf8_to_utf16(utf16_char_t *dest, size_t *dest_size, utf8_char_t *src, size_t src_size, bool swap);
extern size_t enc_utf8_to_utf32(utf32_char_t *dest, size_t *dest_size, utf8_char_t *src, size_t src_size, bool swap);

// Translate UTF8 to UTFX, storing the # of consumed chars in dest_size. Byte swap if requested.
// Return the number of chars of the src buffer that were converted.
extern size_t enc_utf16_to_utf8(utf8_char_t *dest, size_t *dest_size, utf16_char_t *src, size_t src_size, bool swap);
extern size_t enc_utf16_to_utf32(utf32_char_t *dest, size_t *dest_size, utf16_char_t *src, size_t src_size, bool swap);

// Translate UTF8 to UTFX, storing the # of consumed chars in dest_size. Byte swap if requested.
// Return the number of chars of the src buffer that were converted.
extern size_t enc_utf32_to_utf16(utf16_char_t *dest, size_t *dest_size, utf32_char_t *src, size_t src_size, bool swap);
extern size_t enc_utf32_to_utf8(utf8_char_t *dest, size_t *dest_size, utf32_char_t *src, size_t src_size, bool swap);

/* ******************************* */
/* -*- buffer sizing functions -*- */
/* ******************************* */

// Note that the following functions assume the UTF-X strings are well formed.
// That is, they will not check UTF-16 high surrogates have matching low surrogates
//   or that UTF-8 sequences actually contain the right number of characters.
// See the string validation functions below to actually check this is the case.

// Get the number of characters for a UTF-8 string encoded in UTF-16/32
extern size_t utf8_in_utf16_len(utf8_char_t *str, bool swap);
extern size_t utf8_in_utf32_len(utf8_char_t *str, bool swap);

// Get the number of characters for a UTF-16 string encoded in UTF-8/32
extern size_t utf16_in_utf8_len(utf16_char_t *str, bool swap);
extern size_t utf16_in_utf32_len(utf16_char_t *str, bool swap);

// Get the number of characters for a UTF-32 string encoded in UTF-8/16
extern size_t utf32_in_utf8_len(utf32_char_t *str, bool swap);
extern size_t utf32_in_utf16_len(utf32_char_t *str, bool swap);

/* *********************************** */
/* -*- string validation functions -*- */
/* *********************************** */

// Validate all UTF-8 sequences have the correct marking on the first character,
//   and that no bad characters appear in any sequence.
// Additionally ensure no high or low surrogates are encoded in UTF-8.
// Ensure all codepoints fall within the valid unicode range.
// Return is non-zero for malformed strings, 0 for valid strings.
extern int utf8_validate(utf8_char_t *str, bool swap);

// Validate all UTF-16 sequences are one or two characters and that all two char
//   sequences are one high surrogate followed by one low surrogate.
// Additionally ensure there are no 'naked' surrogates without a match.
// Ensure all codepoints fall within the valid unicode range.
// Return is non-zero for malformed strings, 0 for valid strings.
extern int utf16_validate(utf16_char_t *str, bool swap);

// Validate all UTF-32 codepoints.
// This involves ensuring no UTF-16 surrogates occur and all codepoints fall
//   within the range of valid unicode points.
// Return is non-zero for malformed strings, 0 for valid strings.
extern int utf32_validate(utf32_char_t *str, bool swap);

/* ******************************* */
/* -*- string length functions -*- */
/* ******************************* */

// These are CPU-agnostic. Calculate the number of native chars in a string encoded a certain way.
extern size_t strlen_utf8(utf8_char_t *str);
extern size_t strlen_utf16(utf16_char_t *str);
extern size_t strlen_utf32(utf32_char_t *str);

#endif /* !defined(__UNICODE__) */
