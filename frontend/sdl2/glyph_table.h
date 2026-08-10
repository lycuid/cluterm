#ifndef __WITH_SDL2__GLYPH_TABLE_H__
#define __WITH_SDL2__GLYPH_TABLE_H__

#include "main.h"
#include <SDL.h>
#include <cluterm/vt/buffer.h>

typedef struct Glyph {
    SDL_Texture *texture;
    int w, h;
} Glyph;

void glyph_table_init(void);
const Glyph *glyph_table_request_unicode(const GFX_Context *, Cell);
void glyph_table_destroy(void);

#endif
