#include "utf8.h"
#include <stdbool.h>

#define MASK(ch, m)        (((uint8_t)(ch)) & ~utf8_mask[m])
#define BYTE(rune, pos, m) (utf8_byte[m] | MASK(rune >> (6 * (pos - 1)), m))

// clang-format off
static const uint8_t
    utf8_mask[UTF8_MAX_LEN + 1] = {
        /* 0b11000000, 0b10000000, 0b11100000, 0b11110000, 0b11111000 */
           0xc0,       0x80,       0xe0,       0xf0,       0xf8,
},
    utf8_byte[UTF8_MAX_LEN + 1] = {
        /* 0b10000000, 0b00000000, 0b11000000, 0b11100000, 0b11110000 */
           0x80,       0x0,        0xc0,       0xe0,       0xf0,
};
// clang-format on
static const uint32_t utf8_max[] = {0, 0x7f, 0x7ff, 0xffff, 0x10ffff};

// https://www.rfc-editor.org/rfc/rfc3629.html#section-4
static inline bool utf8_leading_validate(uchar byte, size_t i)
{
    switch (i) {
    // byte-2: 0xc2-0xdf
    case 2: return byte >= 0xc2;
    // byte-4: 0xf0-0xf4
    case 4: return byte <= 0xf4;
    }
    return true;
}

// https://www.rfc-editor.org/rfc/rfc3629.html#section-4
static inline bool utf8_continuation_validate(uchar leading, uchar input)
{
    switch (leading) {
    /*
     * UTF8-3 = %xE0 %xA0-BF UTF8-tail / %xE1-EC 2( UTF8-tail ) / %xED
     * %x80-9F UTF8-tail / %xEE-EF 2( UTF8-tail )
     */
    case 0xe0: return BETWEEN(input, 0xa0, 0xbf);
    case 0xed: return BETWEEN(input, 0x80, 0x9f);

    /*
     * UTF8-4 = %xF0 %x90-BF 2( UTF8-tail ) / %xF1-F3 3( UTF8-tail ) / %xF4
     * %x80-8F 2( UTF8-tail )
     */
    case 0xf0: return BETWEEN(input, 0x90, 0xbf);
    case 0xf4: return BETWEEN(input, 0x80, 0x8f);

    /* UTF8-tail = %x80-BF */
    default: return (input & utf8_mask[0]) == utf8_byte[0];
    }
}

static inline size_t utf8_len(uchar byte)
{
    for (size_t i = 1; i <= UTF8_MAX_LEN; ++i) {
        if ((byte & utf8_mask[i]) == utf8_byte[i]) {
            if (utf8_leading_validate(byte, i))
                return i;
            break;
        }
    }
    return 0;
}
#define utf8_error(status) (decoder->rune = UTF8_REPLACEMENT_CHAR, status)

UTF8DecodeResult utf8decoder_feed(UTF8_Decoder *decoder, uchar input)
{
    if (decoder->remaining == 0) {
        decoder->leading = input;
        if ((decoder->remaining = utf8_len(decoder->leading)) == 0)
            return utf8_error(UTF8_DECODE_RESULT_INVALID);
        decoder->rune = MASK(input, decoder->remaining);
    } else {
        if (!utf8_continuation_validate(decoder->leading, input))
            return utf8_error(UTF8_DECODE_RESULT_REJECT);
        decoder->rune    = (decoder->rune << 6) | MASK(input, 0);
        decoder->leading = 0;
    }
    return --decoder->remaining == 0 ? UTF8_DECODE_RESULT_SUCCESS
                                     : UTF8_DECODE_RESULT_ACCEPT;
}

Rune utf8_decode(const char *utf8_string)
{
    UTF8_Decoder decoder = {0};
    UTF8DecodeResult result;
    do {
        result = utf8decoder_feed(&decoder, *utf8_string++);
    } while (result == UTF8_DECODE_RESULT_ACCEPT);
    if (result != UTF8_DECODE_RESULT_SUCCESS)
        return UTF8_REPLACEMENT_CHAR;
    return decoder.rune;
}

size_t utf8_encode(Rune rune, UTF8_String str)
{
    size_t len = 0;
    while (rune > utf8_max[len])
        len++;
    if (len) {
        *str++ = BYTE(rune, len, len);
        for (int l = len; --l;)
            *str++ = BYTE(rune, l, 0);
    }
    return len;
}
