#ifndef __CLUTERM__ACTIONS__CSI_H__
#define __CLUTERM__ACTIONS__CSI_H__

#include <cluterm.h>
#include <cluterm/vt/actions.h>
#include <cluterm/vt/buffer.h>
#include <cluterm/vt/palette.h>
#include <config.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

static inline void csi_tbc(CluTerm *term, CSI_Payload *csi)
{
    CluTermBuffer *b = ACTIVE_BUFFER(term);
    Cursor *cursor   = &b->cursor;

    switch (csi->param[0]) {
    case 0: {
        b->tab[cursor->x] = 0;
    } break;
    case 3: {
        memset(b->tab, 0, (b->cols + 1) * sizeof(*b->tab));
    } break;
    }
}

// cursor position shouldn't change.
static inline void csi_ed(CluTerm *term, CSI_Payload *csi)
{
    CluTermBuffer *b = ACTIVE_BUFFER(term);
    Cursor *cursor   = &b->cursor;

    switch (csi->param[0]) {
    case 0: { // clear from cursor to the end of the screen.
        clearline(b, cursor->y, cursor->x, b->cols - 1);
        clearbox(b, cursor->y + 1, 0, b->rows - 1, b->cols - 1);
    } break;
    case 1: { // clear from cursor to the beginning of the screen.
        clearline(b, cursor->y, 0, cursor->x);
        clearbox(b, 0, 0, cursor->y - 1, b->cols - 1);
    } break;
    case 2:
        clear(b);
        break; // clear the entire screen.
    // clear the entire screen and reset scrollback buffer.
    case 3: clear(b); break;
    default: break;
    }
}

// cursor position shouldn't change.
static inline void csi_el(CluTerm *term, CSI_Payload *csi)
{
    CluTermBuffer *b = ACTIVE_BUFFER(term);
    Cursor *cursor   = &b->cursor;

    switch (csi->param[0]) {
    case 0: { // clear from cursor to the end of the line.
        clearline(b, cursor->y, cursor->x, b->cols - 1);
    } break;
    case 1: { // clear from cursor to the beginning of the line.
        clearline(b, cursor->y, 0, cursor->x);
    } break;
    case 2: { // clear the entire line.
        clearline(b, cursor->y, 0, b->cols - 1);
    } break;
    default: break;
    }
}

static inline void csi_sgr(CluTerm *term, CSI_Payload *csi)
{
    CluTermBuffer *b = ACTIVE_BUFFER(term);

    CellAttributes *attrs = &b->cell_attrs;
    if (!csi->nparam)
        *attrs = DEFAULT_CELL_ATTRS;

    for (int i = 0; i < csi->nparam; ++i) {
        switch (csi->param[i]) {
        case 0: *attrs = DEFAULT_CELL_ATTRS; break;
        case 1: SET(attrs->state, CELL_BOLD); break;
        case 3: SET(attrs->state, CELL_ITALIC); break;
        case 4: SET(attrs->state, CELL_UNDERLINE); break;
        case 7: {
            attrs->fg = DefaultBG;
            attrs->bg = DefaultFG;
        } break;

        case 21: UNSET(attrs->state, CELL_BOLD); break;
        case 23: UNSET(attrs->state, CELL_ITALIC); break;
        case 24: UNSET(attrs->state, CELL_UNDERLINE); break;
        case 27: {
            attrs->fg = DefaultFG;
            attrs->bg = DefaultBG;
        } break;

        // color 0-8 foreground.
        case 30: // fallthrough.
        case 31: // fallthrough.
        case 32: // fallthrough.
        case 33: // fallthrough.
        case 34: // fallthrough.
        case 35: // fallthrough.
        case 36: // fallthrough.
        case 37: attrs->fg = color16[csi->param[i] - 30]; break;
        case 39: attrs->fg = DefaultFG; break;
        // color 0-8 background.
        case 40: // fallthrough.
        case 41: // fallthrough.
        case 42: // fallthrough.
        case 43: // fallthrough.
        case 44: // fallthrough.
        case 45: // fallthrough.
        case 46: // fallthrough.
        case 47: attrs->bg = color16[csi->param[i] - 40]; break;
        case 49: attrs->bg = DefaultBG; break;
        // color 8-16 foreground.
        case 90: // fallthrough.
        case 91: // fallthrough.
        case 92: // fallthrough.
        case 93: // fallthrough.
        case 94: // fallthrough.
        case 95: // fallthrough.
        case 96: // fallthrough.
        case 97: attrs->fg = color16[csi->param[i] - 90 + 8]; break;
        // color 8-16 background.
        case 100: // fallthrough.
        case 101: // fallthrough.
        case 102: // fallthrough.
        case 103: // fallthrough.
        case 104: // fallthrough.
        case 105: // fallthrough.
        case 106: // fallthrough.
        case 107: attrs->bg = color16[csi->param[i] - 100 + 8]; break;

#define GetColor(e, color)                                                     \
    {                                                                          \
        if (i + 1 < (e)->nparam) {                                             \
            if ((e)->param[i + 1] == 5 && i + 2 < (e)->nparam) {               \
                color = color256((e)->param[i + 2]);                           \
                i += 2;                                                        \
            } else if ((e)->param[i + 1] == 2 && i + 4 < (e)->nparam) {        \
                color = RGB((e)->param[i + 2], (e)->param[i + 3],              \
                            (e)->param[i + 4]);                                \
                i += 4;                                                        \
            }                                                                  \
        }                                                                      \
    }
        case 38: GetColor(csi, attrs->fg); break;
        case 48: GetColor(csi, attrs->bg); break;
#undef GetColor

        default: break;
        }
    }
}

static inline void csi_decmode(CluTerm *term, CSI_Payload *csi, bool is_decset)
{

    for (int i = 0; i < csi->nparam; ++i) {
        CluTermBuffer *b = ACTIVE_BUFFER(term);

        switch (csi->param[i]) {

        // CSI_DECANM
        case 2: {
            if (is_decset)
                memset(b->charset, CS_USASCII, sizeof(b->charset));
        } break;

        // CSI_DECOM: Set Origin mode, VT100.
        case 6: UPDATE(term->mode, MODE_ORIGIN, is_decset); break;

        // CSI_DECTCEM: Show cursor, VT220.
        case 25: UPDATE(b->cursor.state, CursorHide, !is_decset); break;

        // Alternate screen buffer with save/restore cursor and screen clear.
        case 1049: {
            if (is_decset) {
                save_cursor(b);
                SET(term->mode, MODE_ALT_BUFFER);
                clear(&term->buffer[1]);
            } else {
                UNSET(term->mode, MODE_ALT_BUFFER);
                restore_cursor(b);
                dirty_buffer(&term->buffer[0]);
            }
        } break;

        // Bracketed paste mode.
        case 2004: UPDATE(term->mode, MODE_BRACKETED_PASTE, is_decset); break;
        }
    }
}

#define PARAM(n) (n < csi->nparam ? MAX(1, csi->param[n]) : 1)
// adjust value based on the origin mode.
#define DECOM(n)                                                               \
    n + (IS_SET(term->mode, MODE_ORIGIN) ? b->scroll_region.start : 0)

EXPORT void csi_execute(CluTerm *term, CSI_Payload *csi)
{
    CluTermBuffer *b = ACTIVE_BUFFER(term);
    Cursor *cursor   = &b->cursor;

    switch (csi->action) {
    case CSI_CUU: move_cursor(b, -PARAM(0), 0); break;
    case CSI_CUD: move_cursor(b, PARAM(0), 0); break;
    case CSI_CUF: move_cursor(b, 0, PARAM(0)); break;
    case CSI_CUB: move_cursor(b, 0, -PARAM(0)); break;
    case CSI_CNL: move_cursor(b, PARAM(0), -cursor->x); break;
    case CSI_CPL: move_cursor(b, -PARAM(0), -cursor->x); break;

    case CSI_VPA: { // 1-based values (default: 1).
        move_cursor_to(b, DECOM(PARAM(0)) - 1, cursor->x);
    } break;
    case CSI_CHA: { // 1-based values (default: 1).
        move_cursor_to(b, cursor->y, CLAMP(csi->param[0], 1, b->cols) - 1);
    } break;

    case CSI_CHT: insert_tab(b, PARAM(0), 1); break;
    case CSI_CBT: insert_tab(b, PARAM(0), -1); break;
    case CSI_TBC: csi_tbc(term, csi); break;

    // 1-based values (default: 1).
    case CSI_HVP: // fallthrough.
    case CSI_CUP: move_cursor_to(b, DECOM(PARAM(0)) - 1, PARAM(1) - 1); break;

    case CSI_ED: csi_ed(term, csi); break;
    case CSI_EL: csi_el(term, csi); break;

    case CSI_IL: scrolldown_rel(b, cursor->y, PARAM(0)); break;
    case CSI_DL: scrollup_rel(b, cursor->y, PARAM(0)); break;
    case CSI_ICH: insert_chars(b, PARAM(0)); break;
    case CSI_DCH: delete_chars(b, PARAM(0)); break;

    case CSI_ECH: {
        int offset = CLAMP(cursor->x + PARAM(0), 1, b->cols) - 1;
        for (int x = cursor->x; x <= offset; ++x)
            putcell(b, cursor->y, x, CELL(' ', b->cell_attrs));
    } break;

    case CSI_SU: scrollup(b, PARAM(0)); break;
    case CSI_SD: scrolldown(b, PARAM(0)); break;

    case CSI_SGR: csi_sgr(term, csi); break;

    case CSI_SC: save_cursor(b); break;
    case CSI_RC: restore_cursor(b); break;

    case CSI_DECSTBM: {
        Region *region = &b->scroll_region;
        region->start = PARAM(0) - 1, region->end = b->rows - 1;

        if (csi->nparam >= 2)
            region->end = MIN(region->end, PARAM(1) - 1);

        if (region->start >= region->end)
            region->start = 0, region->end = b->rows - 1;
    } break;

    case CSI_DECSET: /* fallthrough. */
    case CSI_DECRST: csi_decmode(term, csi, csi->action == CSI_DECSET); break;
    case CSI_UNKNOWN: break;
    }
}

#undef PARAM
#endif
