#include "unicode.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define TEST_X_TO_Y(str, X, Y, path)                                                                                                      \
    do {                                                                                                                            \
        utf ## X ## _char_t *string = (str);                                                                                        \
                                                                                                                                    \
        /* Check validity of original string, find length */                                                                        \
        bool is_valid = (utf ## X ## _validate(string, false) == 0);                                                                \
        size_t strlen_orig = strlen_utf ## X (string);                                                                              \
                                                                                                                                    \
        /* Calculate size in new encoding */                                                                                        \
        size_t length_utf ## Y = utf ## X ## _in_utf ## Y ## _len(string, false);                                                   \
                                                                                                                                    \
        printf("UTF-" #X " to UTF-" #Y " test. Valid string? %s\n", (is_valid ? "yes" : "no"));                                     \
        printf("UTF-" #X " length: %zu\n", strlen_orig);                                                                            \
        printf("UTF-" #Y " length: %zu\n", length_utf ## Y);                                                                        \
                                                                                                                                    \
        /* Allocate a buffer for conversion */                                                                                      \
        utf ## Y ## _char_t *buffer = calloc(length_utf ## Y + 1, sizeof(utf ## Y ## _char_t));                                     \
        size_t converted_length = length_utf ## Y;                                                                                  \
                                                                                                                                    \
        /* Allocate a buffer for reverse conversion */                                                                              \
        utf ## X ## _char_t *rev_buffer = calloc(strlen_orig + 1, sizeof(utf ## X ## _char_t));                                     \
                                                                                                                                    \
        /* Memory check */                                                                                                          \
        if (!buffer || !rev_buffer)                                                                                                 \
        {                                                                                                                           \
            perror("calloc");                                                                                                       \
            return;                                                                                                                 \
        }                                                                                                                           \
                                                                                                                                    \
        /* Ensure everything is null terminated */                                                                                  \
        buffer[length_utf ## Y] = 0;                                                                                                \
        rev_buffer[strlen_orig] = 0;                                                                                                \
                                                                                                                                    \
        /* Convert UTF-X to UTF-Y */                                                                                                \
        size_t converted = enc_utf ## X ## _to_utf ## Y (buffer, &converted_length, string, strlen_orig, false);                    \
                                                                                                                                    \
        printf("\nConversion result:\n");                                                                                           \
        printf("length: %zu, converted: %zu\n", strlen_orig, converted);                                                            \
        printf("buffer size: %zu, consumed: %zu, strlen: %zu\n", length_utf ## Y + 1, converted_length, strlen_utf ## Y (buffer));  \
        printf("Valid UTF-" #Y "? %s\n", (utf ## Y ## _validate(buffer, false) ? "no" : "yes"));                                    \
                                                                                                                                    \
        /* Save to file if requested */                                                                                             \
        if (path)                                                                                                                   \
        {                                                                                                                           \
            FILE *fp = fopen(path, "wb");                                                                                           \
            if (!fp) break; /* This leaks. Oh well... */                                                                            \
                                                                                                                                    \
            fwrite(buffer, converted_length, sizeof(utf ## Y ## _char_t), fp);                                                      \
            fclose(fp);                                                                                                             \
        }                                                                                                                           \
                                                                                                                                    \
        /* Setup reverse conversion */                                                                                              \
        converted_length = strlen_orig;                                                                                             \
                                                                                                                                    \
        /* Perform reverse conversion */                                                                                            \
        converted = enc_utf ## Y ## _to_utf ## X (rev_buffer, &converted_length, buffer, length_utf ## Y, false);                   \
                                                                                                                                    \
        printf("\nReverse result:\n");                                                                                              \
        printf("converted UTF-" #Y ": %zu, consumed UTF-" #X ": %zu\n", converted, converted_length);                               \
        printf("equal? %s\n\n", (memcmp(string, rev_buffer, strlen_orig * sizeof(utf ## X ## _char_t)) ? "no" : "yes"));            \
                                                                                                                                    \
        free(rev_buffer);                                                                                                           \
        free(buffer);                                                                                                               \
    } while (0)

void test_utf8(utf8_char_t *str, const char *path_16, const char *path_32)
{
    TEST_X_TO_Y(str, 8, 16, path_16);

    TEST_X_TO_Y(str, 8, 32, path_32);
}

int main(int argc, const char *const *argv)
{
    utf8_char_t *good_string_1 = (utf8_char_t *)"H¢llo, 試看看這個嘛, 😁。😁";
    utf8_char_t good_string_2[5] = {0x2F, 0x2E, 0x2E, 0x2F, 0x00};

    // This encodes two UTF-16 surrogates.
    utf8_char_t bad_string_1[7] = {0xED, 0xA1, 0x8C, 0xED, 0xBE, 0xB4, 0x00};

    // This is an encoding of 0 that's too long.
    utf8_char_t bad_string_2[3] = {0xC0, 0x80, 0x00};

    // This is an encoding of 0x2E that's too long.
    utf8_char_t bad_string_3[6] = {0x2F, 0xC0, 0xAE, 0x2E, 0x2F, 0x00};

    printf("--> Good string 1: '%s'\n", good_string_1);
    test_utf8(good_string_1, "good.1.16", "good.1.32");

    printf("--> Good string 2: '%s'\n", good_string_2);
    test_utf8(good_string_2, "good.2.16", "good.2.32");

    printf("--> Bad string 1: '%s'\n", bad_string_1);
    test_utf8(bad_string_1, "bad.1.16", "bad.1.32");

    printf("--> Bad string 2: '%s'\n", bad_string_2);
    test_utf8(bad_string_2, "bad.2.16", "bad.2.32");

    printf("--> Bad string 3: '%s'\n", bad_string_3);
    test_utf8(bad_string_3, "bad.3.16", "bad.3.32");

    return 0;
}
