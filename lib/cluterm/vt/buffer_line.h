#ifndef __CLUTERM__VT__BUFFER_LINE_H__
#define __CLUTERM__VT__BUFFER_LINE_H__

#include <include/cluterm.h>

#define DEFAULT_CELL_ATTRS                                                     \
    (CellAttributes) { .fg = ColorFg(), .bg = ColorBg(), .state = CELL_NORMAL }
#define DEFAULT_CELL(val) CELL(val, DEFAULT_CELL_ATTRS)
#define CELL(val, _attrs)                                                      \
    (Cell) { .value = val, .attrs = _attrs }

BufferLine line_new(int);
void line_copy(BufferLine, BufferLine, size_t);

#endif
