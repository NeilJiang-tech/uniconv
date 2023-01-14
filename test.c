#include "unicode.h"

#include <stdlib.h>
#include <stdio.h>

void test_utf8(utf8_char_t *string)
{
    bool valid = (utf8_validate(string, false) == 0);
    size_t utf8_len = strlen_utf8(string);

    size_t utf16_len = utf8_in_utf16_len(string, false);
    size_t utf32_len = utf8_in_utf32_len(string, false);

    printf("String: '%s' (valid: %s)\n", string, (valid ? "yes" : "no"));
    printf("UTF-8 strlen: %zu\n", utf8_len);
    printf("Length in UTF-16: %zu\n", utf16_len);
    printf("Length in UTF-32: %zu\n\n", utf32_len);

    utf16_char_t *utf16_string = calloc(utf16_len, sizeof(utf16_char_t));
    utf32_char_t *utf32_string = calloc(utf32_len, sizeof(utf32_char_t));

    if (!utf16_string || !utf32_string)
    {
        perror("calloc");

        return;
    }

    size_t utf16_res_len = utf16_len;
    size_t utf32_res_len = utf32_len;

    size_t conv16 = enc_utf8_to_utf16(utf16_string, &utf16_res_len, string, utf8_len, false);
    size_t conv32 = enc_utf8_to_utf32(utf32_string, &utf32_res_len, string, utf8_len, false);

    printf("UTF-16 conversion result:\n");
    printf("length: %zu, converted: %zu\n", utf8_len, conv16);
    printf("buffer size: %zu, consumed: %zu, strlen: %zu\n", utf16_len, utf16_res_len, strlen_utf16(utf16_string));
    printf("valid UTF-16: %s\n\n", (!utf16_validate(utf16_string, false) ? "yes" : "no"));

    printf("UTF-32 conversion result:\n");
    printf("length: %zu, converted: %zu\n", utf8_len, conv32);
    printf("buffer size: %zu, consumed: %zu, strlen: %zu\n", utf32_len, utf32_res_len, strlen_utf32(utf32_string));
    printf("valid UTF-32: %s\n\n", (!utf32_validate(utf32_string, false) ? "yes" : "no"));

    FILE *f16 = fopen("utf16.txt", "wb");
    if (!f16) return;

    fwrite(utf16_string, sizeof(utf16_char_t), utf16_len, f16);
    fclose(f16);

    FILE *f32 = fopen("utf32.txt", "wb");
    if (!f32) return;

    fwrite(utf32_string, sizeof(utf32_char_t), utf32_len, f32);
    fclose(f32);
}

int main(int argc, const char *const *argv)
{
    test_utf8((utf8_char_t *)"H¢llo, 試看看這個嘛, 😁。😁");

    return 0;
}
