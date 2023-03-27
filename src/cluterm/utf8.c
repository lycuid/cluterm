#include "utf8.h"
#include <cluterm/utils.h>
#include <string.h>

#define MASK(ch, m)        (((uint8_t)(ch)) & ~utf8_mask[m])
#define BYTE(rune, pos, m) (utf8_byte[m] | MASK(rune >> (6 * (pos - 1)), m))

// clang-format off
static const uint8_t
    utf8_mask[UTF8_MAX_LEN + 1] = {
        /*  0b11000000, 0b10000000, 0b11100000, 0b11110000, 0b11111000 */
            0xc0,       0x80,       0xe0,       0xf0,       0xf8},
    utf8_byte[UTF8_MAX_LEN + 1] = {
        /*  0b10000000, 0b0,        0b11000000, 0b11100000, 0b11110000 */
            0x80,       0x0,        0xc0,       0xe0,       0xf0};
// clang-format on

static const uint32_t utf8_max[] = {0, 0x7f, 0x7ff, 0xffff, 0x10ffff};

static inline uint32_t utf8_len(char ch)
{
    for (uint32_t i = 1; i <= UTF8_MAX_LEN; ++i)
        if ((ch & utf8_mask[i]) == utf8_byte[i])
            return i;
    return 0;
}

uint32_t utf8_decode(const char *s, uint32_t slen, Rune *rune)
{
    uint32_t len = s ? utf8_len(*s) : 0, i = 1;
    if (len *= BETWEEN(len, 1, slen))
        for (*rune = MASK(s[0], len); i < len; i++)
            *rune = (*rune << 6) | MASK(s[i], 0);
    return len;
}

void utf8_encode(Rune rune, UTF8_String str)
{
    int len = 0, i = 0;
    while (rune > utf8_max[len])
        len++;
    if (len)
        for (str[i++] = BYTE(rune, len, len); --len;)
            str[i++] = BYTE(rune, len, 0);
}
