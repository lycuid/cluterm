#include "key.h"
#include <cluterm.h>
#include <stdbool.h>

ssize_t send_arrow(const Cluterm *term, char final, ClutermMod mod)
{
    bool ctrl  = IS_SET_ANY(mod, CLUTERM_MOD_CTRL),
         shift = IS_SET_ANY(mod, CLUTERM_MOD_SHIFT),
         alt   = IS_SET_ANY(mod, CLUTERM_MOD_ALT);

    if (ctrl && shift && alt)
        return term->actions.send((char[]){27, '[', '1', ';', '8', final}, 6);
    if (alt && ctrl)
        return term->actions.send((char[]){27, '[', '1', ';', '7', final}, 6);
    if (ctrl && shift)
        return term->actions.send((char[]){27, '[', '1', ';', '6', final}, 6);
    if (shift && alt)
        return term->actions.send((char[]){27, '[', '1', ';', '4', final}, 6);
    if (ctrl)
        return term->actions.send((char[]){27, '[', '1', ';', '5', final}, 6);
    if (shift)
        return term->actions.send((char[]){27, '[', '1', ';', '2', final}, 6);
    if (alt)
        return term->actions.send((char[]){27, '[', '1', ';', '3', final}, 6);

    if (IS_SET(term->mode, MODE_APP_CURSOR_KEYS))
        return term->actions.send((char[]){27, 'O', final}, 3);

    return term->actions.send((char[]){27, '[', final}, 3);
}

bool keydown(const Cluterm *term, const ClutermKey *key)
{
#define is(mask) (key->mod == (mask))
#define ctrl     CLUTERM_MOD_CTRL
#define alt      CLUTERM_MOD_ALT
#define shift    CLUTERM_MOD_SHIFT

    switch (key->code) {
    case CLUTERM_KEYCODE_a: goto mod_put;
    case CLUTERM_KEYCODE_b: goto mod_put;
    case CLUTERM_KEYCODE_c: goto mod_put;
    case CLUTERM_KEYCODE_d: goto mod_put;
    case CLUTERM_KEYCODE_e: goto mod_put;
    case CLUTERM_KEYCODE_f: goto mod_put;
    case CLUTERM_KEYCODE_g: goto mod_put;
    case CLUTERM_KEYCODE_h: goto mod_put;
    case CLUTERM_KEYCODE_i: goto mod_put;
    case CLUTERM_KEYCODE_j: goto mod_put;
    case CLUTERM_KEYCODE_k: goto mod_put;
    case CLUTERM_KEYCODE_l: goto mod_put;
    case CLUTERM_KEYCODE_m: goto mod_put;
    case CLUTERM_KEYCODE_n: goto mod_put;
    case CLUTERM_KEYCODE_o: goto mod_put;
    case CLUTERM_KEYCODE_p: goto mod_put;
    case CLUTERM_KEYCODE_q: goto mod_put;
    case CLUTERM_KEYCODE_r: goto mod_put;
    case CLUTERM_KEYCODE_s: goto mod_put;
    case CLUTERM_KEYCODE_t: goto mod_put;
    case CLUTERM_KEYCODE_u: goto mod_put;
    case CLUTERM_KEYCODE_v: goto mod_put;
    case CLUTERM_KEYCODE_w: goto mod_put;
    case CLUTERM_KEYCODE_x: goto mod_put;
    case CLUTERM_KEYCODE_y: goto mod_put;
    case CLUTERM_KEYCODE_z: {
    mod_put:
        if (is(ctrl)) {
            term->actions.send((char[]){key->code - 'a' + 1}, 1);
        } else if (is(alt)) {
            term->actions.send((char[]){'\x1b', key->code}, 2);
        } else {
            return false;
        }
    } break;

        // clang-format off
    case CLUTERM_KEYCODE_LBRACKET: {
        if      (is(alt | ctrl))  term->actions.send("\x1b\x1b[",  3);
        else if (is(alt))         term->actions.send("\x1b[",      2);
        else if (is(ctrl))        term->actions.send("\x1b",       1);
        else return false;
    } break;
    case CLUTERM_KEYCODE_RBRACKET: {
        if      (is(alt | ctrl))  term->actions.send("\x1b\x1b]",  3);
        else if (is(alt))         term->actions.send("\x1b]",      2);
        else if (is(ctrl))        term->actions.send("\x1d",       1);
        else return false;
    } break;
    case CLUTERM_KEYCODE_F1:  term->actions.send("\x1bOP",    3); break;
    case CLUTERM_KEYCODE_F2:  term->actions.send("\x1bOQ",    3); break;
    case CLUTERM_KEYCODE_F3:  term->actions.send("\x1bOR",    3); break;
    case CLUTERM_KEYCODE_F4:  term->actions.send("\x1bOS",    3); break;
    case CLUTERM_KEYCODE_F5:  term->actions.send("\x1b[15~",  5); break;
    case CLUTERM_KEYCODE_F6:  term->actions.send("\x1b[17~",  5); break;
    case CLUTERM_KEYCODE_F7:  term->actions.send("\x1b[18~",  5); break;
    case CLUTERM_KEYCODE_F8:  term->actions.send("\x1b[19~",  5); break;
    case CLUTERM_KEYCODE_F9:  term->actions.send("\x1b[20~",  5); break;
    case CLUTERM_KEYCODE_F10: term->actions.send("\x1b[21~",  5); break;
    case CLUTERM_KEYCODE_F11: term->actions.send("\x1b[23~",  5); break;
    case CLUTERM_KEYCODE_F12: term->actions.send("\x1b[24~",  5); break;

    case CLUTERM_KEYCODE_RETURN:    term->actions.send("\r",   1);   break;
    case CLUTERM_KEYCODE_TAB:       term->actions.send("\t",   1);   break;
    case CLUTERM_KEYCODE_BACKSPACE: term->actions.send("\x7f", 1);   break;
    case CLUTERM_KEYCODE_ESCAPE:    term->actions.send("\x1b", 1);   break;

    case CLUTERM_KEYCODE_UP:        send_arrow_up(term, key->mod);    break;
    case CLUTERM_KEYCODE_DOWN:      send_arrow_down(term, key->mod);  break;
    case CLUTERM_KEYCODE_RIGHT:     send_arrow_right(term, key->mod); break;
    case CLUTERM_KEYCODE_LEFT:      send_arrow_left(term, key->mod);  break;

    case CLUTERM_KEYCODE_HOME:      term->actions.send("\x1b[H", 3);  break;
    case CLUTERM_KEYCODE_END:       term->actions.send("\x1b[F", 3);  break;
    case CLUTERM_KEYCODE_INSERT:    term->actions.send("\x1b[2~", 4); break;
    case CLUTERM_KEYCODE_DELETE:    term->actions.send("\x1b[3~", 4); break;
    case CLUTERM_KEYCODE_PAGEUP:    term->actions.send("\x1b[5~", 4); break;
    case CLUTERM_KEYCODE_PAGEDOWN:  term->actions.send("\x1b[6~", 4); break;
        // clang-format on
    }
    return true;

#undef shift
#undef alt
#undef ctrl
#undef is
}
