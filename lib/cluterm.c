#include "cluterm.h"
#include <assert.h>
#include <cluterm/vt/actions.h>
#include <cluterm/vt/actions/csi.h>
#include <cluterm/vt/actions/ctrl.h>
#include <cluterm/vt/actions/esc.h>
#include <cluterm/vt/actions/osc.h>
#include <cluterm/vt/buffer_line.h>
#include <include/cluterm/debug.h>
#include <unistd.h>

Cluterm *cluterm_init(const Config *config)
{
#if DEBUG_LVL >= 2 // {{{
    // clang-format off
    debug("Config:\n");
    debug("    title:       '%s',\n", config->title);
    debug("    rows:        %d,\n", config->rows);
    debug("    cols:        %d,\n", config->cols);
    debug("    tab_width:   %d,\n", config->tab_width);
    debug("    padding:     {%d,%d,%d,%d},\n", config->padding.top, config->padding.right, config->padding.bottom, config->padding.left);
    debug("    font_family: '%s',\n", config->font_family);
    debug("    font_size:   %d,\n", config->font_size);
    // clang-format on
#endif // }}}

    Cluterm *term = calloc(1, sizeof(Cluterm));
    memcpy(&term->config, config, sizeof(Config));
    parser_init(&term->vt_parser);
    term->mouse_report = (MouseReport){0};
    term->mode         = MODE_ALT_SCROLL;
    term->render_flag  = 0;
    memset(&term->actions, 0, sizeof(term->actions));

    buffer_init(&term->buffer[0], 500, &term->config);
    buffer_init(&term->buffer[1], 0, &term->config);

    memcpy(&term->theme, &term->config.theme, sizeof(Theme));
    return term;
}

void cluterm_set_actions(Cluterm *term, ClutermActions actions)
{
    assert(term != NULL);
    assert(actions.send != NULL);
    term->actions = actions;
}

ssize_t cluterm_send(Cluterm *term, const char *buffer, size_t len)
{
    assert(term != NULL);
    selection_clear(term);
    ClutermBuffer *b = active_buffer(term);
    if (b->scroll_offset > 0)
        dirty_buffer(b);
    term->buffer[0].scroll_offset = 0;
    term->buffer[1].scroll_offset = 0;
    term->render_flag             = 1;
    return term->actions.send(buffer, len);
}

bool cluterm_frame_ready(Cluterm *term)
{
    assert(term != NULL);
    int ready_state   = term->render_flag;
    term->render_flag = 0;
    return ready_state;
}

void cluterm_feed(Cluterm *term, const uchar *stream, size_t slen)
{
    assert(term != NULL);
    VT_Parser *vt_parser = &term->vt_parser;

    parser_feed(vt_parser, stream, slen);
    for (FSM_Event fsm_event;;) {
        switch (fsm_event = parser_run(vt_parser)) {
        case EVENT_NOOP: goto done;
        case EVENT_PRINT: {
            ClutermBuffer *b = active_buffer(term);
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

bool cluterm_keydown(Cluterm *term, ClutermKey *key)
{
    assert(term != NULL);
    return keydown(term, key);
}

bool cluterm_mouse_button(Cluterm *term, ClutermMouse *mouse)
{
    assert(term != NULL);
    return handle_mouse_button(term, mouse);
}

bool cluterm_mouse_wheel(Cluterm *term, ClutermMouse *mouse)
{
    assert(term != NULL);
    return handle_mouse_wheel(term, mouse);
}

bool cluterm_mouse_motion(Cluterm *term, ClutermMouse *mouse)
{
    assert(term != NULL);
    return handle_mouse_motion(term, mouse);
}

static inline void snapshot_resize(ClutermSnapshot *snap, int rows, int cols)
{
    BufferLine *lines = malloc(rows * sizeof(BufferLine));
    for (int y = 0; y < rows; ++y)
        lines[y] = line_new(cols);

    if (snap->lines) {
        for (int y = 0; y < snap->rows; ++y)
            line_free(&snap->lines[y]);
        free(snap->lines);
    }
    snap->rows = rows, snap->cols = cols, snap->lines = lines;
    snap->dirty = realloc(snap->dirty, snap->rows * snap->cols * sizeof(bool));
}

void cluterm_snapshot(Cluterm *term, ClutermSnapshot *snap)
{
    assert(term != NULL);
    ClutermBuffer *b = active_buffer(term);

    if (b->rows != snap->rows || b->cols != snap->cols)
        snapshot_resize(snap, b->rows, b->cols);

    buffer_snapshot(b, snap->lines);

    memcpy(&snap->cursor, &b->cursor, sizeof(Cursor));
    snap->cursor.y += b->scroll_offset;

    memcpy(&snap->theme, &term->theme, sizeof(Theme));

    memmove(snap->dirty, b->dirty,
            snap->cols * snap->rows * sizeof(*snap->dirty));
    memset(b->dirty, 0, b->rows * b->cols * sizeof(*b->dirty));
}

ssize_t cluterm_paste(Cluterm *term, const char *text)
{
    assert(term != NULL);
    bool bracketed_mode = IS_SET(term->mode, MODE_BRACKETED_PASTE);
    if (bracketed_mode)
        cluterm_send(term, "\x1b[200~", 6);

    size_t len = strlen(text);
    ssize_t n  = cluterm_send(term, text, len);

    if (bracketed_mode)
        cluterm_send(term, "\x1b[201~", 6);
    return n;
}

ssize_t cluterm_text(Cluterm *term, const char *buffer, size_t len)
{
    assert(term != NULL);
    return cluterm_send(term, buffer, len);
}

void cluterm_resize(Cluterm *term, int rows, int cols)
{
    assert(term != NULL);
    selection_clear(term);
    ClutermBuffer *b = active_buffer(term);
    if (b->rows != rows || b->cols != cols) {
        buffer_resize(&term->buffer[0], &term->config, rows, cols);
        buffer_resize(&term->buffer[1], &term->config, rows, cols);
    }
}

void cluterm_destroy(Cluterm *term)
{
    assert(term != NULL);
    buffer_destroy(&term->buffer[0]);
    buffer_destroy(&term->buffer[1]);
    free(term);
}
// vim:fdm=marker
