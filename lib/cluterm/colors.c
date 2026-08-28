#include "colors.h"
#include <cluterm/config.h>
#include <config.h>

Rgb color256(uint8_t n)
{
    static const int color256_mask[] = {0x00, 0x5f, 0x87, 0xaf, 0xd7, 0xff};

    Rgb color = 0;
    if (n <= 15)
        color = DefaultTheme.palette[n];
    else if (BETWEEN(n, 16, 231))
        for (int i = 0, m = n - 16; m; m /= 6)
            color |= color256_mask[m % 6] << (8 * i++);
    else if (n >= 232)
        n = (n - 232) * 10 + 8, color = (n << 16) | (n << 8) | n;
    return color;
}

Rgb resolve_color(const Color *const color)
{
    return color->is_rgb ? color->c.rgb : cfg->theme.palette[color->c.index];
}

bool parse_rgb(Scanner *s, Rgb *color)
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
