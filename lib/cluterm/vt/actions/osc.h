#ifndef __CLUTERM__VT__ACTIONS__OSC_H__
#define __CLUTERM__VT__ACTIONS__OSC_H__

#include <cluterm.h>
#include <cluterm/colors.h>
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
    case OSC_4: {
        term->theme.palette[index] = color;
        dirty_buffer(ACTIVE_BUFFER(term));
    } break;
    case OSC_10: {
        term->theme.fg = color;
        dirty_buffer(ACTIVE_BUFFER(term));
    } break;
    case OSC_11: {
        term->theme.bg = color;
        dirty_buffer(ACTIVE_BUFFER(term));
    } break;
    case OSC_12: {
        term->buffer[0].cursor.color = color;
        term->buffer[1].cursor.color = color;
        dirty_buffer(ACTIVE_BUFFER(term));
    } break;
    default: debug_2("osc action unsupported: '%d'.\n", action);
    }
    return 1;
}

static inline bool osc_color(Cluterm *term, OSC_Action action, Scanner *s)
{
    void (*query)(const Cluterm *) = NULL;
    switch (action) {
    case OSC_10: query = term->actions.query_palette_fg; break;
    case OSC_11: query = term->actions.query_palette_bg; break;
    case OSC_12: query = term->actions.query_cursor_color; break;
    default: return true;
    }

    if (!s_consume(s, ';'))
        return false;

    bool ok = true;
    if (!s_consume(s, '?'))
        ok = osc_set_color(term, action, 0, s);
    else if (query)
        query(term);

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
            term->actions.set_window_title(term, title);
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
                if (term->actions.query_palette_index)
                    term->actions.query_palette_index(term, index);
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
            memcpy(&term->theme, &term->config.theme, sizeof(Theme));
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
        term->theme.fg = DefaultTheme.fg;
        dirty_buffer(ACTIVE_BUFFER(term));
    } break;

    case OSC_111: {
        term->theme.bg = DefaultTheme.bg;
        dirty_buffer(ACTIVE_BUFFER(term));
    } break;

    case OSC_112: {
        ClutermBuffer *b = ACTIVE_BUFFER(term);
        b->cursor.color  = DefaultCursorColor;
        dirty_buffer(b);
    } break;

    case OSC_UNKNOWN: break;
    }

    if (s_buflen(s))
        debug_2("osc action unsupported: '%d' (%s).\n", osc->action, s->buffer);
}

#endif
