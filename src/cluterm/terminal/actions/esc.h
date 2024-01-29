#include <cluterm/terminal.h>
#include <cluterm/terminal/actions.h>
#include <stdbool.h>

EXPORT void action_esc(Terminal *term, ESC_Payload *esc)
{
    TerminalBuffer *b = ACTIVE_BUFFER(term);
    Cursor *cursor    = &b->cursor;
    switch (esc->action) {
    case ESC_IND: term_linefeed(term); break;
    case ESC_RI: {
        cursor->y == b->scroll_region.start ? buffer_scrolldown(b, 1)
                                            : term_cmove(term, -1, 0);
    } break;
    case ESC_HTS: term->tab[cursor->x] = true; break;
    case ESC_CS_LINEGFX: {
        b->charset[esc->interm[0] - '('] = CS_LINEGFX;
    } break;
    case ESC_CS_USASCII: {
        b->charset[esc->interm[0] - '('] = CS_USASCII;
    } break;
    case ESC_DECSC: term_cursor_save(term); break;
    case ESC_DECRC: term_cursor_restore(term); break;
    case ESC_UNKNOWN: break;
    }
}
