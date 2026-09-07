#ifndef __CLUTERM__UTIL_H__
#define __CLUTERM__UTIL_H__

#include <stdint.h>

#define STR(x)       #x
#define STRINGIFY(x) STR(x)
// clang-format off
#define THEME_FILE(name) STRINGIFY(themes/name.inc)
// clang-format on

#define EXPORT __attribute__((unused)) static

#define LENGTH(iterable)         (sizeof(iterable) / sizeof(iterable[0]))
#define SET(state, mask)         ((state) |= (mask))
#define UNSET(state, mask)       ((state) &= ~(mask))
#define IS_SET(state, mask)      (((state) & (mask)) == (mask))
#define IS_SET_ANY(state, mask)  (((state) & (mask)) != 0)
#define UPDATE(state, mask, set) (set ? SET(state, mask) : UNSET(state, mask))

#define MIN(x, y)        ((x) < (y) ? (x) : (y))
#define MAX(x, y)        ((x) > (y) ? (x) : (y))
#define CLAMP(x, l, r)   ((x) < (l) ? (l) : (x) > (r) ? (r) : (x))
#define BETWEEN(i, l, h) ((l) <= (i) && (i) <= (h))
#define SWAP(i, j)                                                             \
    do {                                                                       \
        __typeof__(i) tmp = i;                                                 \
        i = j, j = tmp;                                                        \
    } while (0)

typedef uint32_t Rgb;
typedef Rgb Palette[256];

typedef struct Theme {
    Palette palette;
    Rgb fg, bg;
} Theme;

typedef enum CursorStyle { CursorSolid, CursorBlink } CursorStyle;
typedef enum CursorShape {
    CursorBlock,
    CursorUnderline,
    CursorBar
} CursorShape;

enum FontType { FontRegular, FontBold, FontItalic, FontBoldItalic };
typedef struct Box {
    int top, right, bottom, left;
} Box;

typedef struct Config {
    const char *title;

    int rows, cols, history, tab_width;
    Box padding;

    const char *font_family;
    int font_size;

    Theme theme;

    Rgb cursor_color;
    CursorStyle cursor_style;
    CursorShape cursor_shape;
} Config;

#endif
