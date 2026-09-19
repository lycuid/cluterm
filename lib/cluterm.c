#include "cluterm.h"
#include <cluterm/vt/actions.h>
#include <cluterm/vt/actions/csi.h>
#include <cluterm/vt/actions/ctrl.h>
#include <cluterm/vt/actions/esc.h>
#include <cluterm/vt/actions/osc.h>
#include <unistd.h>

void cluterm_init(Cluterm *term, const Config *config)
{
    memcpy(&term->config, config, sizeof(Config));
    parser_init(&term->vt_parser);
    term->mouse_report = (MouseReport){0};
    term->mode         = MODE_ALT_SCROLL;
    memset(&term->actions, 0, sizeof(term->actions));

    buffer_init(&term->buffer[0], &term->config);
    buffer_init(&term->buffer[1], &term->config);

    memcpy(&term->theme, &term->config.theme, sizeof(Theme));
}

void cluterm_feed(Cluterm *term, const uchar *stream, size_t slen)
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

static inline void snapshot_resize(ClutermSnapshot *snap, int rows, int cols)
{
    Line *ll = malloc(rows * sizeof(Line));
    for (int y = 0; y < rows; ++y)
        ll[y] = malloc(cols * sizeof(Cell));

    if (snap->lines) {
        for (int y = 0; y < snap->rows; ++y)
            free(snap->lines[y]);
        free(snap->lines);
    }
    snap->rows = rows, snap->cols = cols, snap->lines = ll;
    snap->dirty = realloc(snap->dirty, snap->rows * snap->cols * sizeof(bool));
}

void cluterm_snapshot(Cluterm *term, ClutermSnapshot *snap)
{
    ClutermBuffer *b = ACTIVE_BUFFER(term);

    if (b->rows != snap->rows || b->cols != snap->cols)
        snapshot_resize(snap, b->rows, b->cols);

    snap->term_mode = term->mode;
    for (int y = 0; y < b->rows; ++y)
        memcpy(snap->lines[y], line_at(b, y), b->cols * sizeof(*b->lines[y]));

    memcpy(&snap->cursor, &b->cursor, sizeof(Cursor));
    memmove(snap->dirty, b->dirty, b->cols * b->rows * sizeof(*b->dirty));
    memset(b->dirty, 0, b->rows * b->cols * sizeof(*b->dirty));
    memcpy(&snap->theme, &term->theme, sizeof(Theme));
}

void cluterm_resize(Cluterm *term, int rows, int cols)
{
    ClutermBuffer *b = ACTIVE_BUFFER(term);
    if (b->rows != rows || b->cols != cols) {
        buffer_resize(&term->buffer[0], &term->config, rows, cols);
        buffer_resize(&term->buffer[1], &term->config, rows, cols);
    }
}

void cluterm_destroy(Cluterm *term)
{
    buffer_destroy(&term->buffer[0]);
    buffer_destroy(&term->buffer[1]);
}
// vim:fdm=marker
