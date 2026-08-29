#include "colors.h"
#include <config.h>

Rgb resolve_color(const Color *color, const Theme *theme)
{
    switch (color->kind) {
    case ColorRGB: return color->c.rgb;
    case ColorPalette: return theme->palette[color->c.index];
    case ColorDefaultFg: return theme->fg;
    case ColorDefaultBg: return theme->bg;
    }
    return color->c.rgb;
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
