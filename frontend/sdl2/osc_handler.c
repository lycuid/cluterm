#include "osc_handler.h"
#include "main.h"
#include <SDL2/SDL.h>
#include <cluterm/colors.h>
#include <cluterm/config.h>
#include <config.h>

static inline bool rgb_component(Scanner *s, uint8_t *comp)
{
    uint32_t color = 0, digits = 0;
    for (; s_peek(s) && IS_HEX(*s_peek(s)); ++digits) {
        color = color * 16 + hex[s_next(s)];
    }

    if (!BETWEEN(digits, 1, 4))
        return 0;
    *comp = color * 255 / ((1u << (digits * 4)) - 1);

    return 1;
}

static inline bool parse_color(Scanner *s, Rgb *color)
{
    // rgb:n/n/n | rgb:nn/nn/nn | rgb:nnn/nnn/nnn | rgb:nnnn/nnnn/nnnn
    if (s_consume_string(s, "rgb:", 4)) {
        uint8_t r = 0, g = 0, b = 0;
        if (!rgb_component(s, &r))
            return 0;
        if (!s_consume(s, '/'))
            return 0;
        if (!rgb_component(s, &g))
            return 0;
        if (!s_consume(s, '/'))
            return 0;
        if (!rgb_component(s, &b))
            return 0;
        *color = RGB(r, g, b);
        return 1;
    }

    // #nnnnnn
    return parse_rgb(s, color);
}

static inline bool osc_set_color(Cluterm *term, OSC_Action action, int index,
                                 Scanner *s)
{
    Rgb color = 0;

    if (!parse_color(s, &color))
        return 0;

    switch (action) {
    case OSC_4:
        cfg->theme.palette[index] = color;
        gfx_request_render(1);
        break;
    case OSC_10:
        cfg->theme.fg = color;
        gfx_request_render(1);
        break;
    case OSC_11:
        cfg->theme.bg = color;
        gfx_request_render(1);
        break;
    case OSC_12: {
        term->buffer[0].cursor.color = color;
        term->buffer[1].cursor.color = color;
        gfx_request_render(1);
    } break;
    default: debug_2("osc action unsupported: '%d'.\n", action);
    }
    return 1;
}

static inline bool osc_query(Cluterm *term, OSC_Action action, int index)
{
    char osc_color[36]     = {0};
    const ClutermBuffer *b = ACTIVE_BUFFER(term);

    switch (action) {
    case OSC_4:
        sprintf(osc_color, "\x1b]4;%d;rgb:%02x/%02x/%02x\x07", index,
                UNPACK(cfg->theme.palette[index]));
        goto send_cmd;
    case OSC_10:
        sprintf(osc_color, "\x1b]10;rgb:%02x/%02x/%02x\x07",
                UNPACK(cfg->theme.fg));
        goto send_cmd;
    case OSC_11:
        sprintf(osc_color, "\x1b]11;rgb:%02x/%02x/%02x\x07",
                UNPACK(cfg->theme.bg));
        goto send_cmd;
    case OSC_12:
        sprintf(osc_color, "\x1b]12;rgb:%02x/%02x/%02x\x07",
                UNPACK(b->cursor.color));
#undef fill
    send_cmd: {
        pty_write(&term->pty, osc_color, strlen(osc_color));
    } break;

    default: debug_2("osc action unsupported: '%d'.\n", action);
    }
    return 1;
}

void osc_handler(Cluterm *term, OSC_Payload *osc)
{
    Scanner *s = &osc->scanner;

    switch (osc->action) {
    case OSC_0: // fallthrough
    case OSC_2: {
        if (!s_consume(s, ';'))
            break;
        // safe to malloc/free, as this is probably not gonna be frequent.
        char *title = calloc(s_buflen(s) + 1, sizeof(char));
        memcpy(title, s_buffer(s), s_buflen(s));
        SDL_SetWindowTitle(gfx->window, title);
        free(title);
    } break;

    case OSC_4: {
        if (!s_consume(s, ';'))
            break;
        bool ok = 1;
        do {
            int index = s_consume_number(s);
            if (!BETWEEN(index, 0, 255) || !s_consume(s, ';'))
                break;

            ok = (s_peek(s) && *s_peek(s) == '?')
                     ? osc_query(term, osc->action, index)
                     : osc_set_color(term, osc->action, index, s);
        } while (s_consume(s, ';') && ok);

        if (s_buflen(s))
            debug_2("Invalid osc string '%s'.\n", s->buffer);
    } break;

    case OSC_7: break; // Not supported!.

    case OSC_10: // fallthrough
    case OSC_11: // fallthrough
    case OSC_12: {
        if (!s_consume(s, ';'))
            break;

        OSC_Action action = osc->action;
        bool ok;
        do {
            ok = (s_peek(s) && *s_peek(s) == '?')
                     ? osc_query(term, action, 0)
                     : osc_set_color(term, action, 0, s);
            action++;
        } while (s_consume(s, ';') && ok);

        if (s_buflen(s))
            debug_2("Invalid osc string '%s'.\n", s->buffer);
    } break;

    case OSC_104: {
        if (!s_consume(s, ';')) {
            for (size_t i = 0; i < 256; ++i)
                cfg->theme.palette[i] = color256(i);
            break;
        }
        do {
            int index = s_consume_number(s);
            if (!BETWEEN(index, 0, 255))
                break;
            cfg->theme.palette[index] = color256(index);
        } while (s_consume(s, ';'));

        if (s_buflen(s))
            debug_2("Invalid osc string '%s'.\n", s->buffer);
    } break;

    case OSC_110: {
        cfg->theme.fg = DefaultTheme.fg;
        gfx_request_render(1);
    } break;

    case OSC_111: {
        cfg->theme.bg = DefaultTheme.bg;
        gfx_request_render(1);
    } break;

    case OSC_112: {
        ClutermBuffer *b = ACTIVE_BUFFER(term);
        b->cursor.color  = DefaultCursor.color;
        gfx_request_render(1);
    } break;

    case OSC_UNKNOWN: break;
    }
}
