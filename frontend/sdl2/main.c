#include "main.h"
#include "cli.h"
#include "glyph_cache.h"
#include "osc_handler.h"
#include <SDL2/SDL.h>
#include <cluterm.h>
#include <cluterm/config.h>
#include <cluterm/debug.h>
#include <cluterm/pty.h>
#include <cluterm/vt/buffer.h>
#include <fontconfig/fontconfig.h>
#include <signal.h>

typedef struct LineBatch {
    int y, x, len;
    CellAttributes attrs;
} LineBatch;

#define BATCH(y)                                                               \
    (LineBatch) { .y = y, .x = 0, .len = 0, .attrs = {0} }

#define IS_ASCII(val) (val < 0x7f)
#define FPS(n)        (1000 / n)

static struct {
    SDL_Texture *texture;
    size_t w, h, dispw, disph;
} canvas = {.texture = NULL, .w = 0, .h = 0};

static GFX_Context ctx;
const GFX_Context *gfx = &ctx;
static int running     = 1;

static struct {
    uint64_t last;
    bool visible;
} cursor_blink_state = {0};

void quit(__attribute__((unused)) int _arg) { running = 0; }

static inline void *tryp(void *res)
{
    if (!res)
        die(1, "%s\n", SDL_GetError());
    return res;
}

static inline int tryn(int res)
{
    if (res < 0)
        die(1, "%s\n", SDL_GetError());
    return res;
}

static inline void load_font(FcConfig *config, const char *style,
                             TTF_Font **font)
{
    FcPattern *pat = FcPatternBuild(
        NULL,                                          //
        FC_FAMILY, FcTypeString, cfg->font_family,     // font family.
        FC_STYLE, FcTypeString, style,                 // font style.
        FC_SIZE, FcTypeDouble, (double)cfg->font_size, // font size.
        NULL);

    FcConfigSubstitute(config, pat, FcMatchPattern);
    FcDefaultSubstitute(pat);

    FcResult res;
    FcPattern *font_pat = FcFontMatch(config, pat, &res);
    FcPatternDestroy(pat);
    if (font_pat) {
        FcChar8 *font_file = NULL;
        int font_size      = 11;
        FcPatternGetInteger(font_pat, FC_SIZE, 0, &font_size);
        if (FcPatternGetString(font_pat, FC_FILE, 0, &font_file) ==
            FcResultMatch)
            *font = TTF_OpenFont((const char *)font_file, font_size * 1.3);
        debug_1("font file: %s (%d).\n", font_file, font_size);
    }
    FcPatternDestroy(font_pat);
}

static inline void create_canvas(size_t w, size_t h)
{
    canvas.dispw = w, canvas.disph = h;
    if (canvas.dispw <= canvas.w && canvas.disph <= canvas.h)
        return;
    canvas.w = canvas.dispw, canvas.h = canvas.disph;
    if (canvas.texture)
        SDL_DestroyTexture(canvas.texture);
    canvas.texture = tryp(SDL_CreateTexture(
        ctx.renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
        canvas.dispw, canvas.disph));
}

static inline void sdl_init(void)
{
    tryn(SDL_Init(SDL_INIT_VIDEO));
    tryn(TTF_Init());
    ctx.window = tryp(SDL_CreateWindow(
        cfg->title, 280, 100, 0, 0, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE));
    ctx.renderer =
        tryp(SDL_CreateRenderer(ctx.window, -1, SDL_RENDERER_ACCELERATED));

    /* loading fonts. */ {
        FcConfig *config = FcInitLoadConfigAndFonts();

        load_font(config, "Regular", &ctx.fonts[FontRegular]);
        load_font(config, "Bold", &ctx.fonts[FontBold]);
        load_font(config, "Italic", &ctx.fonts[FontItalic]);
        load_font(config, "BoldItalic", &ctx.fonts[FontBoldItalic]);

        FcConfigDestroy(config);
    }

    /* cell size. */ {
        TTF_SizeText(ctx.fonts[FontBold], "M", &ctx.f_width, NULL);
        ctx.f_height = TTF_FontLineSkip(ctx.fonts[FontBold]);
    }

    SDL_SetWindowSize(ctx.window, ctx.f_width * cfg->cols,
                      ctx.f_height * cfg->rows);
    create_canvas(ctx.f_width * cfg->cols, ctx.f_height * cfg->rows);
    SDL_StartTextInput();

    gcache_init();
}

static inline void background(Rgb bg, const SDL_Rect *rect)
{
    SDL_SetRenderDrawColor(ctx.renderer, UNPACK(bg), 0);
    SDL_RenderFillRect(ctx.renderer, rect);
}

static inline void underline(Rgb color, SDL_Rect rect, size_t sz)
{
    SDL_SetRenderDrawColor(ctx.renderer, UNPACK(color), 0);
    rect.y += rect.h - sz, rect.h = sz;
    SDL_RenderFillRect(ctx.renderer, &rect);
}

static inline void bar(Rgb color, SDL_Rect rect, size_t sz)
{
    SDL_SetRenderDrawColor(ctx.renderer, UNPACK(color), 0);
    rect.w = sz;
    SDL_RenderFillRect(ctx.renderer, &rect);
}

static inline bool cell_belongs(LineBatch *batch, Cell *cell)
{
    if (cell->attrs.fg != batch->attrs.fg || cell->attrs.bg != batch->attrs.bg)
        return false;
    if (cell->attrs.state != batch->attrs.state)
        return false;
    return true;
}

static inline void batch_add(LineBatch *batch, Cell *cell, int x)
{
    if (!batch->len)
        batch->x = x, batch->attrs = cell->attrs;
    batch->len++;
}

static inline void batch_flush(LineBatch *batch, const ClutermBuffer *b)
{
    if (!batch || !batch->len)
        return;

    SDL_Rect dst = {.x = ctx.f_width * batch->x,
                    .y = ctx.f_height * batch->y,
                    .w = ctx.f_width * batch->len,
                    .h = ctx.f_height};

    background(batch->attrs.bg, &dst);

    for (int dx = 0; dx < batch->len; ++dx) {
        int y = batch->y, x = batch->x + dx;
        gcache_push_glyph(getcell(b, y, x), y, x);
    }

    if (IS_SET(batch->attrs.state, CELL_UNDERLINE))
        underline(batch->attrs.fg, dst, 2);

    batch->len = 0;
}

static inline void draw_cursor(const ClutermBuffer *b)
{
    const Cursor *c = &b->cursor;
    if (c->x >= b->cols || c->y >= b->rows)
        return;

    Cell cell    = getcell(b, c->y, c->x);
    SDL_Rect dst = {.x = c->x * ctx.f_width,
                    .y = c->y * ctx.f_height,
                    .w = ctx.f_width,
                    .h = ctx.f_height};

    bool use_cursor =
        c->visible && (c->style == CursorSolid ||
                       (c->style == CursorBlink && cursor_blink_state.visible));

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

static inline void update_canvas(const Cluterm *term, bool fresh)
{
    const ClutermBuffer *b = ACTIVE_BUFFER(term);

#ifdef DUMP_DIRTY_FRAME
    // {{{
    static uint64_t frame = 0;
    debug("----------------- Frame begin: (%ld) -----------------\n", ++frame);
    for (int y = 0; y < b->rows; ++y) {
        for (int x = 0; x < b->cols; ++x) {
            Cell cell = getcell(b, y, x);
            if (b->dirty[y * b->cols + x]) {
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

    for (int y = 0; y < b->rows; ++y) {
        LineBatch batch = BATCH(y);
        for (int x = 0; x < b->cols; ++x) {
            if (!fresh && !b->dirty[y * b->cols + x]) {
                batch_flush(&batch, b);
                continue;
            }

            Cell cell = getcell(b, y, x);

            if (!cell_belongs(&batch, &cell))
                batch_flush(&batch, b);
            batch_add(&batch, &cell, x);
        }
        batch_flush(&batch, b);
    }
    draw_cursor(b);
    gcache_flush();
    memset(b->dirty, 0, b->cols * b->rows * sizeof(*b->dirty));
}

static inline ssize_t clipboard_paste(const Cluterm *term)
{
    char *text = SDL_GetClipboardText();
    if (!text)
        return -1;

    if (IS_SET(term->mode, MODE_BRACKETED_PASTE))
        pty_write(&term->pty, "\x1b[200~", 6);

    size_t len = strlen(text);
    ssize_t n  = pty_write(&term->pty, text, len);

    if (IS_SET(term->mode, MODE_BRACKETED_PASTE))
        pty_write(&term->pty, "\x1b[201~", 6);

    SDL_free(text);
    return n;
}

static inline void handle_keydown(const Cluterm *term, SDL_Event *e)
{
    SDL_KeyboardEvent *key = &e->key;

    bool ctrl  = IS_SET_ANY(key->keysym.mod, KMOD_CTRL),
         shift = IS_SET_ANY(key->keysym.mod, KMOD_SHIFT),
         alt   = IS_SET_ANY(key->keysym.mod, KMOD_ALT);

    switch (key->keysym.sym) {
    case SDLK_a: goto mod_put;
    case SDLK_b: goto mod_put;
    case SDLK_c: goto mod_put;
    case SDLK_d: goto mod_put;
    case SDLK_e: goto mod_put;
    case SDLK_f: goto mod_put;
    case SDLK_g: goto mod_put;
    case SDLK_h: goto mod_put;
    case SDLK_i: goto mod_put;
    case SDLK_j: goto mod_put;
    case SDLK_k: goto mod_put;
    case SDLK_l: goto mod_put;
    case SDLK_m: goto mod_put;
    case SDLK_n: goto mod_put;
    case SDLK_o: goto mod_put;
    case SDLK_p: goto mod_put;
    case SDLK_q: goto mod_put;
    case SDLK_r: goto mod_put;
    case SDLK_s: goto mod_put;
    case SDLK_t: goto mod_put;
    case SDLK_u: goto mod_put;
    case SDLK_v: {
        if (ctrl && shift)
            clipboard_paste(term);
        else
            goto mod_put;
    } break;
    case SDLK_w: goto mod_put;
    case SDLK_x: goto mod_put;
    case SDLK_y: goto mod_put;
    case SDLK_z: {
    mod_put:
        if (ctrl)
            pty_write(&term->pty, (char[]){key->keysym.sym - 'a' + 1}, 1);
        else if (alt)
            pty_write(&term->pty, (char[]){0x1b, key->keysym.sym}, 2);
    } break;

        // clang-format off
    case SDLK_RETURN:    // fallthrough
    case SDLK_RETURN2:   pty_write(&term->pty, "\r", 1);     break;
    case SDLK_TAB:       pty_write(&term->pty, "\t", 1);     break;
    case SDLK_BACKSPACE: pty_write(&term->pty, "\b", 1);     break;
    case SDLK_ESCAPE:    pty_write(&term->pty, "\x1b", 1);   break;
    case SDLK_UP:        pty_write(&term->pty, "\x1b[A", 3); break;
    case SDLK_DOWN:      pty_write(&term->pty, "\x1b[B", 3); break;
    case SDLK_RIGHT:     pty_write(&term->pty, "\x1b[C", 3); break;
    case SDLK_LEFT:      pty_write(&term->pty, "\x1b[D", 3); break;
    case SDLK_HOME:      pty_write(&term->pty, "\x1b[H", 3); break;
    case SDLK_END:       pty_write(&term->pty, "\x1b[F", 3); break;
    case SDLK_INSERT: {
        shift ? clipboard_paste(term) : pty_write(&term->pty, "\x1b[2~", 4);
    } break;
    case SDLK_DELETE:    pty_write(&term->pty, "\x1b[3~", 4); break;
    case SDLK_PAGEUP:    pty_write(&term->pty, "\x1b[5~", 4); break;
    case SDLK_PAGEDOWN:  pty_write(&term->pty, "\x1b[6~", 4); break;
        // clang-format on
    default: break;
    }
}

static inline void render(Cluterm *term, bool fresh)
{
    SDL_SetRenderTarget(ctx.renderer, canvas.texture);
    if (term != NULL)
        update_canvas(term, fresh);
    SDL_SetRenderTarget(ctx.renderer, NULL);

    if (fresh) {
        SDL_SetRenderDrawColor(ctx.renderer, UNPACK(term->bg), 0);
        SDL_RenderClear(ctx.renderer);
    }
    SDL_Rect rect = {.x = 0, .y = 0, .w = canvas.dispw, .h = canvas.disph};
    SDL_RenderCopy(ctx.renderer, canvas.texture, &rect, &rect);
    SDL_RenderPresent(ctx.renderer);
}

static inline bool since(uint64_t *time, uint64_t ms)
{
    if (time == NULL)
        return false;
    uint64_t tick = SDL_GetTicks64();
    bool result   = tick - *time > ms;
    if (result)
        *time = tick;
    return result;
}

int main(int argc, char *const *argv)
{
    init_config();

    char *const *cmd = argparse(argc, argv);
    char *shell[2]   = {0};
    if (!cmd || !*cmd) {
        if (!(*shell = getenv("SHELL")))
            *shell = "/bin/sh";
        cmd = shell;
    }

    debug_1("cfg->title(%s).\n", cfg->title);
    debug_1("cfg->fg(#%x).\n", cfg->fg);
    debug_1("cfg->bg(#%x).\n", cfg->bg);
    debug_1("cfg->tab_width(%d).\n", cfg->tab_width);
    debug_1("cfg->font_family(%s).\n", cfg->font_family);
    debug_1("cfg->font_size(%d).\n", cfg->font_size);

    Cluterm term = {0};
    cluterm_init(&term, cmd);
    term.osc_handler = osc_handler;

    sdl_init();
    signal(SIGCHLD, quit); // shell exits/crashes.

    struct {
        uint64_t last;
        uint w, h, pending : 1;
    } resz             = {0};
    uchar stream[4096] = {0};
    ssize_t n          = 0;

    for (SDL_Event e; running;) {
        if ((n = pty_read(&term.pty, stream, sizeof(stream))) > 0) {
            cluterm_write(&term, stream, n);
            render(&term, 0);
        }

        ClutermBuffer *b = ACTIVE_BUFFER(&term);
        if (b->cursor.visible && b->cursor.style == CursorBlink) {
            if (since(&cursor_blink_state.last, FPS(2))) {
                cursor_blink_state.visible = !cursor_blink_state.visible;
                render(&term, 0);
            }
        }

        if (resz.pending && since(&resz.last, FPS(2))) {
            cluterm_resize(&term, resz.h, resz.w);
            gcache_resize();
            create_canvas(resz.w * ctx.f_width, resz.h * ctx.f_height);
            render(&term, 1);
            resz.pending = 0;
        }

        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT: running = 0; break;

            case SDL_WINDOWEVENT: {
                SDL_WindowEvent *win = &e.window;
                switch (win->event) {
                case SDL_WINDOWEVENT_EXPOSED: render(&term, 1); break;
                case SDL_WINDOWEVENT_CLOSE: running = 0; break;

                case SDL_WINDOWEVENT_SIZE_CHANGED: {
                    resz.w       = MAX(win->data1 / ctx.f_width, 10),
                    resz.h       = MAX(win->data2 / ctx.f_height, 10),
                    resz.pending = 1;
                } break;
                }
            } break;

            case SDL_TEXTINPUT: {
                pty_write(&term.pty, e.text.text, strlen(e.text.text));
                cursor_blink_state.last    = SDL_GetTicks64(),
                cursor_blink_state.visible = 1;
            } break;
            case SDL_KEYDOWN: handle_keydown(&term, &e); break;
            default: break;
            }
        }
        SDL_Delay(FPS(720));
    }

    cluterm_destroy(&term);
    {
        if (canvas.texture)
            SDL_DestroyTexture(canvas.texture);
        gcache_destroy();
        for (size_t i = 0; i < LENGTH(ctx.fonts); ++i)
            if (ctx.fonts[i])
                TTF_CloseFont(ctx.fonts[i]);
        if (ctx.renderer)
            SDL_DestroyRenderer(ctx.renderer);
        if (ctx.window)
            SDL_DestroyWindow(ctx.window);
        TTF_Quit();
        SDL_StopTextInput();
        SDL_Quit();
        FcFini();
        debug_1("SDL cleanup: Done!.\n");
    }
    return 0;
}
// vim:fdm=marker
