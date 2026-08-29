#ifndef __CLUTERM__COLORS_H__
#define __CLUTERM__COLORS_H__

#include <cluterm/scanner.h>
#include <cluterm/util.h>

#define RGB(r, g, b) (((r) << (8 * 2)) | ((g) << (8 * 1)) | ((b) << (8 * 0)))

#define UNPACK(c)                                                              \
    ((c) >> (8 * 2)) & 0xff, ((c) >> (8 * 1)) & 0xff, ((c) >> (8 * 0)) & 0xff

#define IS_HEX(ch)                                                             \
    (BETWEEN(ch, '0', '9') || BETWEEN(ch, 'a', 'f') || BETWEEN(ch, 'A', 'F'))

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

static const int hex[] = {
    ['0'] = 0,  ['1'] = 1,  ['2'] = 2,  ['3'] = 3,  ['4'] = 4,  ['5'] = 5,
    ['6'] = 6,  ['7'] = 7,  ['8'] = 8,  ['9'] = 9,  ['a'] = 10, ['b'] = 11,
    ['c'] = 12, ['d'] = 13, ['e'] = 14, ['f'] = 15, ['A'] = 10, ['B'] = 11,
    ['C'] = 12, ['D'] = 13, ['E'] = 14, ['F'] = 15,
};

Rgb resolve_color(const Color *, const Theme *);
bool parse_rgb(Scanner *s, Rgb *color);

#endif
