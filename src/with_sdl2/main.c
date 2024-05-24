#include "main.h"
#include "glyph_table.h"
#include <cluterm.h>
#include <cluterm/vte/tty.h>
#include <config.h>
#include <fontconfig/fontconfig.h>
#include <signal.h>

static GFX_Context ctx;
static int running = 1;

void quit(int _arg)
{
    (void)_arg;
    running = 0;
}

static inline void *tryp(void *expr)
{
    if (!expr)
        die("%s\n", SDL_GetError());
    return expr;
}

static inline int tryn(int expr)
{
    if (expr < 0)
        die("%s\n", SDL_GetError());
    return expr;
}

static inline void load_font(const char *pattern, TTF_Font **font)
{
    FcConfig *config = FcInitLoadConfigAndFonts();

    FcPattern *pat = FcNameParse((const FcChar8 *)pattern);
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
        printf("font file: %s (%d).\n", font_file, font_size);
        fflush(stdout);
    }
    FcPatternDestroy(font_pat);
}

static inline void sdl_init(void)
{
    tryn(SDL_Init(SDL_INIT_VIDEO));
    tryn(TTF_Init());
    ctx.window = tryp(SDL_CreateWindow(
        "cluterm", 280, 100, 0, 0, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE));
    ctx.renderer =
        tryp(SDL_CreateRenderer(ctx.window, -1, SDL_RENDERER_ACCELERATED));

    load_font(Fonts[FontRegular], &ctx.fonts[FontRegular]);
    load_font(Fonts[FontBold], &ctx.fonts[FontBold]);
    load_font(Fonts[FontItalic], &ctx.fonts[FontItalic]);
    load_font(Fonts[FontBoldItalic], &ctx.fonts[FontBoldItalic]);

    /* cell size */ {
        char printable_ascii[128 - 32] = {0};
        for (int i = 0, size = LENGTH(printable_ascii); i < size; ++i)
            printable_ascii[i] = i + 32;
        TTF_SizeText(ctx.fonts[FontRegular], printable_ascii, &ctx.f_width,
                     &ctx.f_height);
        ctx.f_width = ctx.f_width / LENGTH(printable_ascii) +
                      (ctx.f_width % LENGTH(printable_ascii) != 0);
        printf("font size: %d %d.\n", ctx.f_width, ctx.f_height);
        fflush(stdout);
    }

    SDL_SetWindowSize(ctx.window,
                      Margin[Left] + Margin[Right] + ctx.f_width * Columns,
                      Margin[Top] + Margin[Bottom] + ctx.f_height * Rows);

    glyph_table_init(&ctx); // glyph table requires fonts to be ready to use.
}

#define UNPACK(c)                                                              \
    ((c) >> (8 * 2)) & 0xff, ((c) >> (8 * 1)) & 0xff, ((c) >> (8 * 0)) & 0xff

__attribute__((unused)) static inline void bounding_box(SDL_Rect *box)
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

static inline void background(Rgb bg, int y, int x)
{
    SDL_SetRenderDrawColor(ctx.renderer, UNPACK(bg), 0);
    SDL_RenderFillRect(ctx.renderer, &(SDL_Rect){.x = ctx.f_width * x,
                                                 .y = ctx.f_height * y,
                                                 .w = ctx.f_width,
                                                 .h = ctx.f_height});
}

static inline void underline(SDL_Rect *rect, Rgb color)
{
    SDL_SetRenderDrawColor(ctx.renderer, UNPACK(color), 0);
    SDL_RenderDrawLine(ctx.renderer, rect->x, rect->y + rect->h - 2,
                       rect->x + rect->w, rect->y + rect->h - 2);
}

void clear(void)
{
    SDL_SetRenderDrawColor(ctx.renderer, UNPACK(DefaultBG), 0);
    SDL_RenderClear(ctx.renderer);
}

void draw_cell(Cell cell, int y, int x)
{
    const Glyph *glyph = glyph_table_request(&ctx, cell);
    SDL_Rect src       = {.x = 0, .y = 0, .w = glyph->w, .h = glyph->h},
             dst       = {.x = Margin[Left] + ctx.f_width * x,
                          .y = Margin[Top] + ctx.f_height * y,
                          .w = glyph->w,
                          .h = glyph->h};
    if (glyph->texture) {
        if (cell.attrs.bg != DefaultBG)
            background(cell.attrs.bg, y, x);
        SDL_RenderCopy(ctx.renderer, glyph->texture, &src, &dst);
    }
    if (IS_SET(cell.attrs.state, CELL_UNDERLINE))
        underline(&dst, cell.attrs.fg);
#if 0
  if (cell.value != ' ')
    bounding_box(&dst);
#endif
}

static inline void create_page(CluTerm *term)
{
    CluTermBuffer *b = ACTIVE_BUFFER(term);
    Cursor *c        = &b->cursor;
    for (int y = 0; y < b->rows; ++y) {
        for (int x = b->cols - 1; x >= 0; --x) {
            Cell cell = b->lines[tline(b, y)][x];
            if (y == c->y && x == c->x && (c->state & CursorHide) == 0) {
                Rgb tmp       = cell.attrs.fg;
                cell.attrs.fg = cell.attrs.bg;
                cell.attrs.bg = tmp;
            }
            if (cell.value == ' ' && cell.attrs.fg == DefaultFG &&
                cell.attrs.bg == DefaultBG)
                continue;
            draw_cell(cell, y, x);
        }
    }
}

static inline void handle_keydown(CluTerm *term, SDL_Event *e)
{
    SDL_KeyboardEvent *key = &e->key;

    char repr[2] = {key->keysym.sym, 0};
    ssize_t n    = 1;

    switch (key->keysym.sym) { // clang-format off
#define SHIFT(key) ((key)->keysym.mod & KMOD_SHIFT)
#define CTRL(key)  ((key)->keysym.mod & KMOD_CTRL)

    case SDLK_a: // fallthrough
    case SDLK_b: // fallthrough
    case SDLK_c: // fallthrough
    case SDLK_d: // fallthrough
    case SDLK_e: // fallthrough
    case SDLK_f: // fallthrough
    case SDLK_g: // fallthrough
    case SDLK_h: // fallthrough
    case SDLK_i: // fallthrough
    case SDLK_j: // fallthrough
    case SDLK_k: // fallthrough
    case SDLK_l: // fallthrough
    case SDLK_m: // fallthrough
    case SDLK_n: // fallthrough
    case SDLK_o: // fallthrough
    case SDLK_p: // fallthrough
    case SDLK_q: // fallthrough
    case SDLK_r: // fallthrough
    case SDLK_s: // fallthrough
    case SDLK_t: // fallthrough
    case SDLK_u: // fallthrough
    case SDLK_v: // fallthrough
    case SDLK_w: // fallthrough
    case SDLK_x: // fallthrough
    case SDLK_y: // fallthrough
    case SDLK_z: { repr[0] -= CTRL(key) ? 96 : SHIFT(key) ? 32 : 0; } break;

    case SDLK_0: { repr[0] = SHIFT(key) ? ')' : repr[0]; } break;
    case SDLK_1: { repr[0] = SHIFT(key) ? '!' : repr[0]; } break;
    case SDLK_2: { repr[0] = SHIFT(key) ? '@' : repr[0]; } break;
    case SDLK_3: { repr[0] = SHIFT(key) ? '#' : repr[0]; } break;
    case SDLK_4: { repr[0] = SHIFT(key) ? '$' : repr[0]; } break;
    case SDLK_5: { repr[0] = SHIFT(key) ? '%' : repr[0]; } break;
    case SDLK_6: { repr[0] = SHIFT(key) ? '^' : repr[0]; } break;
    case SDLK_7: { repr[0] = SHIFT(key) ? '&' : repr[0]; } break;
    case SDLK_8: { repr[0] = SHIFT(key) ? '*' : repr[0]; } break;
    case SDLK_9: { repr[0] = SHIFT(key) ? '(' : repr[0]; } break;

    case SDLK_MINUS:      { repr[0] = SHIFT(key) ? '_' : repr[0]; } break;
    case SDLK_EQUALS:     { repr[0] = SHIFT(key) ? '+' : repr[0]; } break;
    case SDLK_SEMICOLON:  { repr[0] = SHIFT(key) ? ':' : repr[0]; } break;
    case SDLK_QUOTE:      { repr[0] = SHIFT(key) ? '"' : repr[0]; } break;
    case SDLK_COMMA:      { repr[0] = SHIFT(key) ? '<' : repr[0]; } break;
    case SDLK_PERIOD:     { repr[0] = SHIFT(key) ? '>' : repr[0]; } break;
    case SDLK_BACKSLASH:  { repr[0] = SHIFT(key) ? '|' : repr[0]; } break;
    case SDLK_BACKQUOTE:  { repr[0] = SHIFT(key) ? '~' : repr[0]; } break;

    case SDLK_LEFTBRACKET: /* fallthrough. */
    case SDLK_RIGHTBRACKET: { repr[0] += SHIFT(key) ? 32 : 0; } break;
#undef CTRL
#undef SHIFT

    case SDLK_UP:     { tty_write(&term->tty, "\x1b[A", 3); } break;
    case SDLK_DOWN:   { tty_write(&term->tty, "\x1b[B", 3); } break;
    case SDLK_RIGHT:  { tty_write(&term->tty, "\x1b[C", 3); } break;
    case SDLK_LEFT:   { tty_write(&term->tty, "\x1b[D", 3); } break;
    default: break;
    }
    if (BETWEEN(repr[0], 1, 127))
        tty_write(&term->tty, repr, n);
} // clang-format on

int main(void)
{
    CluTerm term;
    cluterm_init(&term);
    sdl_init();
    signal(SIGCHLD, quit);

    SDL_Event e;
    ssize_t n = 0;

#define CLEAR_AND_RENDER(term)                                                 \
    clear();                                                                   \
    if ((term) != NULL)                                                        \
        create_page((term));                                                   \
    SDL_RenderPresent(ctx.renderer);

#define STREAM_SIZE 4096
    char stream[STREAM_SIZE] = {0};

    CLEAR_AND_RENDER(NULL);
    while (running) {
        if ((n = tty_read(&term.tty, stream, STREAM_SIZE)) > 0) {
            cluterm_write(&term, stream, n);
            CLEAR_AND_RENDER(&term);
        }
#undef STREAM_SIZE

        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT: running = 0; break;

            case SDL_WINDOWEVENT: {
                SDL_WindowEvent *win = &e.window;
                switch (win->event) {
                case SDL_WINDOWEVENT_EXPOSED: {
                    CLEAR_AND_RENDER(&term);
                } break;

                case SDL_WINDOWEVENT_SIZE_CHANGED: {
                    cluterm_resize(&term, win->data2 / ctx.f_height,
                                   win->data1 / ctx.f_width);
                    CLEAR_AND_RENDER(&term);
                } break;
                }
            } break;

            case SDL_KEYDOWN: handle_keydown(&term, &e); break;
            default: break;
            }
        }
        SDL_Delay(1);
    }

    cluterm_destroy(&term);
    {
        glyph_table_destroy();
        for (size_t i = 0; i < LENGTH(ctx.fonts); ++i)
            if (ctx.fonts[i])
                TTF_CloseFont(ctx.fonts[i]);
        if (ctx.renderer)
            SDL_DestroyRenderer(ctx.renderer);
        if (ctx.window)
            SDL_DestroyWindow(ctx.window);
        TTF_Quit();
        SDL_Quit();
        printf("SDL cleanup: Done!.\n");
        fflush(stdout);
    }
    return 0;
}
