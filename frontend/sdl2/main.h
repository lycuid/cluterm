#ifndef __SDL2__MAIN_H__
#define __SDL2__MAIN_H__

#include <SDL.h>
#include <SDL_ttf.h>

#define UNPACK(c)                                                              \
    ((c) >> (8 * 2)) & 0xff, ((c) >> (8 * 1)) & 0xff, ((c) >> (8 * 0)) & 0xff

#define die(...)                                                               \
    do {                                                                       \
        debug(__VA_ARGS__);                                                    \
        exit(1);                                                               \
    } while (0)

typedef struct GFX_Context {
    SDL_Window *window;
    SDL_Renderer *renderer;
    TTF_Font *fonts[4];
    int f_width, f_height;
} GFX_Context;

#endif
