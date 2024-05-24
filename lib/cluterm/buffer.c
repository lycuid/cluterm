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
#include <cluterm/utf8.h>
#include <config.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define adjust(b)                                                              \
    ((b)->last_row = (((b)->last_row >= buffer_lines(b)) * buffer_lines(b)) +  \
                     ((b)->last_row % buffer_lines(b)))

/* static const Cell DebugCell = DEFAULT_CELL('*'); */

void buffer_init(CluTermBuffer *b, int rows, int cols, int history)
{
    b->rows = rows, b->cols = cols, b->history = history, b->last_row = 0;
    b->scroll_region.start = 0, b->scroll_region.end = b->rows - 1;

    /* Cursor. */ {
        b->cursor.y = b->cursor.x = b->cursor.state = 0;

        b->cursor.cell.attrs = DEFAULT_CELL_ATTRS;

        /* b->cursor.cell.value = utf8_decode("▇"); */
        /* b->cursor.cell.value = utf8_decode("_"); */
        b->cursor.cell.value = utf8_decode("|");
    }

    /* Charset */ {
        memset(b->charset, CS_USASCII, sizeof(b->charset));
        b->active_charset = 0;
    }

    b->cell_attrs = DEFAULT_CELL_ATTRS;
    b->lines      = (Line *)malloc(buffer_lines(b) * sizeof(Line));
    for (int y = 0; y < buffer_lines(b); ++y) {
        b->lines[y] = (Cell *)malloc(b->cols * sizeof(Cell));
        for (int x = 0; x < b->cols; ++x)
            b->lines[y][x] = DEFAULT_CELL(' ');
    }
    buffer_clear(b);
}

void buffer_destroy(CluTermBuffer *b)
{
    if (b->lines) {
        for (int y = 0; y < b->rows; ++y)
            free(b->lines[y]);
        free(b->lines);
    }
    printf("buffer cleanup: Done!.\n");
}

void buffer_clearline(CluTermBuffer *b, int y, int x0, int x1)
{
    for (; x0 <= x1; ++x0)
        buffer_addcell(b, y, x0, CELL(' ', b->cell_attrs));
}

void buffer_clearbox(CluTermBuffer *b, int y0, int x0, int y1, int x1)
{
    for (; y0 <= y1; ++y0)
        buffer_clearline(b, y0, x0, x1);
}

void buffer_addlines(CluTermBuffer *b, int lines)
{
    b->last_row += lines;
    buffer_clearbox(b, b->rows - lines, 0, b->rows - 1, b->cols - 1);
    adjust(b);
}

void buffer_scrollup_relative(CluTermBuffer *b, int origin, int lines)
{
    if (!lines)
        return;
    lines = MIN(lines, b->scroll_region.end - b->scroll_region.start);
    for (int i = origin - 1; i >= 0; --i)
        SWAP(b->lines[tline(b, i)], b->lines[tline(b, i + lines)]);
    buffer_addlines(b, lines);
    for (int i = b->rows - 1; i > b->scroll_region.end; --i)
        SWAP(b->lines[tline(b, i)], b->lines[tline(b, i - lines)]);
}

void buffer_scrolldown_relative(CluTermBuffer *b, int origin, int lines)
{
    if (!lines)
        return;
    lines =
        MIN(lines, b->scroll_region.end - MAX(b->scroll_region.start, origin));
    for (int i = b->scroll_region.end; i >= origin + lines; --i)
        SWAP(b->lines[tline(b, i)], b->lines[tline(b, i - lines)]);
    buffer_clearbox(b, origin, 0, origin + lines - 1, b->cols - 1);
}

static inline void buffer_insert_delete_chars(CluTermBuffer *b, int count,
                                              bool insert)
{
    Cell *xptr = b->lines[tline(b, b->cursor.y)] + b->cursor.x;
    int dx     = MIN(b->cols - 1 - b->cursor.x, count),
        shift  = b->cols - 1 - b->cursor.x - dx;

    insert ? memmove(xptr + dx, xptr, shift * sizeof(Cell))
           : memmove(xptr, xptr + dx, shift * sizeof(Cell));

    xptr += shift * !insert;
    for (int i = 0; i < dx; ++i)
        *(xptr + i) = CELL(' ', b->cell_attrs); // DebugCell;
}

void buffer_insert_chars(CluTermBuffer *b, int count)
{
    buffer_insert_delete_chars(b, count, true);
}

void buffer_delete_chars(CluTermBuffer *b, int count)
{
    buffer_insert_delete_chars(b, count, false);
}

void buffer_resize(CluTermBuffer *b, int rows, int cols)
{
    printf("buffer resized to %dx%d.\n", cols, rows);
    Line *lines = malloc(rows * sizeof(Line));
    for (int y = 0; y < rows; ++y) {
        lines[y] = malloc(cols * sizeof(Cell));
        for (int x = 0; x < cols; ++x)
            lines[y][x] = DEFAULT_CELL(' ');
    }
    for (int y = 0; y < MIN(rows, b->rows); ++y) {
        memmove(lines[y], b->lines[tline(b, y)],
                MIN(cols, b->cols) * sizeof(Cell));
        lines[y] = realloc(lines[y], cols * sizeof(Cell));
    }
    for (int y = 0; y < b->rows; ++y)
        free(b->lines[y]);
    free(b->lines);
    b->rows = rows, b->cols = cols, b->lines = lines;
    b->last_row      = rows;
    b->scroll_region = (Region){0, rows - 1};
    b->cursor.y      = MIN(b->cursor.y, rows - 1);
    b->cursor.x      = MIN(b->cursor.x, cols - 1);
    adjust(b);
}
