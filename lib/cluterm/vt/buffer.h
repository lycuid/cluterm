#ifndef __CLUTERM__VT__BUFFER_H__
#define __CLUTERM__VT__BUFFER_H__

#include <cluterm/debug.h>
#include <cluterm/utf8.h>
#include <cluterm/vt/parser.h>
#include <stdbool.h>

typedef uint16_t CellState;
#define CELL_NORMAL    0
#define CELL_BOLD      (1 << 0)
#define CELL_ITALIC    (1 << 1)
#define CELL_UNDERLINE (1 << 2)

#define MEMBER_COLORS Rgb fg, bg

typedef struct CellAttributes {
    MEMBER_COLORS;
    CellState state;
} CellAttributes;

typedef struct Cell {
    Rune value;
    CellAttributes attrs;
} Cell;

#define DEFAULT_CELL_ATTRS                                                     \
    (CellAttributes){.fg = cfg->fg, .bg = cfg->bg, .state = 0x0}
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

typedef enum CursorStyle { CursorSolid, CursorBlink } CursorStyle;
typedef enum CursorShape {
    CursorBlock,
    CursorUnderline,
    CursorBar
} CursorShape;

typedef struct Cursor {
    int y, x;
    Rgb color;
    bool visible : 1;
    CursorStyle style;
    CursorShape shape;
} Cursor;

typedef struct Region {
    int start, end;
} Region;

typedef enum Charset { CS_USASCII, CS_LINEGFX } Charset;

#define MEMBERS_FRAME_BUFFER                                                   \
    int rows, cols;                                                            \
    Line *lines;                                                               \
    bool *dirty;                                                               \
    Cursor cursor

typedef struct ClutermBuffer {
    MEMBERS_FRAME_BUFFER;

    int history, last_row;
    bool *tab;
    Cursor saved_cursor;
    Region scroll_region;
    CellAttributes cell_attrs;
    int charset[4], active_charset;
} ClutermBuffer;

#define first_row(b)  (MAX(0, (b)->last_row - (b)->rows) % lines(b))
#define lines(b)      ((b)->rows + (b)->history)
#define line_at(b, y) ((b)->lines[(first_row(b) + (y)) % lines(b)])

#define clear(b)             addlines(b, ((b)->cursor.x = 0) + (b)->rows)
#define scrollup(b, count)   scrollup_rel(b, (b)->scroll_region.start, count)
#define scrolldown(b, count) scrolldown_rel(b, (b)->scroll_region.start, count)

#define dirty_buffer(b)                                                        \
    memset((b)->dirty, 1, (b)->rows *(b)->cols * sizeof(*(b)->dirty))

void buffer_init(ClutermBuffer *, int, int, int);
void buffer_destroy(ClutermBuffer *);
void buffer_resize(ClutermBuffer *, int, int);

Cell getcell(const ClutermBuffer *, int, int);
void putcell(ClutermBuffer *, int, int, Cell);
// clear line at 'y' from 'x0' to 'x1'.
void clearline(ClutermBuffer *, int, int, int);
// clear box from '(y0, x0)' to '(y1, x1)'.
void clearbox(ClutermBuffer *, int, int, int, int);
// adds 'n' lines.
void addlines(ClutermBuffer *, int);
void scrollup_rel(ClutermBuffer *, int, int);
void scrolldown_rel(ClutermBuffer *, int, int);

/* cursor actions. */
// All the below actions involve either updating cursor or operations 'relative'
// to the cursor position.

// absolute cursor move (screen coords clamped).
void move_cursor_to(ClutermBuffer *, int, int);
// relative cursor move.
void move_cursor(ClutermBuffer *, int, int);
// insert 'n' chars after cursor.
void insert_chars(ClutermBuffer *, int);
// delete 'n' chars after cursor.
void delete_chars(ClutermBuffer *, int);
// inserts tab at cursor.
void insert_tab(ClutermBuffer *, int, int);
// insert cell at the current cursor position (with word wrap).
void insert_cell(ClutermBuffer *, Cell);
// move cursor to (y+1, 0)
void linefeed(ClutermBuffer *);
// store cursor coordinates.
void save_cursor(ClutermBuffer *);
// restore the stored cursor coordinates.
void restore_cursor(ClutermBuffer *);

#endif
