#include "osc_handler.h"
#include "SDL_events.h"
#include "main.h"
#include <SDL2/SDL.h>
#include <cluterm/colors.h>

static inline int rgb_component(Scanner *s, uint8_t *comp)
{
    uint32_t color = 0, digits = 0;
    for (; s_peek(s) && IS_HEX(*s_peek(s)); ++digits) {
        color = color * 16 + hex[s_next(s)];
    }

    if (!BETWEEN(digits, 1, 4))
        return -1;
    *comp = color * 255 / ((1u << (digits * 4)) - 1);

    return 1;
}

static inline int parse_color(Scanner *s, Rgb *color)
{
    // rgb:n/n/n | rgb:nn/nn/nn | rgb:nnn/nnn/nnn | rgb:nnnn/nnnn/nnnn
    if (s_consume_string(s, "rgb:", 4)) {
        uint8_t r = 0, g = 0, b = 0;
        if (!rgb_component(s, &r))
            return -1;
        if (!s_consume(s, '/'))
            return -1;
        if (!rgb_component(s, &g))
            return -1;
        if (!s_consume(s, '/'))
            return -1;
        if (!rgb_component(s, &b))
            return -1;
        *color = RGB(r, g, b);
        return 1;
    }

    // #nnnnnn
    return parse_rgb(s, color);
}

static inline int osc_set_color(Cluterm *term, OSC_Action action, Scanner *s)
{
    Rgb color = 0;
    if (!parse_color(s, &color))
        return -1;

    switch (action) {
    case OSC_10: term->fg = color; break;
    case OSC_11: term->bg = color; break;
    case OSC_12: {
        term->buffer[0].cursor.color = color;
        term->buffer[1].cursor.color = color;
    } break;
    default: return 0;
    }
    return 1;
}

static inline int osc_query(Cluterm *term, OSC_Action action)
{
    char osc_color[36]     = {0};
    const ClutermBuffer *b = ACTIVE_BUFFER(term);

    switch (action) {
#define fill(index, ...)                                                       \
    sprintf(osc_color, "\x1b]" index ";rgb:%02x/%02x/%02x\x07", __VA_ARGS__);

        // handler only called for queries, not for updating dynamic color.
    case OSC_10: fill("10", UNPACK(term->fg)); goto send_cmd;
    case OSC_11: fill("11", UNPACK(term->bg)); goto send_cmd;
    case OSC_12: fill("12", UNPACK(b->cursor.color));
#undef fill
    send_cmd: {
        pty_write(&term->pty, osc_color, strlen(osc_color));
    } break;

    default: return 0;
    }
    return 1;
}

void osc_handler(Cluterm *term, OSC_Payload *osc)
{
    Scanner *s = &osc->scanner;

    switch (osc->action) {
    case OSC_0: // fallthrough
    case OSC_2: {
        // safe to malloc/free, as this is probably not gonna be frequent.
        char *title = calloc(s_buflen(s) + 1, sizeof(char));
        memcpy(title, s_buffer(s), s_buflen(s));
        SDL_Event e = {.user = {.type  = SDL_USEREVENT,
                                .data1 = title,
                                .code  = USEREVENT_SET_TITLE}};
        if (SDL_PushEvent(&e) < 0)
            free(title);
    } break;
    case OSC_7: break; // Not supported!.

    case OSC_10: // fallthrough
    case OSC_11: // fallthrough
    case OSC_12: {
        OSC_Action action = osc->action;
        int result;
        do {
            result          = 1;
            const uchar *ch = s_peek(s);
            if (ch && *ch == '?') {
                result = osc_query(term, action);
            } else {
                result = osc_set_color(term, action, s);
            }
            if (result == -1)
                debug_2("Invalid osc string '%s'.\n", s->buffer);
            action++;
        } while (s_consume(s, ';') && result);
    } break;
    case OSC_UNKNOWN: break;
    }
}
