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

static inline bool cell_belongs_in_batch(const Cell *cell, const Theme *theme)
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
    const ClutermSnapshot *snap = &frame->term_snapshot;
    const Cursor *c             = &snap->cursor;
    Rgb color                   = snap->theme.cursor;

    if (c->x >= snap->cols || c->y >= snap->rows)
        return;

    Cell cell    = snap->lines[c->y][c->x];
    SDL_Rect dst = {.x = c->x * gfx->f_width,
                    .y = c->y * gfx->f_height,
                    .w = gfx->f_width,
                    .h = gfx->f_height};

    bool use_cursor =
        c->visible &&
        (c->style == CursorSolid ||
         (c->style == CursorBlink && frame->_cursor_blink_state.visible));

    if (use_cursor && c->shape == CursorBlock) {
        cell.attrs.fg = ColorRgb(~color & 0xffffff);
        cell.attrs.bg = ColorRgb(color);
    }

    Rgb fg = cell_fg(&cell, &snap->theme), bg = cell_bg(&cell, &snap->theme);

    background(bg, &dst);
    gcache_emit(cell, fg, c->y, c->x);

    if (use_cursor && c->shape == CursorUnderline)
        underline(color, dst, 3);
    else if (IS_SET(cell.attrs.state, CELL_UNDERLINE))
        underline(fg, dst, 2);

    if (use_cursor && c->shape == CursorBar)
        bar(color, dst, 3);

    gcache_flush();
}

void frame_resize(Frame *frame, int rows, int cols)
{
    canvas_resize(&frame->canvas, cols * gfx->f_width, rows * gfx->f_height);
}

void frame_canvas_update(Frame *frame, bool fresh)
{
    SDL_SetRenderTarget(gfx->renderer, frame->canvas.texture);
    struct ClutermSnapshot *snap = &frame->term_snapshot;

#if DUMP_DIRTY_FRAME >= 1
    // {{{
#if DUMP_DIRTY_FRAME >= 2
    debug("\x1b[2J");
#endif
    static uint64_t frameno = 0;
    debug("----------------- Frame begin: (%ld) -----------------\n",
          ++frameno);
    for (int y = 0; y < snap->rows; ++y) {
        for (int x = 0; x < snap->cols; ++x) {
            Cell cell = snap->lines[y][x];
            if (snap->dirty[y * snap->cols + x] || selection_contains(y, x)) {
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

    for (int y = 0; y < snap->rows; ++y) {
        batch.y = y;
        for (int x = 0; x < snap->cols; ++x) {
            if (!fresh && !snap->dirty[y * snap->cols + x] &&
                !selection_contains(y, x)) {
                batch_flush(snap->lines[y]);
                continue;
            }

            Cell cell = snap->lines[y][x];
            if (selection_contains(y, x))
                cell.attrs.fg = ColorBg(), cell.attrs.bg = ColorFg();

            if (!cell_belongs_in_batch(&cell, &snap->theme))
                batch_flush(snap->lines[y]);
            batch_add(&cell, &snap->theme, x);
        }
        batch_flush(snap->lines[y]);
        gcache_flush(); // draw to canvas.
    }
    draw_cursor(frame);
    SDL_SetRenderTarget(gfx->renderer, NULL);
}

bool frame_tick(Frame *f)
{
    if (!f->term_snapshot.cursor.visible ||
        f->term_snapshot.cursor.style != CursorBlink)
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
    if (frame->term_snapshot.lines) {
        for (int y = 0; y < frame->term_snapshot.rows; ++y)
            free(frame->term_snapshot.lines[y]);
        free(frame->term_snapshot.lines);
        frame->term_snapshot.lines = NULL;
    }

    free(frame->term_snapshot.dirty);
    frame->term_snapshot.dirty = NULL;
}
// vim:fdm=marker
