#ifndef __CLUTERM__ACTIONS__ESC_H__
#define __CLUTERM__ACTIONS__ESC_H__

#include <cluterm.h>
#include <cluterm/vt/actions.h>
#include <stdbool.h>

EXPORT void esc_execute(CluTerm *term, ESC_Payload *esc)
{
    CluTermBuffer *b = ACTIVE_BUFFER(term);
    Cursor *cursor   = &b->cursor;

    switch (esc->action) {
    case ESC_IND: linefeed(b); break;
    case ESC_RI: {
        cursor->y == b->scroll_region.start ? scrolldown(b, 1)
                                            : move_cursor(b, -1, 0);
    } break;
    case ESC_HTS: b->tab[cursor->x] = 1; break;
    case ESC_CS_LINEGFX: {
        b->charset[esc->interm[0] - '('] = CS_LINEGFX;
    } break;
    case ESC_CS_USASCII: {
        b->charset[esc->interm[0] - '('] = CS_USASCII;
    } break;
    case ESC_DECSC: save_cursor(b); break;
    case ESC_DECRC: restore_cursor(b); break;
    case ESC_UNKNOWN: break;
    }
}

#endif
