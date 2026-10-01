#include "keypress.h"
#include "main.h"

static int f_delta = 0;

static inline void clipboard_copy(void)
{
    /* char *buffer = selection_get_text(); */
    /* if (!buffer) */
    /*     return; */
    /*  */
    /* SDL_SetClipboardText(buffer); */
    /* free(buffer); */
    /* selection_clear(); */
}

static inline ssize_t clipboard_paste(void)
{
    /* char *text = SDL_GetClipboardText(); */
    /* if (!text) */
    /*     return -1; */
    /* ssize_t n = cluterm_paste(term, text); */
    /* SDL_free(text); */
    /* return n; */
    return 0;
}

void handle_keydown(const SDL_KeyboardEvent *key, const Config *config)
{
    bool ctrl  = IS_SET_ANY(key->keysym.mod, KMOD_CTRL),
         shift = IS_SET_ANY(key->keysym.mod, KMOD_SHIFT);

    switch (key->keysym.sym) {
    case SDLK_c: {
        if (ctrl && shift)
            clipboard_copy();
    } break;
    case SDLK_v: {
        if (ctrl && shift)
            clipboard_paste();
    } break;
    case SDLK_INSERT: {
        if (shift)
            clipboard_paste();
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
        gfx_rebuild();
    } break;
    default: break;
    }
}
