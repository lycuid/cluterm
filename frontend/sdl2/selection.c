#include "selection.h"
#include "main.h"

void selection_start(int y, int x)
{
    selection_clear();
    gfx->sel->anchor = y * gfx->frame.buffer.cols + x;
    gfx_request_render(1);
}

void selection_extend(int y, int x)
{
    if (gfx->sel->anchor == -1)
        return;

    int cols = gfx->frame.buffer.cols;
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

    const FrameBuffer *b = &gfx->frame.buffer;
    const Line line      = b->lines[y];
    int x0 = x, x1 = x;
    for (; x0 > 0; --x0) {
        Cell cell = line[x0 - 1];
        if (BETWEEN(cell.value, 32, 126) && selection_boundary[cell.value])
            break;
    }
    for (; x1 < b->cols - 1; ++x1) {
        Cell cell = line[x1 + 1];
        if (BETWEEN(cell.value, 32, 126) && selection_boundary[cell.value])
            break;
    }
    gfx->sel->anchor  = y * b->cols + x0;
    gfx->sel->pointer = y * b->cols + x1;

    gfx_request_render(1);
}

void selection_line(int y)
{
    selection_clear();
    gfx->sel->anchor  = y * gfx->frame.buffer.cols;
    gfx->sel->pointer = gfx->sel->anchor + gfx->frame.buffer.cols;
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
        index = y * gfx->frame.buffer.cols + x;
    if (start > end)
        SWAP(start, end);

    return BETWEEN(index, start, end);
}

char *selection_get_text(void)
{
    if (gfx->sel->anchor == -1 || gfx->sel->pointer == -1)
        return NULL;

    const FrameBuffer *b = &gfx->frame.buffer;

    int start = gfx->sel->anchor, end = gfx->sel->pointer;
    if (start > end)
        SWAP(start, end);

    char *buffer = calloc((end - start + 1) * 4 + b->rows + 1, sizeof(char));
    if (!buffer)
        return NULL;

    char *it = buffer;
    for (; start <= end; ++start) {
        int y = start / b->cols, x = start % b->cols;

        Cell cell               = b->lines[y][x];
        UTF8_String utf8_string = {0};
        utf8_encode(cell.value, utf8_string);

        size_t len = strlen(utf8_string);
        memcpy(it, utf8_string, len);
        it += len;

        if (IS_SET(cell.attrs.state, CELL_LINEBREAK)) {
            while (it > buffer && (it[-1] == ' ' || it[-1] == '\t'))
                --it;
            *it++ = '\n';
            start += b->cols - (start % b->cols) - 1;
        }
    }
    while (it > buffer && (it[-1] == ' ' || it[-1] == '\t'))
        --it;
    *it = 0;

    return buffer;
}
