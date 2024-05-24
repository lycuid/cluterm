#ifndef __CLUTERM__ACTIONS__ESC_H__
#define __CLUTERM__ACTIONS__ESC_H__

#include <cluterm.h>
#include <cluterm/actions.h>
#include <stdbool.h>

EXPORT void esc_perform_action(CluTerm *term, ESC_Payload *esc)
{
    CluTermBuffer *b = ACTIVE_BUFFER(term);
    Cursor *cursor   = &b->cursor;
    switch (esc->action) {
    case ESC_IND: linefeed(term); break;
    case ESC_RI: {
        cursor->y == b->scroll_region.start ? buffer_scrolldown(b, 1)
                                            : move_cursor(term, -1, 0);
    } break;
    case ESC_HTS: term->tab[cursor->x] = true; break;
    case ESC_CS_LINEGFX: {
        b->charset[esc->interm[0] - '('] = CS_LINEGFX;
    } break;
    case ESC_CS_USASCII: {
        b->charset[esc->interm[0] - '('] = CS_USASCII;
    } break;
    case ESC_DECSC: save_cursor(term); break;
    case ESC_DECRC: restore_cursor(term); break;
    case ESC_UNKNOWN: break;
    }
}

#endif
