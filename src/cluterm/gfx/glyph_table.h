#ifndef __RENDERER__GLYPH_H__
#define __RENDERER__GLYPH_H__

#include <SDL.h>
#include <cluterm/terminal/buffer.h>

typedef struct Glyph {
    SDL_Texture *texture;
    int w, h;
} Glyph;

void glyph_table_init(void);
const Glyph *glyph_table_request(Cell);
void glyph_table_destroy(void);

#endif
