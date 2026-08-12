// clang-format off
/*
 *            +-----------------------------------------------------------+
 *        ^   |                                                           |
 *            |                                                           |
 *  (history) |                                                           |
 *            |                                                           |
 *        v   |                                                           |
 *            +-----------------------------------------------------------+
 *        ^   | (0, 0)                                                    |
 *        |   |                                                           |
 *        |   |                                                           |
 *            |                                                           |
 *     (rows) |                                                           |
 *            |                                                           |
 *        |   |                                                           |
 *        |   |                                                           |
 *        v   |                   (last_row)              (row-1, cols-1) |
 *            +-----------------------------------------------------------+
 *
 *            <-------------------------  (cols)  ------------------------->
 * */
// clang-format on

#include "buffer.h"
#include <cluterm/debug.h>
#include <cluterm/utf8.h>
#include <config.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define first_row(b)  (MAX(0, (b)->last_row - (b)->rows) % lines(b))
#define line_at(b, y) ((b)->lines[(first_row(b) + (y)) % lines(b)])
#define lines(b)      ((b)->rows + (b)->history)

// making sure b->last_row => [b->rows, 2*b->rows), once it exceeds b->rows.
#define adjust(b)                                                              \
    b->last_row =                                                              \
        (((b)->last_row >= lines(b)) * lines(b)) + ((b)->last_row % lines(b));

#define dirty_cell(b, y, x) (b)->dirty[(y) * (b)->cols + (x)] = 1;

#define dirty_lines(b, y, count)                                               \
    memset(&(b)->dirty[(y) * (b)->cols], 1,                                    \
           (count) * (b)->cols * sizeof(*(b)->dirty));

#define dirty_line(b, y) dirty_lines(b, y, 1)

void buffer_init(CluTermBuffer *b, int rows, int cols, int history)
{
    b->rows = rows, b->cols = cols, b->history = history, b->last_row = 0;
    b->scroll_region.start = 0, b->scroll_region.end = b->rows - 1;
    b->tab = calloc(b->cols + 1, sizeof(bool));
    for (int i = TabWidth; i <= b->cols; i += TabWidth)
        b->tab[i] = 1;

    /* Cursor. */ {
        b->cursor.y = b->cursor.x = b->cursor.state = 0;

        b->cursor.cell.attrs = DEFAULT_CELL_ATTRS;

        /* b->cursor.cell.value = utf8_decode("▇"); */
        /* b->cursor.cell.value = utf8_decode("_"); */
        b->cursor.cell.value = utf8_decode("|");
        b->saved_cursor      = b->cursor;
    }

    /* Charset */ {
        memset(b->charset, CS_USASCII, sizeof(b->charset));
        b->active_charset = 0;
    }

    b->cell_attrs = DEFAULT_CELL_ATTRS;
    b->lines      = malloc(lines(b) * sizeof(Line));
    for (int y = 0; y < lines(b); ++y) {
        b->lines[y] = malloc(b->cols * sizeof(Cell));
        for (int x = 0; x < b->cols; ++x)
            b->lines[y][x] = DEFAULT_CELL(' ');
    }
    b->dirty = malloc(rows * cols * sizeof(bool));
    clear(b);
}

void buffer_resize(CluTermBuffer *b, int rows, int cols)
{
    debug_1("buffer resized to %dx%d.\n", cols, rows);
    Line *ll = malloc(rows * sizeof(Line));
    for (int y = 0; y < rows; ++y) {
        ll[y] = malloc(cols * sizeof(Cell));
        for (int x = 0; x < cols; ++x)
            ll[y][x] = DEFAULT_CELL(' ');
    }
    for (int y = 0; y < MIN(rows, b->rows); ++y)
        memmove(ll[y], line_at(b, y), MIN(cols, b->cols) * sizeof(Cell));
    for (int y = 0; y < lines(b); ++y)
        free(b->lines[y]);
    free(b->lines);

    b->rows = rows, b->cols = cols, b->lines = ll;
    b->last_row      = rows;
    b->scroll_region = (Region){0, rows - 1};
    b->cursor.y      = MIN(b->cursor.y, rows - 1);
    b->cursor.x      = MIN(b->cursor.x, cols - 1);

    b->tab = realloc(b->tab, (b->cols + 1) * sizeof(*b->tab));
    memset(b->tab, 0, (b->cols + 1) * sizeof(*b->tab));
    for (int i = TabWidth; i <= b->cols; i += TabWidth)
        b->tab[i] = 1;
    b->dirty = realloc(b->dirty, b->rows * b->cols * sizeof(*b->dirty));
    dirty_buffer(b);
    adjust(b);
}

void buffer_destroy(CluTermBuffer *b)
{
    if (b->lines) {
        for (int y = 0; y < lines(b); ++y)
            free(b->lines[y]);
        free(b->lines);
    }
    if (b->tab)
        free(b->tab);
    if (b->dirty)
        free(b->dirty);
    debug_1("buffer cleanup: Done!.\n");
}

Cell getcell(const CluTermBuffer *b, int y, int x) { return line_at(b, y)[x]; }

void putcell(CluTermBuffer *b, int y, int x, Cell c)
{
    line_at(b, y)[x] = c;
    dirty_cell(b, y, x);
}

void clearline(CluTermBuffer *b, int y, int x0, int x1)
{
    for (; x0 <= x1; ++x0)
        putcell(b, y, x0, CELL(' ', b->cell_attrs));
}

void clearbox(CluTermBuffer *b, int y0, int x0, int y1, int x1)
{
    for (; y0 <= y1; ++y0)
        clearline(b, y0, x0, x1);
}

void addlines(CluTermBuffer *b, int lines)
{
    b->last_row += lines;
    clearbox(b, b->rows - lines, 0, b->rows - 1, b->cols - 1);
    adjust(b);
}

void scrollup_rel(CluTermBuffer *b, int origin, int lines)
{
    Region *region = &b->scroll_region;
    if (!lines || origin >= region->end)
        return;

    lines = MIN(lines, region->end - MAX(region->start, origin) + 1);

    for (int y = origin + lines; y <= region->end; ++y)
        SWAP(line_at(b, y), line_at(b, y - lines));
    clearbox(b, region->end - lines + 1, 0, region->end, b->cols - 1);

    dirty_lines(b, origin, region->end - origin + 1);
}

void scrolldown_rel(CluTermBuffer *b, int origin, int lines)
{
    Region *region = &b->scroll_region;
    if (!lines || origin >= region->end)
        return;

    lines = MIN(lines, region->end - MAX(region->start, origin) + 1);

    for (int y = region->end - lines; y >= origin; --y)
        SWAP(line_at(b, y), line_at(b, y + lines));
    clearbox(b, origin, 0, origin + lines - 1, b->cols - 1);

    dirty_lines(b, origin, region->end - origin + 1);
}

#define dirty_cursor(b)                                                        \
    do {                                                                       \
        if (BETWEEN((b)->cursor.y, 0, (b)->rows - 1) &&                        \
            BETWEEN((b)->cursor.x, 0, (b)->cols - 1))                          \
            dirty_cell(b, (b)->cursor.y, (b)->cursor.x);                       \
    } while (0)

void move_cursor_to(CluTermBuffer *b, int y, int x)
{
    dirty_cursor(b);
    b->cursor.y = CLAMP(y, 0, b->rows - 1);
    b->cursor.x = CLAMP(x, 0, b->cols);
    dirty_cursor(b);
}
#undef dirty_cursor

void move_cursor(CluTermBuffer *b, int dy, int dx)
{
    move_cursor_to(b, b->cursor.y + dy, b->cursor.x + dx);
}

static inline void insert_delete_chars(CluTermBuffer *b, int count, bool insert)
{
    dirty_line(b, b->cursor.y);
    Cell *xptr = line_at(b, b->cursor.y) + b->cursor.x;
    int dx     = MIN(b->cols - 1 - b->cursor.x, count),
        shift  = b->cols - 1 - b->cursor.x - dx;

    insert ? memmove(xptr + dx, xptr, shift * sizeof(Cell))
           : memmove(xptr, xptr + dx, shift * sizeof(Cell));

    xptr += shift * !insert;
    for (int i = 0; i < dx; ++i)
        *(xptr + i) = CELL(' ', b->cell_attrs);
}

void insert_chars(CluTermBuffer *b, int count)
{
    insert_delete_chars(b, count, true);
}

void delete_chars(CluTermBuffer *b, int count)
{
    insert_delete_chars(b, count, false);
}

void insert_tab(CluTermBuffer *b, int count, int inc)
{
    dirty_line(b, b->cursor.y);
    while (BETWEEN(b->cursor.x, 0, b->cols - 1) && count--) {
        do {
            b->cursor.x += inc;
        } while (BETWEEN(b->cursor.x, 0, b->cols - 1) && !b->tab[b->cursor.x]);
    }
    b->cursor.x = CLAMP(b->cursor.x, 0, b->cols);
}

static inline Cell translate(Cell cell, Charset charset)
{
    switch (charset) {
    case CS_USASCII: break;
    case CS_LINEGFX: {
        // This table is proudly stolen from st, which was proudly stolen from
        // rxvt.
        static const char *const vt100_0[/* 0x41..0x7e */] = {
            "↑", "↓", "→", "←", "█", "▚", "☃",      // A - G
            0,   0,   0,   0,   0,   0,   0,   0,   // H - O
            0,   0,   0,   0,   0,   0,   0,   0,   // P - W
            0,   0,   0,   0,   0,   0,   0,   " ", // X - _
            "◆", "▒", "␉", "␌", "␍", "␊", "°", "±", // ` - g
            "␤", "␋", "┘", "┐", "┌", "└", "┼", "⎺", // h - o
            "⎻", "─", "⎼", "⎽", "├", "┤", "┴", "┬", // p - w
            "│", "≤", "≥", "π", "≠", "£", "·",      // x - ~
        };
        if (BETWEEN(cell.value, 0x41, 0x7e) && vt100_0[cell.value - 0x41])
            cell.value = utf8_decode(vt100_0[cell.value - 0x41]);
    } break;
    }
    return cell;
}

void insert_cell(CluTermBuffer *b, Cell cell)
{
    if (b->cursor.x == b->cols) {
        if (b->cursor.y == b->rows - 1)
            scrollup(b, 1);
        move_cursor_to(b, b->cursor.y + 1, 0);
    }
    putcell(b, b->cursor.y, b->cursor.x,
            translate(cell, b->charset[b->active_charset]));
    move_cursor(b, 0, 1);
}

void linefeed(CluTermBuffer *b)
{
    b->cursor.y == b->scroll_region.end ? scrollup(b, 1) : move_cursor(b, 1, 0);
}

void save_cursor(CluTermBuffer *b) { b->saved_cursor = b->cursor; }
void restore_cursor(CluTermBuffer *b) { b->cursor = b->saved_cursor; }
