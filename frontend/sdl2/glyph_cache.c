#include "glyph_cache.h"
#include "glyph_cache/lru.h"
#include "main.h"

#define PRINTABLE_ASCII_START 32
#define PRINTABLE_ASCII_END   126
#define TOTAL_ASCII           96

#define ATLAS_WIDTH  200
#define ATLAS_HEIGHT 5
#define CACHE_CAP    (TOTAL_ASCII * 2)

typedef struct Slot {
    int y, x;
} Slot;

bool cell_eq(Cell, Cell);

static GlyphCache cache;

static Slot ascii_slots[4 * TOTAL_ASCII] = {0};
static LRU unicode_cache = {.capacity = CACHE_CAP, .key_eq = cell_eq};

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

static inline void map_surface(SDL_Surface *s, void **pixels, int pitch,
                               Slot *slot)
{
    for (int y = 0; y < s->h; ++y)
        memcpy((uint8_t *)*pixels + (slot->y + y) * pitch + (slot->x * 4),
               (uint8_t *)s->pixels + y * s->pitch, gfx->f_width * 4);
}

static inline Slot *ascii_slot(char ch, int f_index)
{
    int index = f_index * TOTAL_ASCII + (ch - PRINTABLE_ASCII_START);
    return &ascii_slots[index];
}

void gcache_init(void)
{
    int atlas_w = ATLAS_WIDTH * gfx->f_width,
        atlas_h = ATLAS_HEIGHT * gfx->f_height;
    cache.atlas =
        SDL_CreateTexture(gfx->renderer, SDL_PIXELFORMAT_RGBA8888,
                          SDL_TEXTUREACCESS_STREAMING, atlas_w, atlas_h);

    SDL_SetTextureBlendMode(cache.atlas, SDL_BLENDMODE_BLEND);

    void *pixels;
    int pitch;
    SDL_LockTexture(cache.atlas, NULL, &pixels, &pitch);
    for (int y = 0; y < atlas_h; ++y)
        memset((uint8_t *)pixels + y * pitch, 0, pitch);

    int nfonts = LENGTH(gfx->fonts);
    for (int f_index = 0; f_index < nfonts; ++f_index) {
        for (int ch = PRINTABLE_ASCII_START; ch <= PRINTABLE_ASCII_END; ch++) {
            Slot *slot = ascii_slot(ch, f_index);

            slot->x = ((f_index % 2) * TOTAL_ASCII * gfx->f_width) +
                      (ch - PRINTABLE_ASCII_START) * gfx->f_width;
            slot->y = f_index / 2 * gfx->f_height;

            SDL_Surface *surface = create_surface(ch, gfx->fonts[f_index]);
            if (!surface)
                continue;

            map_surface(surface, &pixels, pitch, slot);
            SDL_FreeSurface(surface);
        }
    }
    SDL_UnlockTexture(cache.atlas);
    gcache_resize();
    cache.nverts = 0, cache.nindices = 0;
}

void gcache_resize(void)
{
    int w;
    SDL_GetWindowSize(gfx->window, &w, NULL);
    w = w / gfx->f_width + 10; // offset 10 (for eg. cursor etc).

    cache.verts   = realloc(cache.verts, w * 4 * sizeof(SDL_Vertex));
    cache.indices = realloc(cache.indices, w * 6 * sizeof(int));
}

void gcache_destroy(void)
{
    if (cache.verts)
        free(cache.verts);
    if (cache.indices)
        free(cache.indices);
    if (cache.atlas)
        SDL_DestroyTexture(cache.atlas);
}

static inline Slot *get_slot(Cell cell)
{
    int f_index = font_index(cell.attrs.state);
    if (BETWEEN(cell.value, PRINTABLE_ASCII_START, PRINTABLE_ASCII_END))
        return ascii_slot(cell.value, f_index);

    Slot *slot = lru_get(&unicode_cache, cell);
    if (!slot) {
        Slot *stale =
            lru_put(&unicode_cache, cell, (slot = calloc(1, sizeof(Slot))));
        if (stale) {
            slot->x = stale->x, slot->y = stale->y;
        } else {
            size_t cache_size = CACHE_CAP - unicode_cache.capacity;

            slot->x = gfx->f_width * (cache_size % CACHE_CAP);
            slot->y = (gfx->f_height * 2) +
                      (gfx->f_height * (cache_size / CACHE_CAP));
        }

        UTF8_String utf8_string = {0};
        utf8_encode(cell.value, utf8_string);

        SDL_Surface *surface = create_surface(cell.value, gfx->fonts[f_index]);
        SDL_UpdateTexture(cache.atlas,
                          &(SDL_Rect){.x = slot->x,
                                      .y = slot->y,
                                      .w = gfx->f_width,
                                      .h = gfx->f_height},
                          surface->pixels, surface->pitch);
        SDL_FreeSurface(surface);
    }
    return slot;
}

void gcache_push_glyph(Cell cell, int y, int x)
{
    y = y * gfx->f_height, x = x * gfx->f_width;

    Slot *slot = get_slot(cell);
    if (!slot)
        return;

    float atlas_w = (ATLAS_WIDTH * gfx->f_width),
          atlas_h = ATLAS_HEIGHT * gfx->f_height;

    float u0 = slot->x / atlas_w, u1 = (slot->x + gfx->f_width) / atlas_w,
          v0 = slot->y / atlas_h, v1 = (slot->y + gfx->f_height) / atlas_h;

    int base = cache.nverts;

    cache.verts[cache.nverts++] =
        (SDL_Vertex){.position  = {x, y},
                     .tex_coord = {u0, v0},
                     .color     = {UNPACK(cell.attrs.fg), 0xff}};
    cache.verts[cache.nverts++] =
        (SDL_Vertex){.position  = {x + gfx->f_width, y},
                     .tex_coord = {u1, v0},
                     .color     = {UNPACK(cell.attrs.fg), 0xff}};
    cache.verts[cache.nverts++] =
        (SDL_Vertex){.position  = {x + gfx->f_width, y + gfx->f_height},
                     .tex_coord = {u1, v1},
                     .color     = {UNPACK(cell.attrs.fg), 0xff}};
    cache.verts[cache.nverts++] =
        (SDL_Vertex){.position  = {x, y + gfx->f_height},
                     .tex_coord = {u0, v1},
                     .color     = {UNPACK(cell.attrs.fg), 0xff}};

    cache.indices[cache.nindices++] = base + 0;
    cache.indices[cache.nindices++] = base + 1;
    cache.indices[cache.nindices++] = base + 2;
    cache.indices[cache.nindices++] = base + 0;
    cache.indices[cache.nindices++] = base + 2;
    cache.indices[cache.nindices++] = base + 3;
}

int gcache_flush(SDL_Renderer *renderer)
{
    int res = 0;
    if (cache.nverts && cache.nindices) {
        res = SDL_RenderGeometry(renderer, cache.atlas, cache.verts,
                                 cache.nverts, cache.indices, cache.nindices);
        cache.nverts = 0, cache.nindices = 0;
    }
    return res;
}
