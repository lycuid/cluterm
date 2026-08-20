#ifndef __SDL2__GLYPH_CACHE_H__
#define __SDL2__GLYPH_CACHE_H__

#include <SDL2/SDL.h>
#include <cluterm/vt/buffer.h>

void gcache_init(void);
void gcache_destroy(void);
void gcache_resize(void);
void gcache_push_glyph(Cell, int, int);
int gcache_flush(void);

#endif
