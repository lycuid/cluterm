#ifndef __SDL2__HANDLERS__KEYPRESS_H__
#define __SDL2__HANDLERS__KEYPRESS_H__

#include <SDL2/SDL.h>
#include <cluterm.h>

static inline int get_keycode(SDL_KeyCode code)
{
    switch (code) {
    case SDLK_BACKSPACE: return CLUTERM_KEYCODE_BACKSPACE;
    case SDLK_TAB: return CLUTERM_KEYCODE_TAB;
    case SDLK_RETURN: // fallthrough
    case SDLK_RETURN2: return CLUTERM_KEYCODE_RETURN;
    case SDLK_ESCAPE: return CLUTERM_KEYCODE_ESCAPE;
    case SDLK_a: return CLUTERM_KEYCODE_a;
    case SDLK_b: return CLUTERM_KEYCODE_b;
    case SDLK_c: return CLUTERM_KEYCODE_c;
    case SDLK_d: return CLUTERM_KEYCODE_d;
    case SDLK_e: return CLUTERM_KEYCODE_e;
    case SDLK_f: return CLUTERM_KEYCODE_f;
    case SDLK_g: return CLUTERM_KEYCODE_g;
    case SDLK_h: return CLUTERM_KEYCODE_h;
    case SDLK_i: return CLUTERM_KEYCODE_i;
    case SDLK_j: return CLUTERM_KEYCODE_j;
    case SDLK_k: return CLUTERM_KEYCODE_k;
    case SDLK_l: return CLUTERM_KEYCODE_l;
    case SDLK_m: return CLUTERM_KEYCODE_m;
    case SDLK_n: return CLUTERM_KEYCODE_n;
    case SDLK_o: return CLUTERM_KEYCODE_o;
    case SDLK_p: return CLUTERM_KEYCODE_p;
    case SDLK_q: return CLUTERM_KEYCODE_q;
    case SDLK_r: return CLUTERM_KEYCODE_r;
    case SDLK_s: return CLUTERM_KEYCODE_s;
    case SDLK_t: return CLUTERM_KEYCODE_t;
    case SDLK_u: return CLUTERM_KEYCODE_u;
    case SDLK_v: return CLUTERM_KEYCODE_v;
    case SDLK_w: return CLUTERM_KEYCODE_w;
    case SDLK_x: return CLUTERM_KEYCODE_x;
    case SDLK_y: return CLUTERM_KEYCODE_y;
    case SDLK_z: return CLUTERM_KEYCODE_z;
    case SDLK_LEFTBRACKET: return CLUTERM_KEYCODE_LBRACKET;
    case SDLK_RIGHTBRACKET: return CLUTERM_KEYCODE_RBRACKET;
    case SDLK_F1: return CLUTERM_KEYCODE_F1;
    case SDLK_F2: return CLUTERM_KEYCODE_F2;
    case SDLK_F3: return CLUTERM_KEYCODE_F3;
    case SDLK_F4: return CLUTERM_KEYCODE_F4;
    case SDLK_F5: return CLUTERM_KEYCODE_F5;
    case SDLK_F6: return CLUTERM_KEYCODE_F6;
    case SDLK_F7: return CLUTERM_KEYCODE_F7;
    case SDLK_F8: return CLUTERM_KEYCODE_F8;
    case SDLK_F9: return CLUTERM_KEYCODE_F9;
    case SDLK_F10: return CLUTERM_KEYCODE_F10;
    case SDLK_F11: return CLUTERM_KEYCODE_F11;
    case SDLK_F12: return CLUTERM_KEYCODE_F12;
    case SDLK_UP: return CLUTERM_KEYCODE_UP;
    case SDLK_DOWN: return CLUTERM_KEYCODE_DOWN;
    case SDLK_RIGHT: return CLUTERM_KEYCODE_RIGHT;
    case SDLK_LEFT: return CLUTERM_KEYCODE_LEFT;
    case SDLK_HOME: return CLUTERM_KEYCODE_HOME;
    case SDLK_END: return CLUTERM_KEYCODE_END;
    case SDLK_INSERT: return CLUTERM_KEYCODE_INSERT;
    case SDLK_DELETE: return CLUTERM_KEYCODE_DELETE;
    case SDLK_PAGEUP: return CLUTERM_KEYCODE_PAGEUP;
    case SDLK_PAGEDOWN: return CLUTERM_KEYCODE_PAGEDOWN;
    default: return -1;
    }
}

static inline ClutermMod get_mod(SDL_Keymod keymod)
{
    ClutermMod mod = 0;
    if (IS_SET_ANY(keymod, KMOD_CTRL))
        SET(mod, CLUTERM_MOD_CTRL);
    if (IS_SET_ANY(keymod, KMOD_SHIFT))
        SET(mod, CLUTERM_MOD_SHIFT);
    if (IS_SET_ANY(keymod, KMOD_ALT))
        SET(mod, CLUTERM_MOD_ALT);
    return mod;
}

void handle_keydown(const SDL_KeyboardEvent *, const Config *);

#endif
