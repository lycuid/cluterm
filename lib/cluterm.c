#include "cluterm.h"
#include <cluterm/pty.h>
#include <cluterm/vt/actions.h>
#include <cluterm/vt/actions/csi.h>
#include <cluterm/vt/actions/ctrl.h>
#include <cluterm/vt/actions/esc.h>
#include <cluterm/vt/actions/osc.h>
#include <unistd.h>

static inline Rgb color256(uint8_t n)
{
    static const int cube[] = {0x00, 0x5f, 0x87, 0xaf, 0xd7, 0xff};

    Rgb color = 0;
    if (n <= 15)
        color = DefaultTheme.palette[n];
    else if (BETWEEN(n, 16, 231))
        for (int i = 0, m = n - 16; m; m /= 6)
            color |= cube[m % 6] << (8 * i++);
    else if (n >= 232)
        n = (n - 232) * 10 + 8, color = (n << 16) | (n << 8) | n;
    return color;
}

void cluterm_init(Cluterm *term)
{
    term->config.title        = Title;
    term->config.rows         = Rows;
    term->config.cols         = Columns;
    term->config.tab_width    = TabWidth;
    term->config.padding      = Padding;
    term->config.font_family  = FontFamily;
    term->config.font_size    = FontSize;
    term->config.cursor_color = DefaultCursorColor;
    term->config.cursor_style = DefaultCursorStyle;
    term->config.cursor_shape = DefaultCursorShape;

    term->config.theme.fg = DefaultTheme.fg;
    term->config.theme.bg = DefaultTheme.bg;
    for (size_t i = 0; i <= 255; ++i)
        term->config.theme.palette[i] = color256(i);

    parser_init(&term->vt_parser);
    term->mouse_report = (MouseReport){0};
    term->mode         = 0x0;
    memset(&term->actions, 0, sizeof(term->actions));
}

void cluterm_start(Cluterm *term, char *const *cmd)
{
    buffer_init(&term->buffer[0], &term->config);
    buffer_init(&term->buffer[1], &term->config);

    memcpy(&term->theme, &term->config.theme, sizeof(Theme));

    pty_open(&term->pty);
    pty_spawn(&term->pty, cmd);
    pty_resize(&term->pty, term->config.rows, term->config.cols);
}

void cluterm_write(Cluterm *term, uchar *stream, size_t slen)
{
    VT_Parser *vt_parser = &term->vt_parser;
    parser_feed(vt_parser, stream, slen);

#if DEBUG_LVL >= 2 // {{{
    if (slen) {
        debug_2("stream:");
        for (size_t i = 0; i < slen; ++i)
            if (BETWEEN(stream[i], 32, 126))
                debug(" %c", stream[i]);
            else
                debug(" %d", stream[i]);
        debug("\n");
    }
#endif // }}}

    for (FSM_Event fsm_event;;) {
        switch (fsm_event = parser_run(vt_parser)) {
        case EVENT_NOOP: goto done;
        case EVENT_PRINT: {
            ClutermBuffer *b = ACTIVE_BUFFER(term);
            insert_cell(b, CELL(vt_parser->payload.value, b->cell_attrs));
        } break;
        case EVENT_ESC: esc_execute(term, &vt_parser->payload.esc); break;
        case EVENT_CSI: csi_execute(term, &vt_parser->payload.csi); break;
        case EVENT_CTRL: ctrl_execute(term, &vt_parser->payload.ctrl); break;
        case EVENT_OSC: osc_execute(term, &vt_parser->payload.osc); break;
        case EVENT_DCS: {
            // @TODO: unimplemented.
        } break;
        }
    }
done:
    return;
}

void cluterm_resize(Cluterm *term, int rows, int cols)
{
    ClutermBuffer *b = ACTIVE_BUFFER(term);
    if (b->rows != rows || b->cols != cols) {
        pty_resize(&term->pty, rows, cols);
        buffer_resize(&term->buffer[0], &term->config, rows, cols);
        buffer_resize(&term->buffer[1], &term->config, rows, cols);
    }
}

void cluterm_destroy(Cluterm *term)
{
    pty_destroy(&term->pty);
    buffer_destroy(&term->buffer[0]);
    buffer_destroy(&term->buffer[1]);
}
// vim:fdm=marker
