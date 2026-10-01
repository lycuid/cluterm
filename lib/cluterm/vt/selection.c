#include "selection.h"
#include "include/cluterm/vt/buffer_line.h"
#include <cluterm.h>
#include <cluterm/utf8.h>
#include <cluterm/vt/buffer.h>
#include <include/cluterm/debug.h>

void selection_start(Cluterm *term, int y, int x)
{
    selection_clear(term);
    const ClutermBuffer *b    = active_buffer(term);
    term->selection.anchor    = viewport_row(b, y) * b->cols + x;
    term->selection.pointer   = term->selection.anchor;
    b->dirty[y * b->cols + x] = 1;
    term->render_flag         = 1;
}

void selection_extend(Cluterm *term, int y, int x)
{
    if (term->selection.anchor == -1)
        return;

    ClutermBuffer *b = active_buffer(term);

    if (y != -1)
        y = viewport_row(b, y);
    if (y == -1)
        y = term->selection.pointer / b->cols;
    if (x == -1)
        x = term->selection.pointer % b->cols;

    int new_pointer = y * b->cols + x;
    // @TODO: optimize this later, right now just wanna see it work.
    {
        int start = term->selection.anchor, end = term->selection.pointer;
        if (start > end)
            SWAP(start, end);
        for (int i = start; i <= end; ++i) {
            Cell *cell = &b->lines[i / b->cols]->cells[i % b->cols];
            UNSET(cell->attrs.state, CELL_SELECTION);
        }
        term->selection.pointer = new_pointer;
        start = term->selection.anchor, end = term->selection.pointer;
        if (start > end)
            SWAP(start, end);
        for (int i = start; i <= end; ++i) {
            Cell *cell = &b->lines[i / b->cols]->cells[i % b->cols];
            SET(cell->attrs.state, CELL_SELECTION);
        }
    }
    dirty_buffer(b);

    term->render_flag = 1;
}

void selection_word(Cluterm *term, int y, int x)
{
    selection_clear(term);
    static const bool selection_boundary[128] = {
        [' '] = 1, ['\t'] = 1, ['\n'] = 1, ['\"'] = 1, ['|'] = 1, [':'] = 1,
        [';'] = 1, [','] = 1,  ['('] = 1,  [')'] = 1,  ['['] = 1, [']'] = 1,
        ['{'] = 1, ['}'] = 1,  ['<'] = 1,  ['>'] = 1,  ['$'] = 1,
    };

    const ClutermBuffer *b = active_buffer(term);

    int row               = viewport_row(b, y);
    const BufferLine line = b->lines[row];
    int x0 = x, x1 = x;
    for (; x0 > 0; --x0) {
        Cell cell = line->cells[x0 - 1];
        if (BETWEEN(cell.value, 32, 126) && selection_boundary[cell.value])
            break;
    }
    for (; x1 < b->cols - 1; ++x1) {
        Cell cell = line->cells[x1 + 1];
        if (BETWEEN(cell.value, 32, 126) && selection_boundary[cell.value])
            break;
    }
    term->selection.anchor  = row * b->cols + x0;
    term->selection.pointer = row * b->cols + x1;

    memset(&b->dirty[y * b->cols + x0], 1, (x1 - x0) * sizeof(bool));

    term->render_flag = 1;
}

void selection_line(Cluterm *term, int y)
{
    selection_clear(term);

    const ClutermBuffer *b  = active_buffer(term);
    term->selection.anchor  = y * b->cols;
    term->selection.pointer = term->selection.anchor + b->cols - 1;
    term->render_flag       = 1;
    dirty_line(b, y);
}

void selection_clear(Cluterm *term)
{
    if (term->selection.anchor == -1)
        return;

    const ClutermBuffer *b = active_buffer(term);
    int start = term->selection.anchor, end = term->selection.pointer;
    if (start > end)
        SWAP(start, end);
    for (int i = start; i <= end; ++i) {
        Cell *cell = &b->lines[i / b->cols]->cells[i % b->cols];
        UNSET(cell->attrs.state, CELL_SELECTION);
    }
    term->selection.anchor = term->selection.pointer = -1;
    term->render_flag                                = 1;
    dirty_buffer(active_buffer(term));
}

char *selection_get_text(Cluterm *term)
{
    if (term->selection.anchor == -1 || term->selection.pointer == -1)
        return NULL;

    const ClutermBuffer *b = active_buffer(term);

    int start = term->selection.anchor, end = term->selection.pointer;
    if (start > end)
        SWAP(start, end);

    char *buffer = calloc((end - start + 1) * 4 + b->rows + 1, sizeof(char));
    if (!buffer)
        return NULL;

    int starty = start / b->cols, startx = start % b->cols;
    int endy = end / b->cols, endx = end % b->cols;

    char *it                = buffer;
    UTF8_String utf8_string = {0};
    for (int y = starty; y <= endy; ++y) {
        dirty_lines(b, y, 1);
        const BufferLine line = b->lines[y];
        int x0 = startx, x1 = y == endy ? endx : b->cols - 1;

        for (int x = x0; x <= x1; ++x) {
            size_t utf8_len = utf8_encode(line->cells[x].value, utf8_string);
            for (size_t ch = 0; ch < utf8_len; ++ch)
                *it++ = utf8_string[ch];
        }

        if (!IS_SET(line->state, LINE_WRAPPED)) {
            while (it > buffer && (it[-1] == ' ' || it[-1] == '\t'))
                --it;
            if (y != endy)
                *it++ = '\n';
        }

        startx = 0;
    }
    *it = 0;

    return buffer;
}
