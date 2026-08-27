#include "glyph_cache.h"
#include "glyph_cache/lru.h"
#include "main.h"
#include <cluterm/colors.h>
#include <cluterm/config.h>

#define PRINTABLE_ASCII_START 32
#define PRINTABLE_ASCII_END   126
#define TOTAL_ASCII           96

#define ATLAS_COLS 200
#define ATLAS_ROWS 8
#define CACHE_CAP  (ATLAS_COLS * 3)

typedef struct AtlasSlot {
    int y, x, w, h;
} AtlasSlot;

bool cell_eq(Cell, Cell);

static int atlas_cell_width, atlas_cell_height;

static struct GlyphAtlas {
    SDL_Texture *texture;
    SDL_Vertex *verts;
    int *indices;
    int nverts, nindices;
} atlas = {0};

static AtlasSlot ascii_slots[4 * TOTAL_ASCII] = {0};
static LRU non_ascii_cache[2]                 = {
    {.capacity = CACHE_CAP, .key_eq = cell_eq},
    {.capacity = CACHE_CAP / 2, .key_eq = cell_eq},
};

static inline int font_index(CellState state)
{
    return IS_SET(state, CELL_BOLD | CELL_ITALIC) ? FontBoldItalic
           : IS_SET(state, CELL_BOLD)             ? FontBold
           : IS_SET(state, CELL_ITALIC)           ? FontItalic
                                                  : FontRegular;
}

bool cell_eq(Cell c1, Cell c2)
{
    return c1.value == c2.value && c1.attrs.state == c2.attrs.state;
}

static inline SDL_Surface *create_surface(Rune ch, TTF_Font *font)
{
    UTF8_String utf8_string = {0};
    utf8_encode(ch, utf8_string);
    if (strlen(utf8_string) == 0)
        return NULL;

    SDL_Surface *text =
        TTF_RenderUTF8_Blended(font, (char *)utf8_string, Color(0xffffff));
    if (!text)
        return NULL;

    SDL_Surface *surface =
        SDL_ConvertSurfaceFormat(text, SDL_PIXELFORMAT_RGBA8888, 0);
    SDL_FreeSurface(text);
    return surface;
}

static inline AtlasSlot *ascii_slot(char ch, int f_index)
{
    int index = f_index * TOTAL_ASCII + (ch - PRINTABLE_ASCII_START);
    return &ascii_slots[index];
}

void gcache_init(void)
{
    atlas_cell_width  = 1.2f * gfx->f_width,
    atlas_cell_height = 1.2f * gfx->f_height;

    int atlas_w = ATLAS_COLS * atlas_cell_width,
        atlas_h = ATLAS_ROWS * atlas_cell_height;
    atlas.texture =
        SDL_CreateTexture(gfx->renderer, SDL_PIXELFORMAT_RGBA8888,
                          SDL_TEXTUREACCESS_STREAMING, atlas_w, atlas_h);
    SDL_SetTextureBlendMode(atlas.texture, SDL_BLENDMODE_BLEND);

#ifdef DEBUG_ATLAS
    debug_texture =
        SDL_CreateTexture(debug_renderer, SDL_PIXELFORMAT_RGBA8888,
                          SDL_TEXTUREACCESS_STREAMING, atlas_w, atlas_h);
    SDL_SetTextureBlendMode(debug_texture, SDL_BLENDMODE_BLEND);
#endif

    int nfonts = LENGTH(gfx->fonts);
    for (int f_index = 0; f_index < nfonts; ++f_index) {
        for (int ch = PRINTABLE_ASCII_START; ch <= PRINTABLE_ASCII_END; ch++) {
            AtlasSlot *slot = ascii_slot(ch, f_index);

            slot->x = ((f_index % 2) * TOTAL_ASCII * atlas_cell_width) +
                      (ch - PRINTABLE_ASCII_START) * atlas_cell_width;
            slot->y = f_index / 2 * atlas_cell_height;

            SDL_Surface *surface = create_surface(ch, gfx->fonts[f_index]);
            if (!surface)
                continue;

            slot->w = surface->w, slot->h = surface->h;
            SDL_UpdateTexture(
                atlas.texture,
                &(SDL_Rect){
                    .x = slot->x, .y = slot->y, .w = slot->w, .h = slot->h},
                surface->pixels, surface->pitch);
#ifdef DEBUG_ATLAS
            SDL_UpdateTexture(
                debug_texture,
                &(SDL_Rect){
                    .x = slot->x, .y = slot->y, .w = slot->w, .h = slot->h},
                surface->pixels, surface->pitch);
#endif
            SDL_FreeSurface(surface);
        }
    }
    gcache_resize(cfg->rows, cfg->cols);
    atlas.nverts = 0, atlas.nindices = 0;
}

void gcache_destroy(void)
{
    if (atlas.verts) {
        free(atlas.verts);
        atlas.verts = NULL;
    }
    if (atlas.indices) {
        free(atlas.indices);
        atlas.indices = NULL;
    }
    if (atlas.texture) {
        SDL_DestroyTexture(atlas.texture);
        atlas.texture = NULL;
    }
#ifdef DEBUG_ATLAS
    if (debug_texture) {
        SDL_DestroyTexture(debug_texture);
        debug_texture = NULL;
    }
#endif
    while (non_ascii_cache[0].stale)
        free(lru_evict(&non_ascii_cache[0]));
    while (non_ascii_cache[1].stale)
        free(lru_evict(&non_ascii_cache[1]));
}

void gcache_resize(__attribute__((unused)) int rows, int cols)
{
    // offset 2 (for eg. cursor etc).
    int w         = cols + 2;
    atlas.verts   = realloc(atlas.verts, w * 4 * sizeof(SDL_Vertex));
    atlas.indices = realloc(atlas.indices, w * 6 * sizeof(int));
}

static inline AtlasSlot *get_slot(Cell cell)
{
    int f_index = font_index(cell.attrs.state);
    if (BETWEEN(cell.value, PRINTABLE_ASCII_START, PRINTABLE_ASCII_END))
        return ascii_slot(cell.value, f_index);

    AtlasSlot *slot;
    if ((slot = lru_get(&non_ascii_cache[0], cell)))
        return slot;
    if ((slot = lru_get(&non_ascii_cache[1], cell)))
        return slot;

    UTF8_String utf8_string = {0};
    utf8_encode(cell.value, utf8_string);

    SDL_Surface *surface = create_surface(cell.value, gfx->fonts[f_index]);
    if (!surface)
        return NULL;

    slot    = calloc(1, sizeof(AtlasSlot));
    slot->w = surface->w, slot->h = surface->h;

    int c_index = slot->w > gfx->f_width;
    LRU *cache  = &non_ascii_cache[c_index];

    AtlasSlot *stale = lru_put(cache, cell, slot);
    if (stale) { // eviction happened here, reuse the same coords.
        slot->x = stale->x, slot->y = stale->y;
    } else {
        size_t cache_size = CACHE_CAP / (c_index + 1) - cache->capacity;
        int cols          = ATLAS_COLS / (c_index + 1);

        slot->x = atlas_cell_width * (c_index + 1) * (cache_size % cols);
        slot->y = (atlas_cell_height * 2) + // offset for fixed ascii slots.
                  (atlas_cell_height * 3 *
                   c_index) + // offset for non ascii within f_width.
                  (atlas_cell_height * (cache_size / cols));
    }
    free(stale);

    int ascent = TTF_FontAscent(gfx->fonts[f_index]);
    int min_x, max_y;
    TTF_GlyphMetrics32(gfx->fonts[f_index], cell.value, &min_x, 0, 0, &max_y,
                       0);
    int dy = ascent - max_y, dx = min_x;

    SDL_UpdateTexture(atlas.texture,
                      // clipping negative bearing for both y, x.
                      &(SDL_Rect){.x = slot->x + MIN(0, dx),
                                  .y = slot->y + MIN(0, dy),
                                  .w = slot->w,
                                  .h = slot->h},
                      surface->pixels, surface->pitch);
#ifdef DEBUG_ATLAS
    SDL_UpdateTexture(debug_texture,
                      &(SDL_Rect){.x = slot->x + MIN(0, dx),
                                  .y = slot->y + MIN(0, dy),
                                  .w = slot->w,
                                  .h = slot->h},
                      surface->pixels, surface->pitch);
#endif
    SDL_FreeSurface(surface);
    return slot;
}

void gcache_emit(Cell cell, int y, int x)
{
    AtlasSlot *slot = get_slot(cell);
    if (!slot)
        return;

    y = y * gfx->f_height, x = x * gfx->f_width;
    float atlas_w = ATLAS_COLS * atlas_cell_width,
          atlas_h = ATLAS_ROWS * atlas_cell_height;

    float u0 = slot->x / atlas_w, u1 = (slot->x + slot->w) / atlas_w,
          v0 = slot->y / atlas_h,
          v1 = (slot->y + MIN(gfx->f_height, slot->h)) / atlas_h;

    int base_index = atlas.nverts;

    atlas.verts[atlas.nverts++] =
        (SDL_Vertex){.position  = {x, y},
                     .tex_coord = {u0, v0},
                     .color     = {UNPACK(cell.attrs.fg), 0xff}};
    atlas.verts[atlas.nverts++] =
        (SDL_Vertex){.position  = {x + slot->w, y},
                     .tex_coord = {u1, v0},
                     .color     = {UNPACK(cell.attrs.fg), 0xff}};
    atlas.verts[atlas.nverts++] =
        (SDL_Vertex){.position  = {x + slot->w, y + gfx->f_height},
                     .tex_coord = {u1, v1},
                     .color     = {UNPACK(cell.attrs.fg), 0xff}};
    atlas.verts[atlas.nverts++] =
        (SDL_Vertex){.position  = {x, y + gfx->f_height},
                     .tex_coord = {u0, v1},
                     .color     = {UNPACK(cell.attrs.fg), 0xff}};

    atlas.indices[atlas.nindices++] = base_index + 0;
    atlas.indices[atlas.nindices++] = base_index + 1;
    atlas.indices[atlas.nindices++] = base_index + 2;
    atlas.indices[atlas.nindices++] = base_index + 0;
    atlas.indices[atlas.nindices++] = base_index + 2;
    atlas.indices[atlas.nindices++] = base_index + 3;
}

int gcache_flush(void)
{
    int res = 0;
    if (atlas.nverts && atlas.nindices) {
        res = SDL_RenderGeometry(gfx->renderer, atlas.texture, atlas.verts,
                                 atlas.nverts, atlas.indices, atlas.nindices);
        atlas.nverts = 0, atlas.nindices = 0;
    }
    return res;
}
