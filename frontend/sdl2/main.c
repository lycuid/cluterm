#include "main.h"
#include "glyph_cache.h"
#include "osc_handler.h"
#include <SDL2/SDL.h>
#include <cluterm.h>
#include <cluterm/debug.h>
#include <cluterm/pty.h>
#include <cluterm/vt/buffer.h>
#include <config.h>
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

void quit(__attribute__((unused)) int _arg) { running = 0; }

static inline void *tryp(void *res)
{
    if (!res)
        die("%s\n", SDL_GetError());
    return res;
}

static inline int tryn(int res)
{
    if (res < 0)
        die("%s\n", SDL_GetError());
    return res;
}

static inline void load_font(FcConfig *config, const char *style, int size,
                             TTF_Font **font)
{
    FcPattern *pat =
        FcPatternBuild(NULL,                                //
                       FC_FAMILY, FcTypeString, FontFamily, // font family.
                       FC_STYLE, FcTypeString, style,       // font style.
                       FC_SIZE, FcTypeDouble, (double)size, // font size.
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
        "cluterm", 280, 100, 0, 0, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE));
    ctx.renderer =
        tryp(SDL_CreateRenderer(ctx.window, -1, SDL_RENDERER_ACCELERATED));

    /* loading fonts. */ {
        FcConfig *config = FcInitLoadConfigAndFonts();

        load_font(config, "Regular", FontSize, &ctx.fonts[FontRegular]);
        load_font(config, "Bold", FontSize, &ctx.fonts[FontBold]);
        load_font(config, "Italic", FontSize, &ctx.fonts[FontItalic]);
        load_font(config, "BoldItalic", FontSize, &ctx.fonts[FontBoldItalic]);

        FcConfigDestroy(config);
    }

    /* cell size. */ {
        char printable_ascii[128 - 32 + 1] = {0};
        for (int i = 32; i < 128; ++i)
            printable_ascii[i - 32] = i;
        TTF_SizeText(ctx.fonts[FontRegular], printable_ascii, &ctx.f_width,
                     &ctx.f_height);
        ctx.f_width = ctx.f_width / LENGTH(printable_ascii) +
                      (ctx.f_width % LENGTH(printable_ascii) != 0);
    }

    SDL_SetWindowSize(ctx.window, ctx.f_width * Columns, ctx.f_height * Rows);
    create_canvas(ctx.f_width * Columns, ctx.f_height * Rows);
    SDL_StartTextInput();

    gcache_init();
}

static inline void background(Rgb bg, const SDL_Rect *rect)
{
    SDL_SetRenderDrawColor(ctx.renderer, UNPACK(bg), 0);
    SDL_RenderFillRect(ctx.renderer, rect);
}

static inline void underline(Rgb color, const SDL_Rect *rect)
{
    SDL_SetRenderDrawColor(ctx.renderer, UNPACK(color), 0);
    SDL_RenderDrawLine(ctx.renderer, rect->x, rect->y + rect->h - 2,
                       rect->x + rect->w, rect->y + rect->h - 2);
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

    const Cursor *c = &b->cursor;
    if (c->y == batch->y && BETWEEN(c->x, batch->x, batch->x + batch->len) &&
        !IS_SET(c->state, CURSOR_HIDDEN))
        gcache_push_glyph(c->cell, c->y, c->x);

    for (int dx = 0; dx < batch->len; ++dx) {
        int y = batch->y, x = batch->x + dx;
        gcache_push_glyph(getcell(b, y, x), y, x);
    }
    gcache_flush(ctx.renderer);

    if (IS_SET(batch->attrs.state, CELL_UNDERLINE))
        underline(batch->attrs.fg, &dst);

    batch->len = 0;
}

static inline void update_canvas(const Cluterm *term, bool fresh)
{
    SDL_SetRenderTarget(ctx.renderer, canvas.texture);
    const ClutermBuffer *b = ACTIVE_BUFFER(term);
#ifdef DUMP_FRAME
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
    memset(b->dirty, 0, b->cols * b->rows * sizeof(*b->dirty));
    SDL_SetRenderTarget(ctx.renderer, NULL);
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

#define ESC "\x1b"

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
        if (ctrl && shift) {
            clipboard_paste(term);
        } else {
            goto mod_put;
        }
    } break;
    case SDLK_w: goto mod_put;
    case SDLK_x: goto mod_put;
    case SDLK_y: goto mod_put;
    case SDLK_z: {
    mod_put:
        if (ctrl)
            pty_write(&term->pty, (char[]){key->keysym.sym - 96}, 1);
        else if (alt)
            pty_write(&term->pty, (char[]){0x1b, key->keysym.sym}, 2);
    } break;

        // clang-format off
    case SDLK_RETURN2:   // fallthrough
    case SDLK_RETURN:    pty_write(&term->pty, "\r", 1); break;

    case SDLK_TAB:       pty_write(&term->pty, "\t", 1); break;
    case SDLK_ESCAPE:    pty_write(&term->pty, ESC, 1); break;
    case SDLK_BACKSPACE: pty_write(&term->pty, "\x7f", 1); break;
    case SDLK_UP:        pty_write(&term->pty, ESC"[A", 3); break;
    case SDLK_DOWN:      pty_write(&term->pty, ESC"[B", 3); break;
    case SDLK_RIGHT:     pty_write(&term->pty, ESC"[C", 3); break;
    case SDLK_LEFT:      pty_write(&term->pty, ESC"[D", 3); break;
    case SDLK_HOME:      pty_write(&term->pty, ESC"[H", 3); break;
    case SDLK_END:       pty_write(&term->pty, ESC"[F", 3); break;
    case SDLK_INSERT: {
        shift ? clipboard_paste(term) : pty_write(&term->pty, ESC"[2~", 4);
    } break;
    case SDLK_DELETE:    pty_write(&term->pty, ESC"[3~", 4); break;
    case SDLK_PAGEUP:    pty_write(&term->pty, ESC"[5~", 4); break;
    case SDLK_PAGEDOWN:  pty_write(&term->pty, ESC"[6~", 4); break;
        // clang-format on

#undef ESC
    default: break;
    }
}

static inline void render(Cluterm *term, bool fresh)
{
    if (term != NULL)
        update_canvas(term, fresh);

    if (fresh) {
        SDL_SetRenderDrawColor(ctx.renderer, UNPACK(DefaultBG), 0);
        SDL_RenderClear(ctx.renderer);
    }
    SDL_Rect rect =
        (SDL_Rect){.x = 0, .y = 0, .w = canvas.dispw, .h = canvas.disph};
    SDL_RenderCopy(ctx.renderer, canvas.texture, &rect, &rect);
    SDL_RenderPresent(ctx.renderer);
}

int main(void)
{
    Cluterm term;
    cluterm_init(&term);
    term.osc_handler = osc_handler;

    sdl_init();
    signal(SIGCHLD, quit); // shell exits/crashes.

    ssize_t n          = 0;
    uchar stream[4096] = {0};
    struct {
        uint w, h, pending : 1;
        uint64_t time;
    } resz = {0};

    render(NULL, 0);
    uint64_t tick;
    for (SDL_Event e; running;) {
        if ((n = pty_read(&term.pty, stream, sizeof(stream))) > 0) {
            cluterm_write(&term, stream, n);
            render(&term, 0);
        }

        if (resz.pending && (tick = SDL_GetTicks64()) - resz.time > FPS(2)) {
            cluterm_resize(&term, resz.h, resz.w);
            gcache_resize();
            create_canvas(resz.w * ctx.f_width, resz.h * ctx.f_height);
            render(&term, 1);
            resz.pending = 0, resz.time = tick;
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
