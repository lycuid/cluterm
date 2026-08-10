#ifndef __SDL2__ATLAS_H__
#define __SDL2__ATLAS_H__

#include "main.h"
#include <cluterm/utils.h>
#include <cluterm/vt/buffer.h>

typedef struct Atlas {
    SDL_Texture *texture;
    SDL_Vertex *verts;
    int *indices;
    int nverts, nindices;
} Atlas;

void atlas_init(Atlas *, const GFX_Context *);
void atlas_destroy(Atlas *);
void atlas_resize(Atlas *, const GFX_Context *);
void atlas_push_glyph(Atlas *, const GFX_Context *, Cell, int, int);
int atlas_flush(Atlas *, SDL_Renderer *);

#endif
