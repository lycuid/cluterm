#include "keypress.h"
#include "main.h"

static int f_delta = 0;

ssize_t send_arrow(char final, Uint16 keymod)
{
    bool ctrl  = IS_SET_ANY(keymod, KMOD_CTRL),
         shift = IS_SET_ANY(keymod, KMOD_SHIFT),
         alt   = IS_SET_ANY(keymod, KMOD_ALT);

    if (ctrl && shift && alt)
        return gfx_write((char[]){27, '[', '1', ';', '8', final}, 6);
    if (alt && ctrl)
        return gfx_write((char[]){27, '[', '1', ';', '7', final}, 6);
    if (ctrl && shift)
        return gfx_write((char[]){27, '[', '1', ';', '6', final}, 6);
    if (shift && alt)
        return gfx_write((char[]){27, '[', '1', ';', '4', final}, 6);
    if (ctrl)
        return gfx_write((char[]){27, '[', '1', ';', '5', final}, 6);
    if (shift)
        return gfx_write((char[]){27, '[', '1', ';', '2', final}, 6);
    if (alt)
        return gfx_write((char[]){27, '[', '1', ';', '3', final}, 6);

    if (IS_SET(gfx->frame.term_snapshot.term_mode, MODE_APP_CURSOR_KEYS))
        return gfx_write((char[]){27, 'O', final}, 3);

    return gfx_write((char[]){27, '[', final}, 3);
}

static inline void clipboard_copy(void)
{
    char *buffer = selection_get_text();
    if (!buffer)
        return;

    SDL_SetClipboardText(buffer);
    free(buffer);
    selection_clear();
}

static inline ssize_t clipboard_paste(void)
{
    char *text = SDL_GetClipboardText();
    if (!text)
        return -1;

    bool bracketed_mode =
        IS_SET(gfx->frame.term_snapshot.term_mode, MODE_BRACKETED_PASTE);
    if (bracketed_mode)
        gfx_write("\x1b[200~", 6);

    size_t len = strlen(text);
    ssize_t n  = gfx_write(text, len);

    if (bracketed_mode)
        gfx_write("\x1b[201~", 6);

    SDL_free(text);
    return n;
}

void keydown(const SDL_KeyboardEvent *key, const Config *config)
{
    bool ctrl  = IS_SET_ANY(key->keysym.mod, KMOD_CTRL),
         shift = IS_SET_ANY(key->keysym.mod, KMOD_SHIFT),
         alt   = IS_SET_ANY(key->keysym.mod, KMOD_ALT);

    switch (key->keysym.sym) {
    case SDLK_a: goto mod_put;
    case SDLK_b: goto mod_put;
    case SDLK_c: {
        if (ctrl && shift)
            clipboard_copy();
        else
            goto mod_put;
    } break;
    case SDLK_d: goto mod_put;
    case SDLK_e: goto mod_put;
    case SDLK_f: goto mod_put;
    case SDLK_g: goto mod_put;
    case SDLK_h: goto mod_put;
    case SDLK_i: goto mod_put;
    case SDLK_j: goto mod_put;
    case SDLK_k: goto mod_put;
    case SDLK_l: goto mod_put;
    case SDLK_m: goto mod_put;
    case SDLK_n: goto mod_put;
    case SDLK_o: goto mod_put;
    case SDLK_p: goto mod_put;
    case SDLK_q: goto mod_put;
    case SDLK_r: goto mod_put;
    case SDLK_s: goto mod_put;
    case SDLK_t: goto mod_put;
    case SDLK_u: goto mod_put;
    case SDLK_v: {
        if (ctrl && shift)
            clipboard_paste();
        else
            goto mod_put;
    } break;
    case SDLK_w: goto mod_put;
    case SDLK_x: goto mod_put;
    case SDLK_y: goto mod_put;
    case SDLK_z: {
    mod_put:
        if (ctrl)
            gfx_write((char[]){key->keysym.sym - 'a' + 1}, 1);
        else if (alt)
            gfx_write((char[]){0x1b, key->keysym.sym}, 2);
    } break;

    case SDLK_0: // fallthrough
    case SDLK_KP_0:
        if (ctrl) {
            if (f_delta != 0) {
                f_delta = 0;
                goto resize_font;
            }
        }
        break;
    case SDLK_EQUALS: // fallthrough
    case SDLK_KP_EQUALS:
        if (ctrl) {
            f_delta++;
            goto resize_font;
        }
        break;
    case SDLK_MINUS: // fallthrough
    case SDLK_KP_MINUS:
        if (ctrl) {
            f_delta = MAX(1 - config->font_size, f_delta - 1);
            goto resize_font;
        }
        break;
    resize_font: {
        Dpi dpi;
        SDL_GetDisplayDPI(0, &dpi.d, &dpi.h, &dpi.v);
        uint hdpi = lroundf(dpi.h), vdpi = lroundf(dpi.v);

        int size = config->font_size + f_delta;
        TTF_SetFontSizeDPI(gfx->fonts[FontRegular], size, hdpi, vdpi);
        TTF_SetFontSizeDPI(gfx->fonts[FontBold], size, hdpi, vdpi);
        TTF_SetFontSizeDPI(gfx->fonts[FontItalic], size, hdpi, vdpi);
        TTF_SetFontSizeDPI(gfx->fonts[FontBoldItalic], size, hdpi, vdpi);
        gfx_rebuild(config);
    } break;

        // clang-format off
    case SDLK_LEFTBRACKET: {
        if      (alt && ctrl) gfx_write("\x1b\x1b[",  3);
        else if (alt)         gfx_write("\x1b[",      2);
        else if (ctrl)        gfx_write("\x1b",       1);
    } break;
    case SDLK_RIGHTBRACKET: {
        if      (alt && ctrl) gfx_write("\x1b\x1b]",  3);
        else if (alt)         gfx_write("\x1b]",      2);
        else if (ctrl)        gfx_write("\x1d",       1);
    } break;
    case SDLK_F1:  gfx_write("\x1bOP",    3); break;
    case SDLK_F2:  gfx_write("\x1bOQ",    3); break;
    case SDLK_F3:  gfx_write("\x1bOR",    3); break;
    case SDLK_F4:  gfx_write("\x1bOS",    3); break;
    case SDLK_F5:  gfx_write("\x1b[15~",  5); break;
    case SDLK_F6:  gfx_write("\x1b[17~",  5); break;
    case SDLK_F7:  gfx_write("\x1b[18~",  5); break;
    case SDLK_F8:  gfx_write("\x1b[19~",  5); break;
    case SDLK_F9:  gfx_write("\x1b[20~",  5); break;
    case SDLK_F10: gfx_write("\x1b[21~",  5); break;
    case SDLK_F11: gfx_write("\x1b[23~",  5); break;
    case SDLK_F12: gfx_write("\x1b[24~",  5); break;

    case SDLK_RETURN:    // fallthrough
    case SDLK_RETURN2:   gfx_write("\r", 1);     break;
    case SDLK_TAB:       gfx_write("\t", 1);     break;
    case SDLK_BACKSPACE: gfx_write("\b", 1);     break;
    case SDLK_ESCAPE:    gfx_write("\x1b", 1);   break;

    case SDLK_UP:        send_arrow_up(key->keysym.mod); break;
    case SDLK_DOWN:      send_arrow_down(key->keysym.mod); break;
    case SDLK_RIGHT:     send_arrow_right(key->keysym.mod); break;
    case SDLK_LEFT:      send_arrow_left(key->keysym.mod); break;

    case SDLK_HOME:      gfx_write("\x1b[H", 3); break;
    case SDLK_END:       gfx_write("\x1b[F", 3); break;
    case SDLK_INSERT: {
        shift ? clipboard_paste() : gfx_write("\x1b[2~", 4);
    } break;
    case SDLK_DELETE:    gfx_write("\x1b[3~", 4); break;
    case SDLK_PAGEUP:    gfx_write("\x1b[5~", 4); break;
    case SDLK_PAGEDOWN:  gfx_write("\x1b[6~", 4); break;
        // clang-format on
    default: break;
    }
}
