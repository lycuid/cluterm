#ifndef __SDL2__ATLAS_H__
#define __SDL2__ATLAS_H__

#include <SDL2/SDL.h>
#include <cluterm/utils.h>
#include <cluterm/vt/buffer.h>

typedef struct Atlas {
    SDL_Texture *texture;
    SDL_Vertex *verts;
    int *indices;
    int nverts, nindices;
} Atlas;

void atlas_init(Atlas *);
void atlas_destroy(Atlas *);
void atlas_resize(Atlas *);
void atlas_push_glyph(Atlas *, Cell, int, int);
int atlas_flush(Atlas *, SDL_Renderer *);

#endif
