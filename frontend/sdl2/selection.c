#include "selection.h"
#include "main.h"
#include <cluterm/utf8.h>

void selection_start(int y, int x)
{
    selection_clear();
    gfx->sel->anchor = y * gfx->frame.snapshot.cols + x;
    gfx_request_render(1);
}

void selection_extend(int y, int x)
{
    if (gfx->sel->anchor == -1)
        return;

    int cols = gfx->frame.snapshot.cols;
    if (y == -1)
        y = gfx->sel->pointer / cols;
    if (x == -1)
        x = gfx->sel->pointer % cols;
    gfx->sel->pointer = y * cols + x;

    gfx_request_render(1);
}

void selection_word(int y, int x)
{
    selection_clear();
    static const bool selection_boundary[128] = {
        [' '] = 1, ['\t'] = 1, ['\n'] = 1, ['\"'] = 1, ['|'] = 1, [':'] = 1,
        [';'] = 1, [','] = 1,  ['('] = 1,  [')'] = 1,  ['['] = 1, [']'] = 1,
        ['{'] = 1, ['}'] = 1,  ['<'] = 1,  ['>'] = 1,  ['$'] = 1,
    };

    const ClutermSnapshot *snap = &gfx->frame.snapshot;
    const BufferLine line       = snap->lines[y];
    int x0 = x, x1 = x;
    for (; x0 > 0; --x0) {
        Cell cell = line->cells[x0 - 1];
        if (BETWEEN(cell.value, 32, 126) && selection_boundary[cell.value])
            break;
    }
    for (; x1 < snap->cols - 1; ++x1) {
        Cell cell = line->cells[x1 + 1];
        if (BETWEEN(cell.value, 32, 126) && selection_boundary[cell.value])
            break;
    }
    gfx->sel->anchor  = y * snap->cols + x0;
    gfx->sel->pointer = y * snap->cols + x1;

    gfx_request_render(1);
}

void selection_line(int y)
{
    selection_clear();
    gfx->sel->anchor  = y * gfx->frame.snapshot.cols;
    gfx->sel->pointer = gfx->sel->anchor + gfx->frame.snapshot.cols - 1;
    gfx_request_render(1);
}

void selection_clear(void)
{
    gfx->sel->anchor = gfx->sel->pointer = -1;
    gfx_request_render(1);
}

bool selection_contains(int y, int x)
{
    if (gfx->sel->anchor == -1 || gfx->sel->pointer == -1)
        return false;

    int start = gfx->sel->anchor, end = gfx->sel->pointer,
        index = y * gfx->frame.snapshot.cols + x;
    if (start > end)
        SWAP(start, end);

    return BETWEEN(index, start, end);
}

char *selection_get_text(void)
{
    if (gfx->sel->anchor == -1 || gfx->sel->pointer == -1)
        return NULL;

    const ClutermSnapshot *snap = &gfx->frame.snapshot;

    int start = gfx->sel->anchor, end = gfx->sel->pointer;
    if (start > end)
        SWAP(start, end);

    char *buffer = calloc((end - start + 1) * 4 + snap->rows + 1, sizeof(char));
    if (!buffer)
        return NULL;

    int starty = start / snap->cols, startx = start % snap->cols;
    int endy = end / snap->cols, endx = end % snap->cols;

    char *it                = buffer;
    UTF8_String utf8_string = {0};
    for (int y = starty; y <= endy; ++y) {
        const BufferLine line = snap->lines[y];
        int x0 = startx, x1 = y == endy ? endx : snap->cols - 1;

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
