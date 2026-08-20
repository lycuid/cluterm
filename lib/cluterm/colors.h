#ifndef __CLUTERM__COLORS_H__
#define __CLUTERM__COLORS_H__

#include <cluterm/scanner.h>
#include <cluterm/util.h>

#define RGB(r, g, b) ((r) << (8 * 2)) | ((g) << (8 * 1)) | ((b) << (8 * 0))

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

static const Rgb color16[] = {
#if defined(PALETTE_VGA)
    [0] = RGB(0, 0, 0),
    [1] = RGB(170, 0, 0),
    [2] = RGB(0, 170, 0),
    [3] = RGB(170, 85, 0),
    [4] = RGB(0, 0, 170),
    [5] = RGB(170, 0, 170),
    [6] = RGB(0, 170, 170),
    [7] = RGB(170, 170, 170),

    [8]  = RGB(85, 85, 85),
    [9]  = RGB(255, 85, 85),
    [10] = RGB(85, 255, 85),
    [11] = RGB(255, 255, 85),
    [12] = RGB(85, 85, 255),
    [13] = RGB(255, 85, 255),
    [14] = RGB(85, 255, 255),
    [15] = RGB(255, 255, 255)
#elif defined(PALETTE_TERMINAL_APP)
    [0] = RGB(0, 0, 0),
    [1] = RGB(194, 54, 33),
    [2] = RGB(37, 188, 36),
    [3] = RGB(173, 173, 39),
    [4] = RGB(73, 46, 225),
    [5] = RGB(211, 56, 211),
    [6] = RGB(51, 187, 200),
    [7] = RGB(203, 204, 205),

    [8]  = RGB(129, 131, 131),
    [9]  = RGB(252, 57, 31),
    [10] = RGB(49, 231, 34),
    [11] = RGB(234, 236, 35),
    [12] = RGB(88, 51, 255),
    [13] = RGB(249, 53, 248),
    [14] = RGB(20, 240, 240),
    [15] = RGB(235, 235, 235)
#elif defined(PALETTE_VSCODE)
    [0] = RGB(0, 0, 0),
    [1] = RGB(205, 49, 49),
    [2] = RGB(13, 188, 121),
    [3] = RGB(229, 229, 16),
    [4] = RGB(36, 114, 200),
    [5] = RGB(188, 63, 188),
    [6] = RGB(17, 168, 205),
    [7] = RGB(229, 229, 229),

    [8]  = RGB(102, 102, 102),
    [9]  = RGB(241, 76, 76),
    [10] = RGB(35, 209, 139),
    [11] = RGB(245, 245, 67),
    [12] = RGB(59, 142, 234),
    [13] = RGB(214, 112, 214),
    [14] = RGB(41, 184, 219),
    [15] = RGB(229, 229, 229)
#else
    [0] = 0x000000,  [1] = 0xee0000,  [2] = 0x00ee00,
    [3] = 0xeedd00,  [4] = 0x0000ee,  [5] = 0xee00ee,
    [6] = 0x00eeee,  [7] = 0xeeeeee,

    [8] = 0xdddddd,  [9] = 0xffdddd,  [10] = 0xddffdd,
    [11] = 0xffffdd, [12] = 0xddddff, [13] = 0xffddff,
    [14] = 0xddffff, [15] = 0xffffff
#endif
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
