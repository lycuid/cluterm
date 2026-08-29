#include "cell.h"

Rgb cell_fg(const Cell *cell)
{
    if (IS_SET(cell->attrs.state, CELL_INVERSE))
        return resolve_color(&cell->attrs.bg);
    return resolve_color(&cell->attrs.fg);
}

Rgb cell_bg(const Cell *cell)
{
    if (IS_SET(cell->attrs.state, CELL_INVERSE))
        return resolve_color(&cell->attrs.fg);
    return resolve_color(&cell->attrs.bg);
}
