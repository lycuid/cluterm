#include "cluterm.h"
#include <cluterm/actions.h>
#include <cluterm/actions/csi.h>
#include <cluterm/actions/ctrl.h>
#include <cluterm/actions/esc.h>
#include <cluterm/actions/osc.h>
#include <cluterm/pty.h>
#include <stdlib.h>
#include <unistd.h>

void cluterm_init(CluTerm *term)
{
    {
        buffer_init(&term->buffer[0], Rows, Columns, 0); // primary.
        buffer_init(&term->buffer[1], Rows, Columns, 0); // alt.
    }
    parser_init(&term->vt_parser);
    {
        pty_open(&term->pty);
        setenv("TERM", "st-256color", 1);
        char *shell = getenv("SHELL");
        if (!shell)
            shell = "/bin/bash";
        pty_spawn(&term->pty, shell);
    }
}

void cluterm_write(CluTerm *term, char *stream, uint32_t slen)
{
    VT_Parser *vt_parser = &term->vt_parser;
    parser_feed(vt_parser, stream, slen);

    for (FSM_Event fsm_event;;) {
        CluTermBuffer *b = ACTIVE_BUFFER(term);

        switch (fsm_event = parser_run(vt_parser)) {
        case EVENT_NOOP: goto done;
        case EVENT_PRINT: {
            put_cell(term, CELL(vt_parser->payload.value, b->cell_attrs));
        } break;
        case EVENT_ESC: esc_execute(term, &vt_parser->payload.esc); break;
        case EVENT_CSI: csi_execute(term, &vt_parser->payload.csi); break;
        case EVENT_CTRL: ctrl_execute(term, &vt_parser->payload.ctrl); break;
        case EVENT_OSC: osc_execute(term, &vt_parser->payload.osc); break;
        }
    }
done:
    return;
}

void cluterm_resize(CluTerm *term, int rows, int cols)
{
    CluTermBuffer *b = ACTIVE_BUFFER(term);
    if (b->rows != rows || b->cols != cols) {
        pty_resize(&term->pty, rows, cols);
        buffer_resize(&term->buffer[0], rows, cols);
        buffer_resize(&term->buffer[1], rows, cols);
    }
}

void cluterm_destroy(CluTerm *term)
{
    pty_destroy(&term->pty);
    buffer_destroy(&term->buffer[0]);
    buffer_destroy(&term->buffer[1]);
}
