#ifndef __CLUTERM__ACTIONS_H__
#define __CLUTERM__ACTIONS_H__

#include <cluterm.h>
#include <cluterm/buffer.h>

#define UNHANDLED(action) printf("Unhandled: %s.\n", #action);

EXPORT inline void move_cursor_to(CluTerm *term, int y, int x)
{
    CluTermBuffer *b = ACTIVE_BUFFER(term);
    b->cursor.y =
        IS_SET(term->mode, MODE_ORIGIN)
            ? CLAMP(b->scroll_region.start + y, 0, b->scroll_region.end)
            : CLAMP(y, 0, b->rows - 1);
    b->cursor.x = CLAMP(x, 0, b->cols);
}

EXPORT inline void move_cursor(CluTerm *term, int dy, int dx)
{
    CluTermBuffer *b = ACTIVE_BUFFER(term);
    move_cursor_to(term, b->cursor.y + dy, b->cursor.x + dx);
}

EXPORT inline void put_tab(CluTerm *term, int count, int inc)
{
    CluTermBuffer *b = ACTIVE_BUFFER(term);
    while (BETWEEN(b->cursor.x, 0, b->cols - 1) && count--) {
        do {
            b->cursor.x += inc;
        } while (BETWEEN(b->cursor.x, 0, b->cols - 1) &&
                 !term->tab[b->cursor.x]);
    }
    b->cursor.x = CLAMP(b->cursor.x, 0, b->cols);
}

EXPORT inline Cell translate(Cell cell, Charset charset)
{
    switch (charset) {
    case CS_USASCII: break;
    case CS_LINEGFX: {
        // This table is proudly stolen from st, which was proudly stolen from
        // rxvt.
        static const char *vt100_0[/* 0x41..0x7e */] = {
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

// add cell to the current cursor position (with managed newline/wrapping etc).
EXPORT inline void put_cell(CluTerm *term, Cell cell)
{
    CluTermBuffer *b = ACTIVE_BUFFER(term);
    if (b->cursor.x == b->cols) {
        if (b->cursor.y == b->rows - 1)
            buffer_scrollup(b, 1);
        move_cursor_to(term, b->cursor.y + 1, 0);
    }
    buffer_addcell(b, b->cursor.y, b->cursor.x,
                   translate(cell, b->charset[b->active_charset]));
    move_cursor(term, 0, 1);
}

EXPORT inline void linefeed(CluTerm *term)
{
    CluTermBuffer *b = ACTIVE_BUFFER(term);
    b->cursor.y == b->scroll_region.end ? buffer_scrollup(b, 1)
                                        : move_cursor(term, 1, 0);
}

EXPORT inline void save_cursor(CluTerm *term)
{
    int buf                 = IS_SET(term->mode, MODE_ALT_BUFFER);
    term->saved_cursor[buf] = term->buffer[buf].cursor;
}

EXPORT inline void restore_cursor(CluTerm *term)
{
    int buf                  = IS_SET(term->mode, MODE_ALT_BUFFER);
    term->buffer[buf].cursor = term->saved_cursor[buf];
}

#endif
