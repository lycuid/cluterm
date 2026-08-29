#include "frame.h"
#include "glyph_cache.h"
#include "main.h"
#include <SDL2/SDL.h>
#include <cluterm.h>
#include <cluterm/vt/buffer.h>

static struct {
    int y, x, len;
    Rgb fg, bg;
    CellState state;
} batch = {0};

static inline void canvas_resize(FrameCanvas *canvas, size_t w, size_t h)
{
    canvas->dispw = w, canvas->disph = h;
    if (canvas->dispw <= canvas->w && canvas->disph <= canvas->h)
        return;
    canvas->w = canvas->dispw, canvas->h = canvas->disph;
    if (canvas->texture)
        SDL_DestroyTexture(canvas->texture);
    canvas->texture = SDL_CreateTexture(gfx->renderer, SDL_PIXELFORMAT_RGBA8888,
                                        SDL_TEXTUREACCESS_TARGET, canvas->dispw,
                                        canvas->disph);
    if (!canvas->texture)
        die(1, "%s\n", SDL_GetError());
}

static inline void background(Rgb bg, const SDL_Rect *rect)
{
    SDL_SetRenderDrawColor(gfx->renderer, UNPACK(bg), 0);
    SDL_RenderFillRect(gfx->renderer, rect);
}

static inline void underline(Rgb color, SDL_Rect rect, size_t sz)
{
    SDL_SetRenderDrawColor(gfx->renderer, UNPACK(color), 0);
    rect.y += rect.h - sz, rect.h = sz;
    SDL_RenderFillRect(gfx->renderer, &rect);
}

debug_var static inline void bounding_box(int y, int x)
{
    SDL_SetRenderDrawColor(gfx->renderer, UNPACK(0x434343), 0);
    SDL_RenderDrawRect(gfx->renderer, &(SDL_Rect){
                                          .y = y * gfx->f_height,
                                          .x = x * gfx->f_width,
                                          .w = gfx->f_width,
                                          .h = gfx->f_height,
                                      });
}

static inline void bar(Rgb color, SDL_Rect rect, size_t sz)
{
    SDL_SetRenderDrawColor(gfx->renderer, UNPACK(color), 0);
    rect.w = sz;
    SDL_RenderFillRect(gfx->renderer, &rect);
}

static inline Rgb cell_fg(const Cell *cell, const Theme *theme)
{
    if (IS_SET(cell->attrs.state, CELL_INVERSE))
        return resolve_color(&cell->attrs.bg, theme);
    return resolve_color(&cell->attrs.fg, theme);
}

static inline Rgb cell_bg(const Cell *cell, const Theme *theme)
{
    if (IS_SET(cell->attrs.state, CELL_INVERSE))
        return resolve_color(&cell->attrs.fg, theme);
    return resolve_color(&cell->attrs.bg, theme);
}

static inline bool cell_belongs(const Cell *cell, const Theme *theme)
{
    Rgb fg = cell_fg(cell, theme), bg = cell_bg(cell, theme);

    return fg == batch.fg && bg == batch.bg && cell->attrs.state == batch.state;
}

static inline void batch_add(const Cell *cell, const Theme *theme, int x)
{
    if (!batch.len) {
        batch.x     = x;
        batch.state = cell->attrs.state;
        batch.fg    = cell_fg(cell, theme);
        batch.bg    = cell_bg(cell, theme);
    }
    batch.len++;
}

static inline void batch_flush(const Line line)
{
    if (!batch.len)
        return;

    SDL_Rect dst = {.x = gfx->f_width * batch.x,
                    .y = gfx->f_height * batch.y,
                    .w = gfx->f_width * batch.len,
                    .h = gfx->f_height};

    background(batch.bg, &dst);

    for (int dx = 0; dx < batch.len; ++dx) {
        int y = batch.y, x = batch.x + dx;
        if (line[x].value != ' ')
            gcache_emit(line[x], batch.fg, y, x);
#if DEBUG_LVL >= 4
        bounding_box(y, x);
#endif
    }

    if (IS_SET(batch.state, CELL_UNDERLINE))
        underline(batch.fg, dst, 2);

    batch.len = 0;
}

static inline void draw_cursor(const Frame *frame)
{
    const Cursor *c = &frame->buffer.cursor;
    if (c->x >= frame->buffer.cols || c->y >= frame->buffer.rows)
        return;

    Cell cell    = frame->buffer.lines[c->y][c->x];
    SDL_Rect dst = {.x = c->x * gfx->f_width,
                    .y = c->y * gfx->f_height,
                    .w = gfx->f_width,
                    .h = gfx->f_height};

    bool use_cursor =
        c->visible &&
        (c->style == CursorSolid ||
         (c->style == CursorBlink && frame->_cursor_blink_state.visible));

    if (use_cursor && c->shape == CursorBlock) {
        cell.attrs.fg = ColorRgb(~c->color & 0xffffff);
        cell.attrs.bg = ColorRgb(c->color);
    }

    Rgb fg = cell_fg(&cell, &frame->theme), bg = cell_bg(&cell, &frame->theme);

    background(bg, &dst);
    gcache_emit(cell, fg, c->y, c->x);

    if (use_cursor && c->shape == CursorUnderline)
        underline(c->color, dst, 3);
    else if (IS_SET(cell.attrs.state, CELL_UNDERLINE))
        underline(fg, dst, 2);

    if (use_cursor && c->shape == CursorBar)
        bar(c->color, dst, 3);

    gcache_flush();
}

void frame_resize(Frame *frame, int rows, int cols)
{
    Line *ll = malloc(rows * sizeof(Line));
    for (int y = 0; y < rows; ++y)
        ll[y] = malloc(cols * sizeof(Cell));

    struct FrameBuffer *buffer = &frame->buffer;
    if (buffer->lines) {
        for (int y = 0; y < buffer->rows; ++y)
            free(buffer->lines[y]);
        free(buffer->lines);
    }
    buffer->rows = rows, buffer->cols = cols, buffer->lines = ll;
    buffer->dirty =
        realloc(buffer->dirty, buffer->rows * buffer->cols * sizeof(bool));

    canvas_resize(&frame->canvas, cols * gfx->f_width, rows * gfx->f_height);
}

void frame_capture(Frame *frame, const Cluterm *term)
{
    const ClutermBuffer *cb = ACTIVE_BUFFER(term);
    struct FrameBuffer *fb  = &frame->buffer;

    for (int y = 0; y < cb->rows; ++y)
        memcpy(fb->lines[y], line_at(cb, y), cb->cols * sizeof(*cb->lines[y]));
    memcpy(&fb->cursor, &cb->cursor, sizeof(Cursor));

    memmove(fb->dirty, cb->dirty, cb->cols * cb->rows * sizeof(*cb->dirty));
    memset(cb->dirty, 0, cb->rows * cb->cols * sizeof(*cb->dirty));

    memcpy(&frame->theme, &term->theme, sizeof(Theme));

#ifdef DUMP_DIRTY_FRAME
    // {{{
    static uint64_t frameno = 0;
    debug("\x1b[2J");
    debug("----------------- Frame begin: (%ld) -----------------\n",
          ++frameno);
    for (int y = 0; y < frame->buffer.rows; ++y) {
        for (int x = 0; x < frame->buffer.cols; ++x) {
            Cell cell = frame->buffer.lines[y][x];
            if (frame->buffer.dirty[y * frame->buffer.cols + x]) {
                UTF8_String utf8_string = {0};
                utf8_encode(cell.value, utf8_string);
                debug("%s", utf8_string);
            } else {
                debug(".");
            }
        }
        debug("\n");
    }
    debug("----------------- Frame end -----------------\n");
    // }}}
#endif
}

void frame_canvas_update(Frame *frame, bool fresh)
{
    SDL_SetRenderTarget(gfx->renderer, frame->canvas.texture);
    struct FrameBuffer *buffer = &frame->buffer;

    for (int y = 0; y < buffer->rows; ++y) {
        batch.y = y;
        for (int x = 0; x < buffer->cols; ++x) {
            if (!fresh && !buffer->dirty[y * buffer->cols + x]) {
                batch_flush(buffer->lines[y]);
                continue;
            }

            Cell cell = buffer->lines[y][x];

            if (!cell_belongs(&cell, &frame->theme))
                batch_flush(buffer->lines[y]);
            batch_add(&cell, &frame->theme, x);
        }
        batch_flush(buffer->lines[y]);
        gcache_flush();
    }
    draw_cursor(frame);
    SDL_SetRenderTarget(gfx->renderer, NULL);
}

bool frame_tick(Frame *f)
{
    if (!f->buffer.cursor.visible || f->buffer.cursor.style != CursorBlink)
        return 0;

    if (!since(&f->_cursor_blink_state.last, FPS(2)))
        return 0;
    f->_cursor_blink_state.visible = !f->_cursor_blink_state.visible;

    return 1;
}

void frame_activity(Frame *frame)
{
    frame->_cursor_blink_state.last    = SDL_GetTicks64(),
    frame->_cursor_blink_state.visible = 1;
}

void frame_destroy(Frame *frame)
{
    if (frame->canvas.texture) {
        SDL_DestroyTexture(frame->canvas.texture);
        frame->canvas.texture = NULL;
    }
    if (frame->buffer.lines) {
        for (int y = 0; y < frame->buffer.rows; ++y)
            free(frame->buffer.lines[y]);
        free(frame->buffer.lines);
        frame->buffer.lines = NULL;
    }

    free(frame->buffer.dirty);
    frame->buffer.dirty = NULL;
}
// vim:fdm=marker
