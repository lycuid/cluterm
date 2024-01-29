#include "gfx.h"
#include <cluterm/utils.h>
#include <config.h>
#include <fontconfig/fontconfig.h>

void gfx_init(void);
void gfx_clear(void);
void gfx_draw_cell(Cell, int, int);
void gfx_display(void);
void gfx_destroy(void);

static Gfx local     = {.init      = gfx_init,
                        .clear     = gfx_clear,
                        .draw_cell = gfx_draw_cell,
                        .display   = gfx_display,
                        .destroy   = gfx_destroy};
const Gfx *const gfx = &local;

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
        printf("font file: %s.\n", font_file);
    }
    FcPatternDestroy(font_pat);
}

#define UNPACK(c)                                                              \
    ((c) >> (8 * 2)) & 0xff, ((c) >> (8 * 1)) & 0xff, ((c) >> (8 * 0)) & 0xff

__attribute__((unused)) static inline void bounding_box(SDL_Rect *box)
{
    SDL_SetRenderDrawColor(local.renderer, UNPACK(0x303030), 0);
    SDL_RenderDrawLines(
        local.renderer,
        (SDL_Point[]){{.x = box->x, .y = box->y},
                      {.x = box->x + box->w, .y = box->y},
                      {.x = box->x + box->w, .y = box->y + box->h},
                      {.x = box->x, .y = box->y + box->h},
                      {.x = box->x, .y = box->y}},
        5);
}

static inline void underline(SDL_Rect *rect, Rgb color)
{
    SDL_SetRenderDrawColor(local.renderer, UNPACK(color), 0);
    SDL_RenderDrawLine(local.renderer, rect->x, rect->y + rect->h - 2,
                       rect->x + rect->w, rect->y + rect->h - 2);
}

void gfx_init(void)
{
    tryn(SDL_Init(SDL_INIT_VIDEO));
    tryn(TTF_Init());
    local.window = tryp(SDL_CreateWindow(
        "cluterm", 280, 100, 0, 0, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE));
    local.renderer =
        tryp(SDL_CreateRenderer(local.window, -1, SDL_RENDERER_ACCELERATED));

    load_font(Fonts[FontRegular], &local.fonts[FontRegular]);
    load_font(Fonts[FontBold], &local.fonts[FontBold]);
    load_font(Fonts[FontItalic], &local.fonts[FontItalic]);
    load_font(Fonts[FontBoldItalic], &local.fonts[FontBoldItalic]);

    /* cell size */ {
        char printable_ascii[128 - 32] = {0};
        for (int i = 0, size = LENGTH(printable_ascii); i < size; ++i)
            printable_ascii[i] = i + 32;
        TTF_SizeText(local.fonts[FontRegular], printable_ascii, &local.f_width,
                     &local.f_height);
        local.f_width = local.f_width / LENGTH(printable_ascii) +
                        (local.f_width % LENGTH(printable_ascii) != 0);
        printf("font size: %d %d.\n", local.f_width, local.f_height);
    }

    SDL_SetWindowSize(local.window,
                      Margin[Left] + Margin[Right] + local.f_width * Columns,
                      Margin[Top] + Margin[Bottom] + local.f_height * Rows);

    glyph_table_init(); // glyph table requires fonts to be ready to use.
}

void gfx_clear(void)
{
    SDL_SetRenderDrawColor(local.renderer, UNPACK(DefaultBG), 0);
    SDL_RenderClear(local.renderer);
}

void gfx_draw_cell(Cell cell, int y, int x)
{
    const Glyph *glyph = glyph_table_request(cell);
    SDL_Rect src       = {.x = 0, .y = 0, .w = glyph->w, .h = glyph->h},
             dst       = {.x = Margin[Left] + local.f_width * x,
                          .y = Margin[Top] + local.f_height * y,
                          .w = glyph->w,
                          .h = glyph->h};
    if (glyph->texture)
        SDL_RenderCopy(local.renderer, glyph->texture, &src, &dst);
    if (IS_SET(cell.attrs.state, CELL_UNDERLINE))
        underline(&dst, cell.attrs.fg);
#if 0
  if (cell.value != ' ')
    bounding_box(&dst);
#endif
}

void gfx_display(void) { SDL_RenderPresent(local.renderer); }

void gfx_destroy(void)
{
    glyph_table_destroy();
    for (size_t i = 0; i < LENGTH(local.fonts); ++i)
        if (local.fonts[i])
            TTF_CloseFont(local.fonts[i]);
    if (local.renderer)
        SDL_DestroyRenderer(local.renderer);
    if (local.window)
        SDL_DestroyWindow(local.window);
    TTF_Quit();
    SDL_Quit();
    printf("Core cleanup: Done!.\n");
}
