#include "cluterm.h"
#include <cluterm/config.h>
#include <cluterm/pty.h>
#include <cluterm/vt/actions.h>
#include <cluterm/vt/actions/csi.h>
#include <cluterm/vt/actions/ctrl.h>
#include <cluterm/vt/actions/esc.h>
#include <unistd.h>

void cluterm_init(Cluterm *term, char *const *cmd)
{
    {
        buffer_init(&term->buffer[0], cfg->rows, cfg->cols, 0); // primary.
        buffer_init(&term->buffer[1], cfg->rows, cfg->cols, 0); // alt.
    }
    parser_init(&term->vt_parser);
    {
        pty_open(&term->pty);
        pty_spawn(&term->pty, cmd);
    }
    term->mode = 0x0, term->fg = cfg->fg, term->bg = cfg->bg,
    term->osc_handler = NULL;
}

void cluterm_write(Cluterm *term, uchar *stream, uint32_t slen)
{
    VT_Parser *vt_parser = &term->vt_parser;
    parser_feed(vt_parser, stream, slen);

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
        case EVENT_OSC: {
            if (term->osc_handler)
                term->osc_handler(term, &vt_parser->payload.osc);
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
        buffer_resize(&term->buffer[0], rows, cols);
        buffer_resize(&term->buffer[1], rows, cols);
    }
}

void cluterm_destroy(Cluterm *term)
{
    pty_destroy(&term->pty);
    buffer_destroy(&term->buffer[0]);
    buffer_destroy(&term->buffer[1]);
}
