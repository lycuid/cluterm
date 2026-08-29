#ifndef __CLUTERM__VT__CELL_H__
#define __CLUTERM__VT__CELL_H__

#include <cluterm/colors.h>
#include <cluterm/utf8.h>

typedef uint16_t CellState;
#define CELL_NORMAL    0
#define CELL_BOLD      (1 << 0)
#define CELL_ITALIC    (1 << 1)
#define CELL_UNDERLINE (1 << 2)
#define CELL_INVERSE   (1 << 3)

typedef struct CellAttributes {
    Color fg, bg;
    CellState state;
} CellAttributes;

typedef struct Cell {
    Rune value;
    CellAttributes attrs;
} Cell;

#define DEFAULT_CELL_ATTRS                                                     \
    (CellAttributes) { .fg = ColorFg(), .bg = ColorBg(), .state = CELL_NORMAL }
#define DEFAULT_CELL(val) CELL(val, DEFAULT_CELL_ATTRS)
#define CELL(val, _attrs)                                                      \
    (Cell) { .value = val, .attrs = _attrs }

typedef Cell *Line;

Rgb cell_fg(const Cell *);
Rgb cell_bg(const Cell *);

#endif
