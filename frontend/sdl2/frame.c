#include "frame.h"
#include "glyph_cache.h"
#include "main.h"
#include <SDL2/SDL.h>
#include <cluterm.h>
#include <cluterm/colors.h>
#include <cluterm/vt/buffer.h>

static struct {
    int y, x, len;
    CellAttributes attrs;
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

static inline void bar(Rgb color, SDL_Rect rect, size_t sz)
{
    SDL_SetRenderDrawColor(gfx->renderer, UNPACK(color), 0);
    rect.w = sz;
    SDL_RenderFillRect(gfx->renderer, &rect);
}

static inline bool cell_belongs(Cell *cell)
{
    if (cell->attrs.fg != batch.attrs.fg || cell->attrs.bg != batch.attrs.bg)
        return false;
    if (cell->attrs.state != batch.attrs.state)
        return false;
    return true;
}

static inline void batch_add(Cell *cell, int x)
{
    if (!batch.len)
        batch.x = x, batch.attrs = cell->attrs;
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

    background(batch.attrs.bg, &dst);

    for (int dx = 0; dx < batch.len; ++dx) {
        int y = batch.y, x = batch.x + dx;
        gcache_push_glyph(line[x], y, x);
    }

    if (IS_SET(batch.attrs.state, CELL_UNDERLINE))
        underline(batch.attrs.fg, dst, 2);

    batch.len = 0;
}

static inline void draw_cursor(Frame *frame)
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
         (c->style == CursorBlink && frame->cursor_blink_state.visible));

    if (use_cursor && c->shape == CursorBlock)
        cell.attrs.fg = ~c->color & 0xffffff, cell.attrs.bg = c->color;
    background(cell.attrs.bg, &dst);

    gcache_push_glyph(cell, c->y, c->x);

    if (use_cursor && c->shape == CursorUnderline)
        underline(c->color, dst, 3);
    else if (IS_SET(cell.attrs.state, CELL_UNDERLINE))
        underline(cell.attrs.fg, dst, 2);

    if (use_cursor && c->shape == CursorBar)
        bar(c->color, dst, 3);
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
}

void frame_canvas_update(Frame *frame, bool fresh)
{
    SDL_SetRenderTarget(gfx->renderer, frame->canvas.texture);
    struct FrameBuffer *buffer = &frame->buffer;

#ifdef DUMP_DIRTY_FRAME
    // {{{
    static uint64_t frameno = 0;
    debug("----------------- Frame begin: (%ld) -----------------\n",
          ++frameno);
    for (int y = 0; y < buffer->rows; ++y) {
        for (int x = 0; x < buffer->cols; ++x) {
            Cell cell = buffer->lines[y][x];
            if (buffer->dirty[y * buffer->cols + x]) {
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
    for (int y = 0; y < buffer->rows; ++y) {
        batch.y = y;
        for (int x = 0; x < buffer->cols; ++x) {
            if (!fresh && !buffer->dirty[y * buffer->cols + x]) {
                batch_flush(buffer->lines[y]);
                continue;
            }

            Cell cell = buffer->lines[y][x];

            if (!cell_belongs(&cell))
                batch_flush(buffer->lines[y]);
            batch_add(&cell, x);
        }
        batch_flush(buffer->lines[y]);
    }
    draw_cursor(frame);
    gcache_flush();
    SDL_SetRenderTarget(gfx->renderer, NULL);
}

bool frame_tick(Frame *f)
{
    if (!f->buffer.cursor.visible || f->buffer.cursor.style != CursorBlink)
        return 0;

    if (!since(&f->cursor_blink_state.last, FPS(2)))
        return 0;
    f->cursor_blink_state.visible = !f->cursor_blink_state.visible;

    return 1;
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
