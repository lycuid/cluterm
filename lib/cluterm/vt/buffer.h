#ifndef __CLUTERM__VT__BUFFER_H__
#define __CLUTERM__VT__BUFFER_H__

#include <cluterm/colors.h>
#include <cluterm/utf8.h>
#include <cluterm/vt/parser.h>
#include <stdbool.h>

#define DEFAULT_CELL_ATTRS                                                     \
    (CellAttributes) { .fg = ColorFg(), .bg = ColorBg(), .state = CELL_NORMAL }
#define DEFAULT_CELL(val) CELL(val, DEFAULT_CELL_ATTRS)
#define CELL(val, _attrs)                                                      \
    (Cell) { .value = val, .attrs = _attrs }

typedef struct Region {
    int start, end;
} Region;

typedef enum Charset { CS_USASCII, CS_LINEGFX } Charset;

typedef struct ClutermBuffer {
    MEMBERS_FRAME_BUFFER

    int last_row;
    bool *tab;
    Cursor saved_cursor;
    Region scroll_region;
    CellAttributes cell_attrs;
    int charset[4], active_charset;
} ClutermBuffer;

#define clear(b)             addlines(b, ((b)->cursor.x = 0) + (b)->rows)
#define scrollup(b, count)   scrollup_rel(b, count, (b)->scroll_region.start)
#define scrolldown(b, count) scrolldown_rel(b, count, (b)->scroll_region.start)

#define dirty_buffer(b)                                                        \
    memset((b)->dirty, 1, (b)->rows *(b)->cols * sizeof(*(b)->dirty))

int buffer_histsize(const ClutermBuffer *);
void buffer_snapshot(const ClutermBuffer *, Line *, int);

void buffer_init(ClutermBuffer *, int, const Config *);
void buffer_destroy(ClutermBuffer *);
void buffer_resize(ClutermBuffer *, const Config *, int, int);

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
