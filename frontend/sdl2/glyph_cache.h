#ifndef __SDL2__GLYPH_CACHE_H__
#define __SDL2__GLYPH_CACHE_H__

#include <SDL2/SDL.h>
#include <cluterm/utils.h>
#include <cluterm/vt/buffer.h>

typedef struct GlyphCache {
    SDL_Texture *atlas;
    SDL_Vertex *verts;
    int *indices;
    int nverts, nindices;
} GlyphCache;

void gcache_init(void);
void gcache_destroy(void);
void gcache_resize(void);
void gcache_push_glyph(Cell, int, int);
int gcache_flush(SDL_Renderer *);

#endif
