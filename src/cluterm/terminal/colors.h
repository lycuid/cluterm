#ifndef __TERMINAL__COLORS_H__
#define __TERMINAL__COLORS_H__

#include <cluterm/utils.h>
#include <stdint.h>

#define RGB(r, g, b) ((r) << (8 * 2)) | ((g) << (8 * 1)) | ((b) << (8 * 0))

static const int color256_mask[] = {0x00, 0x5f, 0x87, 0xaf, 0xd7, 0xff};
static const Rgb color16[]       = {
#if defined(COLORS_VGA)
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
#elif defined(COLORS_TERMINAL_APP)
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
#elif defined(COLORS_VSCODE)
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

__attribute__((unused)) static inline Rgb color256(uint8_t n)
{
    Rgb color = 0;
    if (n <= 15)
        color = color16[n];
    else if (BETWEEN(n, 16, 231))
        for (int i = 0, m = n - 16; m; m /= 6)
            color |= color256_mask[m % 6] << (8 * i++);
    else if (n >= 232)
        n = 8 + (n - 232) * 10, color = (n << 16) | (n << 8) | n;
    return color;
}

#endif
