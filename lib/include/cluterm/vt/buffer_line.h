#ifndef __INCLUDE__CLUTERM__VT__BUFFER_LINE_H__
#define __INCLUDE__CLUTERM__VT__BUFFER_LINE_H__

#include "../colors.h"
#include "../utf8.h"
#include <stdbool.h>
#include <stdint.h>

typedef uint8_t CellState;
#define CELL_NORMAL    0
#define CELL_BOLD      (1 << 0)
#define CELL_ITALIC    (1 << 1)
#define CELL_UNDERLINE (1 << 2)
#define CELL_INVERSE   (1 << 3)

typedef struct CellAttributes {
    CellState state;
    Color fg, bg;
} CellAttributes;

typedef struct Cell {
    Rune value;
    CellAttributes attrs;
} Cell;

typedef uint8_t LineState;
#define LINE_WRAPPED (1 << 0)

typedef struct Line {
    Cell *cells;
    LineState state;
} *BufferLine;

void line_free(BufferLine *);

#endif
