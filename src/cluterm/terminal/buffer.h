#ifndef __TERMINAL__BUFFER_H__
#define __TERMINAL__BUFFER_H__

#include <cluterm/terminal/colors.h>
#include <cluterm/utf8.h>
#include <cluterm/vte/parser.h>
#include <stdbool.h>

typedef uint16_t CellState;
#define CELL_NORMAL    0
#define CELL_BOLD      (1 << 0)
#define CELL_ITALIC    (1 << 1)
#define CELL_UNDERLINE (1 << 2)

typedef struct CellAttributes {
    Rgb fg, bg;
    CellState state;
} CellAttributes;

typedef struct Cell {
    Rune value;
    CellAttributes attrs;
} Cell;

#define DEFAULT_CELL_ATTRS                                                     \
    (CellAttributes) { .fg = DefaultFG, .bg = DefaultBG, .state = 0x0 }
#define DEFAULT_CELL(val) CELL(val, DEFAULT_CELL_ATTRS)
#define CELL(val, _attrs)                                                      \
    (Cell) { .value = val, .attrs = _attrs }
#define Color(rgb)                                                             \
    (SDL_Color)                                                                \
    {                                                                          \
        .r = ((rgb) >> (8 * 2)) & 0xff, .g = ((rgb) >> (8 * 1)) & 0xff,        \
        .b = ((rgb) >> (8 * 0)) & 0xff, .a = 0x0,                              \
    }

typedef Cell *Line;

typedef uint16_t CursorState;
#define CursorHide (1 << 0)

typedef struct Cursor {
    int y, x;
    Cell cell;
    CursorState state;
} Cursor;

typedef struct Region {
    int start, end;
} Region;

typedef enum Charset { CS_USASCII, CS_LINEGFX } Charset;

typedef struct TerminalBuffer {
    int rows, cols, history, last_row;
    Region scroll_region;
    Line *lines;
    Cursor cursor;
    CellAttributes cell_attrs;
    int charset[4], active_charset;
} TerminalBuffer;

#define buffer_lines(b)            ((b)->rows + (b)->history)
#define first_row(b)               (MAX(0, (b)->last_row - (b)->rows) % buffer_lines(b))
#define tline(b, y)                ((first_row(b) + y) % buffer_lines(b))
#define buffer_addcell(b, y, x, c) ((b)->lines[tline(b, y)][x] = (c))
#define buffer_clear(b)            buffer_addlines(b, ((b)->cursor.x = 0) + (b)->rows)
#define buffer_scrollup(b, count)                                              \
    buffer_scrollup_relative(b, (b)->scroll_region.start, count)
#define buffer_scrolldown(b, count)                                            \
    buffer_scrolldown_relative(b, (b)->scroll_region.start, count)

void buffer_init(TerminalBuffer *, int, int, int);
void buffer_create_texture(TerminalBuffer *);
void buffer_destroy(TerminalBuffer *);

// clear line at 'y' from 'x0' to 'x1'.
void buffer_clearline(TerminalBuffer *, int, int, int);
// clear box from '(y0, x0)' to '(y1, x1)'.
void buffer_clearbox(TerminalBuffer *, int, int, int, int);
// adds 'n' lines.
void buffer_addlines(TerminalBuffer *, int);
void buffer_scrollup_relative(TerminalBuffer *b, int, int);
void buffer_scrolldown_relative(TerminalBuffer *b, int, int);
// insert 'n' chars after cursor.
void buffer_insert_chars(TerminalBuffer *, int);
// delete 'n' chars after cursor.
void buffer_delete_chars(TerminalBuffer *, int);
void buffer_resize(TerminalBuffer *, int, int);

#endif
