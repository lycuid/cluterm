#ifndef __CLUTERM__ACTIONS__CTRL_H__
#define __CLUTERM__ACTIONS__CTRL_H__

#include <cluterm.h>
#include <cluterm/vt/actions.h>

EXPORT void ctrl_execute(Cluterm *term, CTRL_Payload *ctrl)
{
    ClutermBuffer *b = ACTIVE_BUFFER(term);

    switch (ctrl->action) {
    case C0_NOOP: break;
    case C0_BEL: /* not supported. */ break;
    case C0_BS: move_cursor(b, 0, -1); break;
    case C0_HT: insert_tab(b, 1, 1); break;
    case C0_LF: // fallthrough.
    case C0_VT: // fallthrough.
    case C0_FF: linefeed(b); break;
    case C0_CR: move_cursor_to(b, b->cursor.y, 0); break;
    case C0_SO: // fallthrough.
    case C0_SI: b->active_charset = 1 - (ctrl->action - C0_SO); break;
    }
}

#endif
