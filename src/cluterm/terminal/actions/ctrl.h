#include <cluterm/terminal.h>
#include <cluterm/terminal/actions.h>

EXPORT void ctrl_perform_action(Terminal *term, CTRL_Payload *ctrl)
{
    TerminalBuffer *b = ACTIVE_BUFFER(term);
    Cursor *cursor    = &b->cursor;
    switch (ctrl->action) {
    case C0_BEL: /* ding dong mf. */ break;
    case C0_BS: move_cursor(term, 0, -1); break;
    case C0_HT: put_tab(term, 1, 1); break;
    case C0_LF: // fallthrough.
    case C0_VT: // fallthrough.
    case C0_FF: linefeed(term); break;
    case C0_CR: move_cursor_to(term, cursor->y, 0); break;
    case C0_SO: // fallthrough.
    case C0_SI: b->active_charset = 1 - (ctrl->action - C0_SO); break;
    }
}
