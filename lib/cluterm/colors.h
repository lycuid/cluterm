#ifndef __CLUTERM__COLORS_H__
#define __CLUTERM__COLORS_H__

#include <cluterm/scanner.h>
#include <cluterm/util.h>

#define RGB(r, g, b) (((r) << (8 * 2)) | ((g) << (8 * 1)) | ((b) << (8 * 0)))

#define UNPACK(c)                                                              \
    ((c) >> (8 * 2)) & 0xff, ((c) >> (8 * 1)) & 0xff, ((c) >> (8 * 0)) & 0xff

#define IS_HEX(ch)                                                             \
    (BETWEEN(ch, '0', '9') || BETWEEN(ch, 'a', 'f') || BETWEEN(ch, 'A', 'F'))

static const int hex[] = {
    [0] = 0,    [1] = 1,    [2] = 2,    [3] = 3,    [4] = 4,    [5] = 5,
    [6] = 6,    [7] = 7,    [8] = 8,    [9] = 9,    ['a'] = 10, ['b'] = 11,
    ['c'] = 12, ['d'] = 13, ['e'] = 14, ['f'] = 15, ['A'] = 10, ['B'] = 11,
    ['C'] = 12, ['D'] = 13, ['E'] = 14, ['F'] = 15,
};

static inline uint32_t parse_rgb(Scanner *s, Rgb *color)
{
    if (s_consume(s, '#') && s_buflen(s) >= 6) {
        Rgb rgb = 0;
        for (int i = 0; i < 3; ++i) {
            if (!IS_HEX(*s_peek(s)))
                return 0;
            rgb |= hex[s_next(s)] << (20 - i * 8);

            if (!IS_HEX(*s_peek(s)))
                return 0;
            rgb |= hex[s_next(s)] << (16 - i * 8);
        }
        *color = rgb;
        return 1;
    }
    return 0;
}

#endif
