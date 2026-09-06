#include "keypress.h"
#include "main.h"

static int f_delta = 0;

static inline ssize_t clipboard_paste(const Cluterm *term)
{
    char *text = SDL_GetClipboardText();
    if (!text)
        return -1;

    if (IS_SET(term->mode, MODE_BRACKETED_PASTE))
        pty_write(&term->pty, "\x1b[200~", 6);

    size_t len = strlen(text);
    ssize_t n  = pty_write(&term->pty, text, len);

    if (IS_SET(term->mode, MODE_BRACKETED_PASTE))
        pty_write(&term->pty, "\x1b[201~", 6);

    SDL_free(text);
    return n;
}

void handle_keydown(Cluterm *term, SDL_KeyboardEvent *key)
{
    bool ctrl  = IS_SET_ANY(key->keysym.mod, KMOD_CTRL),
         shift = IS_SET_ANY(key->keysym.mod, KMOD_SHIFT),
         alt   = IS_SET_ANY(key->keysym.mod, KMOD_ALT);

    switch (key->keysym.sym) {
    case SDLK_a: goto mod_put;
    case SDLK_b: goto mod_put;
    case SDLK_c: goto mod_put;
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
            clipboard_paste(term);
        else
            goto mod_put;
    } break;
    case SDLK_w: goto mod_put;
    case SDLK_x: goto mod_put;
    case SDLK_y: goto mod_put;
    case SDLK_z: {
    mod_put:
        if (ctrl)
            pty_write(&term->pty, (char[]){key->keysym.sym - 'a' + 1}, 1);
        else if (alt)
            pty_write(&term->pty, (char[]){0x1b, key->keysym.sym}, 2);
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
            f_delta = MAX(1 - term->config.font_size, f_delta - 1);
            goto resize_font;
        }
        break;
    resize_font: {
        int size  = term->config.font_size + f_delta;
        uint hdpi = lroundf(gfx->hdpi), vdpi = lroundf(gfx->vdpi);
        TTF_SetFontSizeDPI(gfx->fonts[FontRegular], size, hdpi, vdpi);
        TTF_SetFontSizeDPI(gfx->fonts[FontBold], size, hdpi, vdpi);
        TTF_SetFontSizeDPI(gfx->fonts[FontItalic], size, hdpi, vdpi);
        TTF_SetFontSizeDPI(gfx->fonts[FontBoldItalic], size, hdpi, vdpi);
        gfx_rebuild(term);
        gfx_request_render(1);
    } break;

        // clang-format off
    case SDLK_LEFTBRACKET: {
        if      (alt && ctrl) pty_write(&term->pty, "\x1b\x1b[",  3);
        else if (alt)         pty_write(&term->pty, "\x1b[",      2);
        else if (ctrl)        pty_write(&term->pty, "\x1b",       1);
    } break;
    case SDLK_RIGHTBRACKET: {
        if      (alt && ctrl) pty_write(&term->pty, "\x1b\x1b]",  3);
        else if (alt)         pty_write(&term->pty, "\x1b]",      2);
        else if (ctrl)        pty_write(&term->pty, "\x1d",       1);
    } break;
    case SDLK_F1:  pty_write(&term->pty, "\x1bOP",    3); break;
    case SDLK_F2:  pty_write(&term->pty, "\x1bOQ",    3); break;
    case SDLK_F3:  pty_write(&term->pty, "\x1bOR",    3); break;
    case SDLK_F4:  pty_write(&term->pty, "\x1bOS",    3); break;
    case SDLK_F5:  pty_write(&term->pty, "\x1b[15~",  5); break;
    case SDLK_F6:  pty_write(&term->pty, "\x1b[17~",  5); break;
    case SDLK_F7:  pty_write(&term->pty, "\x1b[18~",  5); break;
    case SDLK_F8:  pty_write(&term->pty, "\x1b[19~",  5); break;
    case SDLK_F9:  pty_write(&term->pty, "\x1b[20~",  5); break;
    case SDLK_F10: pty_write(&term->pty, "\x1b[21~",  5); break;
    case SDLK_F11: pty_write(&term->pty, "\x1b[23~",  5); break;
    case SDLK_F12: pty_write(&term->pty, "\x1b[24~",  5); break;

    case SDLK_RETURN:    // fallthrough
    case SDLK_RETURN2:   pty_write(&term->pty, "\r", 1);     break;
    case SDLK_TAB:       pty_write(&term->pty, "\t", 1);     break;
    case SDLK_BACKSPACE: pty_write(&term->pty, "\b", 1);     break;
    case SDLK_ESCAPE:    pty_write(&term->pty, "\x1b", 1);   break;

#define pty_write_arrow(final)                                                 \
    do {                                                                       \
        if (ctrl && shift && alt)                                              \
            pty_write(&term->pty, "\x1b[1;8" final, 6);                        \
        else if (alt && ctrl)                                                  \
            pty_write(&term->pty, "\x1b[1;7" final, 6);                        \
        else if (ctrl && shift)                                                \
            pty_write(&term->pty, "\x1b[1;6" final, 6);                        \
        else if (shift && alt)                                                 \
            pty_write(&term->pty, "\x1b[1;4" final, 6);                        \
        else if (ctrl)                                                         \
            pty_write(&term->pty, "\x1b[1;5" final, 6);                        \
        else if (shift)                                                        \
            pty_write(&term->pty, "\x1b[1;2" final, 6);                        \
        else if (alt)                                                          \
            pty_write(&term->pty, "\x1b[1;3" final, 6);                        \
        else                                                                   \
            pty_write(&term->pty, "\x1b[" final, 3);                           \
    } while (0)

    case SDLK_UP:        pty_write_arrow("A"); break;
    case SDLK_DOWN:      pty_write_arrow("B"); break;
    case SDLK_RIGHT:     pty_write_arrow("C"); break;
    case SDLK_LEFT:      pty_write_arrow("D"); break;
#undef pty_write_arrow

    case SDLK_HOME:      pty_write(&term->pty, "\x1b[H", 3); break;
    case SDLK_END:       pty_write(&term->pty, "\x1b[F", 3); break;
    case SDLK_INSERT: {
        shift ? clipboard_paste(term) : pty_write(&term->pty, "\x1b[2~", 4);
    } break;
    case SDLK_DELETE:    pty_write(&term->pty, "\x1b[3~", 4); break;
    case SDLK_PAGEUP:    pty_write(&term->pty, "\x1b[5~", 4); break;
    case SDLK_PAGEDOWN:  pty_write(&term->pty, "\x1b[6~", 4); break;
        // clang-format on
    default: break;
    }
}
