#include "cluterm.h"
#include <cluterm/actions.h>
#include <cluterm/actions/csi.h>
#include <cluterm/actions/ctrl.h>
#include <cluterm/actions/esc.h>
#include <cluterm/actions/osc.h>
#include <cluterm/vte/tty.h>
#include <stdlib.h>
#include <unistd.h>

void cluterm_init(CluTerm *term)
{
    {
        buffer_init(&term->buffer[0], Rows, Columns, 0); // primary.
        buffer_init(&term->buffer[1], Rows, Columns, 0); // alt.
        term->saved_cursor[0] = term->buffer[0].cursor;
        term->saved_cursor[1] = term->buffer[1].cursor;
        term->tab             = (bool *)calloc(Columns + 1, sizeof(bool));
        for (int i = TabWidth; i <= Columns; i += TabWidth)
            term->tab[i] = true;
    }
    parser_init(&term->vt_parser);
    {
        tty_init(&term->tty);
        setenv("TERM", "st-256color", 1);
        char *shell = getenv("SHELL");
        if (!shell)
            shell = "/bin/bash";
        tty_start(&term->tty, shell);
    }
}

void cluterm_write(CluTerm *term, char *stream, uint32_t slen)
{
    VT_Parser *vt_parser = &term->vt_parser;
    CluTermBuffer *b     = ACTIVE_BUFFER(term);
    parser_feed(vt_parser, stream, slen);
    for (FSM_Event fsm_event;;) {
        switch (fsm_event = parser_run(vt_parser)) {
        case EVENT_NOOP: {
            goto DONE;
        }
        case EVENT_PRINT: {
            put_cell(term, CELL(vt_parser->payload.value, b->cell_attrs));
        } break;
        case EVENT_CTRL: {
            ctrl_perform_action(term, &vt_parser->payload.ctrl);
        } break;
        case EVENT_ESC: {
            esc_perform_action(term, &vt_parser->payload.esc);
        } break;
        case EVENT_CSI: {
            csi_perform_action(term, &vt_parser->payload.csi);
        } break;
        case EVENT_OSC: {
            osc_perform_action(term, &vt_parser->payload.osc);
        } break;
        }
    }
DONE:
    return;
}

void cluterm_resize(CluTerm *term, int rows, int cols)
{
    CluTermBuffer *b = ACTIVE_BUFFER(term);
    if (b->rows != rows || b->cols != cols) {
        buffer_resize(&term->buffer[0], rows, cols);
        buffer_resize(&term->buffer[1], rows, cols);
        tty_resize(&term->tty, rows, cols);
    }
}

void cluterm_destroy(CluTerm *term)
{
    free(term->tab);
    tty_destroy(&term->tty);
    buffer_destroy(&term->buffer[0]);
    buffer_destroy(&term->buffer[1]);
}
