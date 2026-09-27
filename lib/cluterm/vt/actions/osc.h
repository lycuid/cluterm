#ifndef __CLUTERM__VT__ACTIONS__OSC_H__
#define __CLUTERM__VT__ACTIONS__OSC_H__

#include <cluterm.h>
#include <cluterm/colors.h>
#include <include/cluterm/debug.h>

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
    case OSC_4: {
        term->theme.palette[index] = color;
        dirty_buffer(active_buffer(term));
    } break;
    case OSC_10: {
        term->theme.fg = color;
        dirty_buffer(active_buffer(term));
    } break;
    case OSC_11: {
        term->theme.bg = color;
        dirty_buffer(active_buffer(term));
    } break;
    case OSC_12: {
        term->theme.cursor = color;
        dirty_buffer(active_buffer(term));
    } break;
    default: debug_2("osc action unsupported: '%d'.\n", action);
    }
    return 1;
}

#define RRGGBB(i)                                                              \
    ((i) >> 16) & 0xff, ((i) >> 16) & 0xff, ((i) >> 8) & 0xff,                 \
        ((i) >> 8) & 0xff, (i) & 0xff, (i) & 0xff

static inline void osc_query(Cluterm *term, OSC_Action action, int index)
{
    const Theme *theme  = &term->theme;
    char osc_string[64] = {0};
    int len             = 0;

    switch (action) {
    case OSC_4: {
        len =
            sprintf(osc_string, "\x1b]4;%d;rgb:%02x%02x/%02x%02x/%02x%02x\x07",
                    index, RRGGBB(theme->palette[index]));
    } break;
    case OSC_10: {
        len = sprintf(osc_string, "\x1b]10;rgb:%02x%02x/%02x%02x/%02x%02x\x07",
                      RRGGBB(theme->fg));
    } break;
    case OSC_11: {
        len = sprintf(osc_string, "\x1b]11;rgb:%02x%02x/%02x%02x/%02x%02x\x07",
                      RRGGBB(theme->bg));
    } break;
    case OSC_12: {
        len = sprintf(osc_string, "\x1b]11;rgb:%02x%02x/%02x%02x/%02x%02x\x07",
                      RRGGBB(theme->bg));
    } break;
    default: break;
    }
    if (len)
        term->actions.send(osc_string, len);
}
#undef RRGGBB

static inline bool osc_color(Cluterm *term, OSC_Action action, Scanner *s)
{
    switch (action) {
    case OSC_10: // fallthrough
    case OSC_11: // fallthrough
    case OSC_12: break;
    default: return true;
    }

    if (!s_consume(s, ';'))
        return false;

    bool ok = true;
    if (s_consume(s, '?'))
        osc_query(term, action, 0);
    else
        ok = osc_set_color(term, action, 0, s);

    if (ok && s_buflen(s))
        ok = osc_color(term, action + 1, s);
    return ok;
}

EXPORT inline void osc_execute(Cluterm *term, OSC_Payload *osc)
{
    Scanner *s = &osc->scanner;
    debug_2("OSC %d%s.\n", osc->action, s_buffer(s));

    switch (osc->action) {
    case OSC_0: // fallthrough
    case OSC_2: {
        if (term->actions.set_window_title) {
            if (!s_consume(s, ';'))
                break;
            char *title = calloc(s_buflen(s) + 1, sizeof(char));
            memcpy(title, s_buffer(s), s_buflen(s));
            term->actions.set_window_title(title);
            free(title);
        }
    } break;

    case OSC_4: {
        if (!s_consume(s, ';'))
            break;

        bool ok = 1;
        do {
            int index = s_consume_number(s);
            if (!BETWEEN(index, 0, 255) || !s_consume(s, ';'))
                break;

            if (s_consume(s, '?')) {
                osc_query(term, osc->action, index);
            } else {
                ok = osc_set_color(term, osc->action, index, s);
            }
        } while (s_consume(s, ';') && ok);

        if (s_buflen(s))
            debug_2("Invalid osc string '%s'.\n", s->buffer);
    } break;

    case OSC_7: break; // Not supported!.

    case OSC_10:
    case OSC_11:
    case OSC_12: {
        if (!osc_color(term, osc->action, s))
            debug_2("Unable to execute osc %d %s", osc->action, s->buffer);

        if (s_buflen(s))
            debug_2("Invalid osc string '%s'.\n", s->buffer);
    } break;

    case OSC_104: {
        if (!s_consume(s, ';')) {
            memcpy(&term->theme.palette, &term->config.theme.palette,
                   sizeof(term->theme.palette));
            break;
        }

        do {
            int index = s_consume_number(s);
            if (!BETWEEN(index, 0, 255))
                break;
            term->theme.palette[index] = term->config.theme.palette[index];
        } while (s_consume(s, ';'));

        if (s_buflen(s))
            debug_2("Invalid osc string '%s'.\n", s->buffer);
    } break;

    case OSC_110: {
        term->theme.fg = term->config.theme.fg;
        dirty_buffer(active_buffer(term));
    } break;

    case OSC_111: {
        term->theme.bg = term->config.theme.bg;
        dirty_buffer(active_buffer(term));
    } break;

    case OSC_112: {
        term->theme.cursor = term->config.theme.cursor;
        dirty_buffer(active_buffer(term));
    } break;

    case OSC_UNKNOWN: break;
    }

    if (s_buflen(s))
        debug_2("osc action unsupported: '%d' (%s).\n", osc->action, s->buffer);
}

#endif
