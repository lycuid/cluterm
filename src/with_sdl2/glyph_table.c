#include "glyph_table.h"
#include "glyph_table/cache.h"
#include "main.h"
#include <SDL.h>
#include <SDL_ttf.h>
#include <cluterm/buffer.h>
#include <cluterm/utf8.h>
#include <config.h>
#include <stdbool.h>

#define PRINTABLE_ASCII_START 32
#define PRINTABLE_ASCII_END   127

#define IS_PRINTABLE_ASCII(value)                                              \
    BETWEEN((value), PRINTABLE_ASCII_START, PRINTABLE_ASCII_END)

#define IS_DEFAULT_CELL(cell)                                                  \
    ((cell)->attrs.fg == DefaultFG && (cell)->attrs.bg == DefaultBG &&         \
     (cell)->attrs.state == 0x0)

static Glyph printable_ascii_glyph[128];
static GlyphCache glyph_cache;
static uint64_t CacheHit = 0, CacheMiss = 0;

bool cell_eq(Cell c1, Cell c2)
{
    // @NOTE: use 'cell.atts.bg' if bg is used to create texture stored in the
    // glyph.
    // right now, bg is instead just a rect of the same color drawn behind the
    // cell, instead of being part of the glyph.
    return c1.value == c2.value && c1.attrs.fg == c2.attrs.fg &&
           c1.attrs.state == c2.attrs.state;
}

void glyph_dealloc(Glyph *glyph)
{
    SDL_DestroyTexture(glyph->texture);
    free(glyph);
}

static inline TTF_Font *get_font(GFX_Context *ctx, Cell *cell)
{
    return ctx->fonts[IS_SET(cell->attrs.state, CELL_BOLD | CELL_ITALIC)
                          ? FontBoldItalic
                      : IS_SET(cell->attrs.state, CELL_BOLD)   ? FontBold
                      : IS_SET(cell->attrs.state, CELL_ITALIC) ? FontItalic
                                                               : FontRegular];
}

static inline void glyph_create(GFX_Context *ctx, Glyph *glyph, Cell cell)
{
    UTF8_String utf8_string = {' '};
    utf8_encode(cell.value, utf8_string);
    SDL_Surface *surface = TTF_RenderUTF8_Blended(
        get_font(ctx, &cell), (char *)utf8_string, Color(cell.attrs.fg));

    // @NOTE: This can fail if surface is NULL.
    if (surface) {
        glyph->texture = SDL_CreateTextureFromSurface(ctx->renderer, surface);
        glyph->w = surface->w, glyph->h = surface->h;
        SDL_FreeSurface(surface);
        CacheHit--, CacheMiss++;
    }
}

void glyph_table_init(GFX_Context *ctx)
{
    for (int i = PRINTABLE_ASCII_START; i <= PRINTABLE_ASCII_END; ++i)
        glyph_create(ctx, &printable_ascii_glyph[i], DEFAULT_CELL(i));
    glyph_cache = (GlyphCache){.capacity      = (30 * 104) + (1 << 5),
                               .key_eq        = cell_eq,
                               .value_dealloc = glyph_dealloc};
    CacheHit = CacheMiss = 0;
}

const Glyph *glyph_table_request(GFX_Context *ctx, Cell cell)
{
    CacheHit++;
    if (IS_DEFAULT_CELL(&cell) && IS_PRINTABLE_ASCII(cell.value))
        return &printable_ascii_glyph[cell.value];

    Glyph *glyph = gcache_get(&glyph_cache, cell);
    if (!glyph) {
        glyph_create(ctx, (glyph = calloc(1, sizeof(Glyph))), cell);
        gcache_put(&glyph_cache, cell, glyph);
    }

    return glyph;
}

void glyph_table_destroy(void)
{
    for (int i = PRINTABLE_ASCII_START; i <= PRINTABLE_ASCII_END; ++i)
        SDL_DestroyTexture(printable_ascii_glyph[i].texture);
    gcache_clear(&glyph_cache);
    printf("Cache cleanup: Done!.\n");
    printf("Cache hit/miss: %ld/%ld.\n", CacheHit, CacheMiss);
}
