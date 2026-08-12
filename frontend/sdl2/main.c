#include "main.h"
#include "atlas.h"
#include "glyph_table.h"
#include <SDL.h>
#include <cluterm.h>
#include <cluterm/debug.h>
#include <cluterm/pty.h>
#include <cluterm/vt/buffer.h>
#include <config.h>
#include <fontconfig/fontconfig.h>
#include <signal.h>

static SDL_Texture *canvas = NULL; // persistent framebuffer.
static GFX_Context ctx;
static Atlas ascii_atlas;
static int running = 1;

void quit(int _arg)
{
    (void)_arg;
    running = 0;
}

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

static inline void load_font(FcConfig *config, const char *family,
                             const char *style, int size, TTF_Font **font)
{
    /* FcPattern *pat = FcNameParse((const FcChar8 *)pattern); */
    FcPattern *pat =
        FcPatternBuild(NULL,                                //
                       FC_FAMILY, FcTypeString, family,     // font family.
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

static inline void create_canvas(int w, int h)
{
    if (canvas)
        SDL_DestroyTexture(canvas);
    canvas = tryp(SDL_CreateTexture(ctx.renderer, SDL_PIXELFORMAT_ABGR8888,
                                    SDL_TEXTUREACCESS_TARGET, w, h));
}

static inline void load_fonts(void)
{
    FcConfig *config = FcInitLoadConfigAndFonts();

    load_font(config, FontFamily, "Regular", FontSize, &ctx.fonts[FontRegular]);
    load_font(config, FontFamily, "Bold", FontSize, &ctx.fonts[FontBold]);
    load_font(config, FontFamily, "Italic", FontSize, &ctx.fonts[FontItalic]);
    load_font(config, FontFamily, "BoldItalic", FontSize,
              &ctx.fonts[FontBoldItalic]);

    FcConfigDestroy(config);
}

static inline void sdl_init(void)
{
    tryn(SDL_Init(SDL_INIT_VIDEO));
    tryn(TTF_Init());
    ctx.window = tryp(SDL_CreateWindow(
        "cluterm", 280, 100, 0, 0, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE));
    ctx.renderer =
        tryp(SDL_CreateRenderer(ctx.window, -1, SDL_RENDERER_ACCELERATED));

    load_fonts();
    /* cell size */ {
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

    atlas_init(&ascii_atlas, &ctx);
    glyph_table_init();
}

debug_var static inline void bounding_box(SDL_Rect *box)
{
    SDL_SetRenderDrawColor(ctx.renderer, UNPACK(0x303030), 0);
    SDL_RenderDrawLines(
        ctx.renderer,
        (SDL_Point[]){{.x = box->x, .y = box->y},
                      {.x = box->x + box->w, .y = box->y},
                      {.x = box->x + box->w, .y = box->y + box->h},
                      {.x = box->x, .y = box->y + box->h},
                      {.x = box->x, .y = box->y}},
        5);
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

static inline Cell get_display_cell(const CluTermBuffer *b, int y, int x)
{
    const Cursor *c = &b->cursor;
    Cell cell       = getcell(b, y, x);
    if (y == c->y && x == c->x && (c->state & CursorHide) == 0)
        SWAP(cell.attrs.fg, cell.attrs.bg);
    return cell;
}

static inline void draw_unicode(Cell cell, int y, int x)
{
    const Glyph *glyph = glyph_table_request_unicode(&ctx, cell);
    SDL_Rect src       = {.x = 0, .y = 0, .w = glyph->w, .h = glyph->h},
             dst       = {.x = ctx.f_width * x,
                          .y = ctx.f_height * y,
                          .w = MAX(glyph->w, ctx.f_width),
                          .h = MAX(glyph->h, ctx.f_height)};
    if (glyph->texture)
        SDL_RenderCopy(ctx.renderer, glyph->texture, &src, &dst);
#if DEBUG_LVL >= 4
    bounding_box(&dst);
#endif
}

typedef struct LineBatch {
    int y, x, len;
    bool is_ascii : 1;
    CellAttributes attrs;
} LineBatch;

#define BATCH(y)                                                               \
    (LineBatch){.y = y, .x = 0, .len = 0, .attrs = {0}, .is_ascii = 0}

#define IS_ASCII(val) (val < 0x7f)

static inline bool cell_belongs(LineBatch *batch, Cell *cell)
{
    // making sure that the batch is either ascii or non-ascii unicode as
    // currently ascii and unicode glyphs are rendered in seperate ways.
    // @NOTE: remove this condition later, when there is a unified way to render
    // any character regardless if its ascii or non-ascii unicode.
    if (batch->len && batch->is_ascii != IS_ASCII(cell->value))
        return false;

    if (cell->attrs.fg != batch->attrs.fg || cell->attrs.bg != batch->attrs.bg)
        return false;

    if (IS_SET(cell->attrs.state, CELL_UNDERLINE) !=
        IS_SET(batch->attrs.state, CELL_UNDERLINE))
        return false;

    return true;
}

static inline void batch_add(LineBatch *batch, Cell *cell, int x)
{
    if (!batch->len)
        batch->x = x, batch->is_ascii = IS_ASCII(cell->value),
        batch->attrs = cell->attrs;
    batch->len++;
}

static inline void batch_flush(LineBatch *batch, const CluTermBuffer *b)
{
    if (!batch || !batch->len)
        return;

    ascii_atlas.nverts = 0, ascii_atlas.nindices = 0;
    SDL_Rect dst = {.x = ctx.f_width * batch->x,
                    .y = ctx.f_height * batch->y,
                    .w = ctx.f_width * batch->len,
                    .h = ctx.f_height};

    background(batch->attrs.bg, &dst);

    for (int dx = 0; dx < batch->len; ++dx) {
        Cell cell = get_display_cell(b, batch->y, batch->x + dx);
        int y = batch->y, x = batch->x + dx;
        if (batch->is_ascii) {
            atlas_push_glyph(&ascii_atlas, &ctx, cell, y, x);
        } else {
            draw_unicode(cell, y, x);
        }
    }
    if (batch->is_ascii)
        atlas_flush(&ascii_atlas, ctx.renderer);

    if (IS_SET(batch->attrs.state, CELL_UNDERLINE))
        underline(batch->attrs.fg, &dst);

    batch->len = 0;
}

static inline void fill_canvas(const CluTerm *term, bool fresh)
{
    const CluTermBuffer *b = ACTIVE_BUFFER(term);

    for (int y = 0; y < b->rows; ++y) {
        LineBatch batch = BATCH(y);
        for (int x = 0; x < b->cols; ++x) {
            if (!fresh && !b->dirty[y * b->cols + x]) {
                batch_flush(&batch, b);
                continue;
            }

            Cell cell = get_display_cell(b, y, x);

            if (!cell_belongs(&batch, &cell))
                batch_flush(&batch, b);
            batch_add(&batch, &cell, x);
        }
        batch_flush(&batch, b);
    }
    memset(b->dirty, 0, b->cols * b->rows * sizeof(*b->dirty));
}

static inline ssize_t clipboard_paste(const CluTerm *term)
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

static inline void handle_keydown(const CluTerm *term, SDL_Event *e)
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

static inline void render(CluTerm *term, bool fresh)
{
    SDL_SetRenderTarget(ctx.renderer, canvas);
    if (fresh) {
        SDL_SetRenderDrawColor(ctx.renderer, UNPACK(DefaultBG), 0);
        SDL_RenderClear(ctx.renderer);
    }
    if (term != NULL)
        fill_canvas(term, fresh);
    SDL_SetRenderTarget(ctx.renderer, NULL);
    SDL_RenderCopy(ctx.renderer, canvas, NULL, NULL);
    SDL_RenderPresent(ctx.renderer);
}

#define FPS(n) (1000 / n)

int main(void)
{
    CluTerm term;
    cluterm_init(&term);
    sdl_init();
    signal(SIGCHLD, quit); // shell exits/crashes.

    ssize_t n         = 0;
    char stream[4096] = {0};
    struct {
        uint w, h, pending : 1;
        uint64_t time;
    } resize = {0};

    render(NULL, 0);
    for (SDL_Event e; running;) {
        if ((n = pty_read(&term.pty, stream, sizeof(stream))) > 0) {
            cluterm_write(&term, stream, n);
            render(&term, 0);
        }

        uint64_t tick = SDL_GetTicks64();
        if (resize.pending && tick - resize.time > FPS(5)) {
            cluterm_resize(&term, resize.h, resize.w);
            atlas_resize(&ascii_atlas, &ctx);
            create_canvas(resize.w * ctx.f_width, resize.h * ctx.f_height);
            render(&term, 1);
            resize.pending = 0, resize.time = tick;
        }

        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT: running = 0; break;

            case SDL_WINDOWEVENT: {
                SDL_WindowEvent *win = &e.window;
                switch (win->event) {
                case SDL_WINDOWEVENT_EXPOSED: render(&term, 1); break;

                case SDL_WINDOWEVENT_SIZE_CHANGED: {
                    resize.w       = MAX(win->data1 / ctx.f_width, 10),
                    resize.h       = MAX(win->data2 / ctx.f_height, 10),
                    resize.pending = 1;
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
        if (canvas)
            SDL_DestroyTexture(canvas);
        atlas_destroy(&ascii_atlas);
        glyph_table_destroy();
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
