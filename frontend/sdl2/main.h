#ifndef __WITH_SDL2__MAIN_H__
#define __WITH_SDL2__MAIN_H__

#include <SDL.h>
#include <SDL_ttf.h>

#define die(...) (void)0;

typedef struct GFX_Context {
    SDL_Window *window;
    SDL_Renderer *renderer;
    TTF_Font *fonts[4];
    int f_width, f_height;
} GFX_Context;

#endif
