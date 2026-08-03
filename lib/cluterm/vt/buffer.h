#ifndef __CLUTERM__VT__BUFFER_H__
#define __CLUTERM__VT__BUFFER_H__

#include <cluterm/debug.h>
#include <cluterm/utf8.h>
#include <cluterm/vt/palette.h>
#include <cluterm/vt/parser.h>
#include <stdbool.h>

/* used by macro(dirty_line). */
#include <string.h> // IWYU pragma: keep

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
    (CellAttributes){.fg = DefaultFG, .bg = DefaultBG, .state = 0x0}
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

typedef struct CluTermBuffer {
    int rows, cols, history, last_row;
    Line *lines;
    bool *tab, *dirty;
    Cursor cursor, saved_cursor;
    Region scroll_region;
    CellAttributes cell_attrs;
    int charset[4], active_charset;
} CluTermBuffer;

#define first_row(b)  (MAX(0, (b)->last_row - (b)->rows) % lines(b))
#define line_at(b, y) ((b)->lines[(first_row(b) + y) % lines(b)])
#define lines(b)      ((b)->rows + (b)->history)

#define clear(b)             addlines(b, ((b)->cursor.x = 0) + (b)->rows)
#define scrollup(b, count)   scrollup_rel(b, (b)->scroll_region.start, count)
#define scrolldown(b, count) scrolldown_rel(b, (b)->scroll_region.start, count)

#define dirty_cell(b, y, x)                                                    \
    do {                                                                       \
        int i = y * (b)->cols + x, size = (b)->rows * (b)->cols;               \
        if (i < size)                                                          \
            (b)->dirty[i] = true;                                              \
        else                                                                   \
            debug_1("oob %d -> %d (%dx%d)\n", i, size, (b)->cols, (b)->rows);  \
    } while (0)
#define dirty_line(b, y)                                                       \
    memset(&(b)->dirty[y * (b)->cols], 1, (b)->cols * sizeof(*b->dirty));
#define dirty_buffer(b)                                                        \
    memset((b)->dirty, 1, (b)->rows *(b)->cols * sizeof(*(b)->dirty))

static inline void putcell(CluTermBuffer *b, int y, int x, Cell c)
{
    line_at(b, y)[x] = c;
    dirty_cell(b, y, x);
}

void buffer_init(CluTermBuffer *, int, int, int);
void buffer_destroy(CluTermBuffer *);
void buffer_resize(CluTermBuffer *, int, int);

// clear line at 'y' from 'x0' to 'x1'.
void clearline(CluTermBuffer *, int, int, int);
// clear box from '(y0, x0)' to '(y1, x1)'.
void clearbox(CluTermBuffer *, int, int, int, int);
// adds 'n' lines.
void addlines(CluTermBuffer *, int);
void scrollup_rel(CluTermBuffer *, int, int);
void scrolldown_rel(CluTermBuffer *, int, int);

/* cursor actions. */
// All the below actions involve either updating cursor or operations relative
// to the cursor.

// absolute cursor move (screen coords clamped).
void move_cursor_to(CluTermBuffer *, int, int);
// relative cursor move.
void move_cursor(CluTermBuffer *, int, int);
// insert 'n' chars after cursor.
void insert_chars(CluTermBuffer *, int);
// delete 'n' chars after cursor.
void delete_chars(CluTermBuffer *, int);
// inserts tab at cursor.
void insert_tab(CluTermBuffer *, int, int);
// insert cell at the current cursor position (with word wrap).
void insert_cell(CluTermBuffer *, Cell);
// move cursor to (y+1, 0)
void linefeed(CluTermBuffer *);
// store cursor coordinates.
void save_cursor(CluTermBuffer *);
// restore the stored cursor coordinates.
void restore_cursor(CluTermBuffer *);

#endif
