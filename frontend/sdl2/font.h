#ifndef __SDL2__FONT_H__
#define __SDL2__FONT_H__

#include <SDL2/SDL_ttf.h>
#include <fontconfig/fontconfig.h>

#define dpi() 96

void load_font(FcConfig *, const char *, int, const char *, TTF_Font **);

#endif
