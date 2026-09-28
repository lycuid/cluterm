#include "buffer_line.h"
#include <cluterm/utf8.h>

BufferLine line_new(int cols)
{
    BufferLine line = malloc(sizeof(struct Line));
    line->cells     = malloc(cols * sizeof(Cell));
    for (int x = 0; x < cols; ++x)
        line->cells[x] = DEFAULT_CELL(' ');
    line->state = 0;
    return line;
}

void line_copy(BufferLine dst, BufferLine src, size_t nmemb)
{
    memcpy(dst->cells, src->cells, nmemb * sizeof(Cell));
    dst->state = src->state;
}

void line_free(BufferLine *line)
{
    if (line == NULL || *line == NULL)
        return;

    free((*line)->cells);
    free(*line);
    *line = NULL;
}
