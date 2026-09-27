#ifndef __INCLUDE__CLUTERM__COLORS_H__
#define __INCLUDE__CLUTERM__COLORS_H__

#include "scanner.h"
#include <stdbool.h>
#include <stdint.h>

#define UNPACK(c)                                                              \
    ((c) >> (8 * 2)) & 0xff, ((c) >> (8 * 1)) & 0xff, ((c) >> (8 * 0)) & 0xff

typedef uint32_t Rgb;
typedef Rgb Palette[256];

typedef enum ColorKind {
    ColorRGB,
    ColorPalette,
    ColorDefaultFg,
    ColorDefaultBg,
} ColorKind;

typedef struct Color {
    ColorKind kind;
    union {
        Rgb rgb;
        uint8_t index;
    } c;
} Color;

#define ColorRgb(val) ((Color){.kind = ColorRGB, .c.rgb = (val)})
#define ColorIdx(i)   ((Color){.kind = ColorPalette, .c.index = (i)})
#define ColorFg()     ((Color){.kind = ColorDefaultFg})
#define ColorBg()     ((Color){.kind = ColorDefaultBg})

typedef struct Theme {
    Palette palette;
    Rgb fg, bg, cursor;
} Theme;

Rgb resolve_color(const Color *, const Theme *);
bool parse_rgb(Scanner *s, Rgb *color);

#endif
