#include "atlas.h"

#define PRINTABLE_ASCII_START 32
#define PRINTABLE_ASCII_END   126
#define TOTAL_ASCII           96

#define ATLAS_WIDTH  200
#define ATLAS_HEIGHT 5

typedef struct slot_t {
    int x, y;
} slot_t;

static inline int font_index(CellAttributes *attrs)
{
    return IS_SET(attrs->state, CELL_BOLD | CELL_ITALIC) ? FontBoldItalic
           : IS_SET(attrs->state, CELL_BOLD)             ? FontBold
           : IS_SET(attrs->state, CELL_ITALIC)           ? FontItalic
                                                         : FontRegular;
}

static inline bool get_slot(const GFX_Context *ctx, Rune value, int f_index,
                            slot_t *slot)
{
    memset(slot, 0, sizeof(slot_t));
    slot->x = ((f_index % 2) * TOTAL_ASCII * ctx->f_width) +
              (value - PRINTABLE_ASCII_START) * ctx->f_width;
    slot->y = f_index * ctx->f_height;
    return true;
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

static inline void map_surface(SDL_Surface *surface, void **pixels, int pitch,
                               int w, slot_t *slot)
{
    for (int y = 0; y < surface->h; ++y)
        memcpy((uint8_t *)*pixels + (slot->y + y) * pitch + (slot->x * 4),
               (uint8_t *)surface->pixels + y * surface->pitch, w * 4);
}

void atlas_init(Atlas *atlas, const GFX_Context *ctx)
{
    int atlas_w = ATLAS_WIDTH * ctx->f_width,
        atlas_h = ATLAS_HEIGHT * ctx->f_height;
    atlas->texture =
        SDL_CreateTexture(ctx->renderer, SDL_PIXELFORMAT_RGBA8888,
                          SDL_TEXTUREACCESS_STREAMING, atlas_w, atlas_h);

    SDL_SetTextureBlendMode(atlas->texture, SDL_BLENDMODE_BLEND);

    void *pixels;
    int pitch;
    SDL_LockTexture(atlas->texture, NULL, &pixels, &pitch);
    for (int y = 0; y < atlas_h; ++y)
        memset((uint8_t *)pixels + y * pitch, 0, pitch);

    int nfonts = LENGTH(ctx->fonts);
    slot_t slot;
    for (int f_index = 0; f_index < nfonts; ++f_index) {
        for (int ch = PRINTABLE_ASCII_START; ch <= PRINTABLE_ASCII_END; ch++) {
            SDL_Surface *surface = create_surface(ch, ctx->fonts[f_index]);
            if (!surface)
                continue;

            get_slot(ctx, ch, f_index, &slot);
            map_surface(surface, &pixels, pitch, ctx->f_width, &slot);
            SDL_FreeSurface(surface);
        }
    }
    SDL_UnlockTexture(atlas->texture);
    atlas_resize(atlas, ctx);
    atlas->nverts = 0, atlas->nindices = 0;
}

void atlas_resize(Atlas *atlas, const GFX_Context *ctx)
{
    int w;
    SDL_GetWindowSize(ctx->window, &w, NULL);
    w /= ctx->f_width;

    atlas->verts   = realloc(atlas->verts, w * 4 * sizeof(SDL_Vertex));
    atlas->indices = realloc(atlas->indices, w * 6 * sizeof(int));
}

SDL_Texture *atlas_texture(Atlas *atlas) { return atlas->texture; }

void atlas_destroy(Atlas *atlas)
{
    if (atlas->verts)
        free(atlas->verts);
    if (atlas->indices)
        free(atlas->indices);
    if (atlas->texture)
        SDL_DestroyTexture(atlas->texture);
}

void atlas_push_glyph(Atlas *atlas, const GFX_Context *ctx, Cell cell, int y,
                      int x)
{
    y = y * ctx->f_height, x = x * ctx->f_width;

    slot_t slot;
    if (!get_slot(ctx, cell.value, font_index(&cell.attrs), &slot))
        return;

    float atlas_w = (ATLAS_WIDTH * ctx->f_width),
          atlas_h = ATLAS_HEIGHT * ctx->f_height;

    float u0 = slot.x / atlas_w, u1 = (slot.x + ctx->f_width) / atlas_w,
          v0 = slot.y / atlas_h, v1 = (slot.y + ctx->f_height) / atlas_h;

    int base = atlas->nverts;

    atlas->verts[atlas->nverts++] = (SDL_Vertex){
        .position  = {x, y},
        .tex_coord = {u0, v0},
        .color     = {UNPACK(cell.attrs.fg), 0xff},
    };
    atlas->verts[atlas->nverts++] = (SDL_Vertex){
        .position  = {x + ctx->f_width, y},
        .tex_coord = {u1, v0},
        .color     = {UNPACK(cell.attrs.fg), 0xff},
    };
    atlas->verts[atlas->nverts++] = (SDL_Vertex){
        .position  = {x + ctx->f_width, y + ctx->f_height},
        .tex_coord = {u1, v1},
        .color     = {UNPACK(cell.attrs.fg), 0xff},
    };
    atlas->verts[atlas->nverts++] = (SDL_Vertex){
        .position  = {x, y + ctx->f_height},
        .tex_coord = {u0, v1},
        .color     = {UNPACK(cell.attrs.fg), 0xff},
    };

    atlas->indices[atlas->nindices++] = base + 0;
    atlas->indices[atlas->nindices++] = base + 1;
    atlas->indices[atlas->nindices++] = base + 2;
    atlas->indices[atlas->nindices++] = base + 0;
    atlas->indices[atlas->nindices++] = base + 2;
    atlas->indices[atlas->nindices++] = base + 3;
}

int atlas_flush(Atlas *atlas, SDL_Renderer *renderer)
{
    return SDL_RenderGeometry(renderer, atlas->texture, atlas->verts,
                              atlas->nverts, atlas->indices, atlas->nindices);
}
