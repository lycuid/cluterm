#include <cluterm/terminal.h>
#include <cluterm/terminal/actions.h>
#include <cluterm/terminal/colors.h>
#include <cluterm/vte/parser.h>
#include <cluterm/vte/tty.h>
#include <config.h>
#include <stdbool.h>
#include <unistd.h>
// clang-format off

#define PARAM(n) (n < csi->nparam ? MAX(1, csi->param[n]) : 1)

EXPORT void action_csi(Terminal *term, CSI_Payload *csi)
{
    TerminalBuffer *b = ACTIVE_BUFFER(term);
    Cursor *cursor    = &b->cursor;
    switch (csi->action) {
    case CSI_CUU: term_cmove(term, -PARAM(0), 0);           break;
    case CSI_CUD: term_cmove(term,  PARAM(0), 0);           break;
    case CSI_CUF: term_cmove(term,  0,  PARAM(0));          break;
    case CSI_CUB: term_cmove(term,  0, -PARAM(0));          break;
    case CSI_CNL: term_cmove(term,  PARAM(0), -cursor->x);  break;
    case CSI_CPL: term_cmove(term, -PARAM(0), -cursor->x);  break;
    case CSI_VPA: { // 1-based values (default: 1).
        term_cmoveto(term, CLAMP(csi->param[0], 1, b->rows) - 1, cursor->x);
    } break;
    case CSI_CHA: { // 1-based values (default: 1).
        term_cmoveto(term, cursor->y, CLAMP(csi->param[0], 1, b->cols) - 1);
    } break;
    case CSI_CHT: term_puttab(term, csi->param[0],  1);     break;
    case CSI_CBT: term_puttab(term, csi->param[0], -1);     break;
    case CSI_TBC: {
        switch (csi->param[0]) {
        case 0: { term->tab[cursor->x] = false; }       break;
        case 3: { memset(term->tab, 0, b->cols + 1); }  break;
        }
    } break;
    // 1-based values (default: 1).
    case CSI_HVP: // fallthrough.
    case CSI_CUP: term_cmoveto(term, PARAM(0) - 1, PARAM(1) - 1); break;
    case CSI_ED: { // cursor position shouldn't change.
        switch (csi->param[0]) {
        case 0: { // clear from cursor to the end of the screen.
            buffer_clearline(b, cursor->y, cursor->x, b->cols - 1);
            buffer_clearbox(b, cursor->y + 1, 0, b->rows - 1, b->cols - 1);
        } break;
        case 1: { // clear from cursor to the beginning of the screen.
            buffer_clearline(b, cursor->y, 0, cursor->x);
            buffer_clearbox(b, 0, 0, cursor->y - 1, b->cols - 1);
        } break;
        case 2: buffer_clear(b); break; // clear the entire screen.
        // clear the entire screen and reset scrollback buffer.
        case 3: buffer_clear(b); break;
        default: break;
        }
    } break;
    case CSI_EL: { // cursor position shouldn't change.
        switch (csi->param[0]) {
        case 0: { // clear from cursor to the end of the line.
            buffer_clearline(b, cursor->y, cursor->x, b->cols - 1);
        } break;
        case 1: { // clear from cursor to the beginning of the line.
            buffer_clearline(b, cursor->y, 0, cursor->x);
        } break;
        case 2: { // clear the entire line.
            buffer_clearline(b, cursor->y, 0, b->cols - 1);
        } break;
        default: break;
        }
    } break;
    case CSI_IL:  buffer_scrolldown_relative(b, cursor->y, PARAM(0));   break;
    case CSI_DL:  buffer_scrollup_relative(b, cursor->y, PARAM(0));     break;
    case CSI_ICH: buffer_insert_chars(b, PARAM(0));                     break;
    case CSI_DCH: buffer_delete_chars(b, PARAM(0));                     break;
    case CSI_ECH: {
        int offset = CLAMP(cursor->x + PARAM(0), 1, b->cols) - 1;
        for (int x = cursor->x; x <= offset; ++x)
            b->lines[tline(b, cursor->y)][x] = (Cell)CELL(' ', b->cell_attrs);
    } break;
    case CSI_SU: buffer_scrollup(b, PARAM(0));      break;
    case CSI_SD: buffer_scrolldown(b, PARAM(0));    break;
        // clang-format off
    case CSI_SGR: {
        CellAttributes *attrs = &b->cell_attrs;
        if (!csi->nparam)
            *attrs = (CellAttributes)DEFAULT_CELL_ATTRS;
        for (int i = 0; i < csi->nparam; ++i) {
            switch (csi->param[i]) {
            case 0:   *attrs = (CellAttributes)DEFAULT_CELL_ATTRS;    break;
            case 1:   SET(attrs->state, CELL_BOLD);                   break;
            case 3:   SET(attrs->state, CELL_ITALIC);                 break;
            case 4:   SET(attrs->state, CELL_UNDERLINE);              break;
            case 7:   attrs->fg = DefaultBG; attrs->bg = DefaultFG;   break;
            case 21:  UNSET(attrs->state, CELL_BOLD);                 break;
            case 24:  UNSET(attrs->state, CELL_ITALIC);               break;
            case 27:  attrs->fg = DefaultFG; attrs->bg = DefaultBG;   break;
            // color 0-8 foreground.
            case 30: case 31: case 32: case 33: case 34: case 35: case 36: case 37: {
                attrs->fg = color16[csi->param[i] - 30];
            } break;
            case 39: attrs->fg = DefaultFG; break;
            // color 0-8 background.
            case 40: case 41: case 42: case 43: case 44: case 45: case 46: case 47: {
                attrs->bg = color16[csi->param[i] - 40];
            } break;
            case 49: attrs->bg = DefaultBG; break;
            // color 8-16 foreground.
            case 90: case 91: case 92: case 93: case 94: case 95: case 96: case 97: {
                attrs->fg = color16[csi->param[i] - 90 + 8];
            } break;
            // color 8-16 background.
            case 100: case 101: case 102: case 103: case 104: case 105: case 106: case 107: {
                attrs->bg = color16[csi->param[i] - 100 + 8];
            } break;
#define GetColor(e, color)                                                     \
    {                                                                          \
        if (i + 1 < (e)->nparam) {                                             \
            if ((e)->param[i + 1] == 5 && i + 2 < (e)->nparam) {               \
                color = color256((e)->param[i + 2]);                           \
                i += 2;                                                        \
            } else if ((e)->param[i + 1] == 2 && i + 4 < (e)->nparam) {        \
                color = RGB((e)->param[i + 2], (e)->param[i + 3],              \
                        (e)->param[i + 4]);                                    \
                i += 4;                                                        \
            }                                                                  \
        }                                                                      \
    }
            case 38: { GetColor(csi, attrs->fg); } break;
            case 48: { GetColor(csi, attrs->bg); } break;
#undef GetColor
            default: break;
            }
        }
    } break;
    case CSI_SC: term_cursor_save(term);    break;
    case CSI_RC: term_cursor_restore(term); break;
    case CSI_DECSTBM: {
        Region *region = &b->scroll_region;
        region->start = PARAM(0) - 1, region->end = PARAM(1) - 1;
        if (region->start >= region->end)
            region->start = 0, region->end = b->rows - 1;
    } break;
    case CSI_DECSET: /* fallthrough. */
    case CSI_DECRST: {
        for (int i = 0, set = csi->action == CSI_DECSET; i < csi->nparam; ++i) {
            switch (csi->param[i]) {
            case 2: { // CSI_DECANM
                if (set)
                    memset(b->charset, CS_USASCII, sizeof(b->charset));
            } break;
            // CSI_DECOM: Set Origin mode, VT100.
            case 6: UPDATE(term->mode, MODE_ORIGIN, set);       break;
            // CSI_DECTCEM: Show cursor, VT220.
            case 25: UPDATE(cursor->state, CursorHide, !set);   break;
            case 1049: { // Alternate screen buffer.
                UPDATE(term->mode, MODE_ALT_BUFFER, set);
                if (set)
                    buffer_clear(ACTIVE_BUFFER(term));
            } break;
            }
        }
    } break;
    case CSI_UNKNOWN: break;
    }
}

#undef PARAM
