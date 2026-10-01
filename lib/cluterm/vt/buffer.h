#ifndef __CLUTERM__VT__BUFFER_H__
#define __CLUTERM__VT__BUFFER_H__

#include <cluterm/colors.h>
#include <cluterm/utf8.h>
#include <cluterm/vt/buffer_line.h>
#include <cluterm/vt/parser.h>
#include <stdbool.h>

typedef struct Region {
    int start, end;
} Region;

typedef enum Charset { CS_USASCII, CS_LINEGFX } Charset;

typedef struct ClutermBuffer {
    MEMBERS_FRAME_BUFFER

    int history, last_row, scroll_offset;
    bool *tab;
    Cursor saved_cursor;
    Region scroll_region;
    CellAttributes cell_attrs;
    int charset[4], active_charset;
} ClutermBuffer;

#define clear(b)             addlines(b, ((b)->cursor.x = 0) + (b)->rows)
#define scrollup(b, count)   scrollup_rel(b, count, (b)->scroll_region.start)
#define scrolldown(b, count) scrolldown_rel(b, count, (b)->scroll_region.start)

#define line_cnt(b) ((b)->rows + (b)->history)
#define viewport_row(b, y)                                                     \
    MAX(0, b->last_row - b->rows - b->scroll_offset + y) % line_cnt(b)

#define dirty_buffer(b)                                                        \
    memset((b)->dirty, 1, (b)->rows *(b)->cols * sizeof(*(b)->dirty))

#define dirty_lines(b, y, count)                                               \
    memset(&(b)->dirty[(y) * (b)->cols], 1,                                    \
           (count) * (b)->cols * sizeof(*(b)->dirty));

#define dirty_line(b, y) dirty_lines(b, y, 1)

void buffer_snapshot(const ClutermBuffer *, BufferLine *);

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
