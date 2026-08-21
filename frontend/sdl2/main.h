#ifndef __SDL2__MAIN_H__
#define __SDL2__MAIN_H__

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#define die(code, ...)                                                         \
    do {                                                                       \
        debug(__VA_ARGS__);                                                    \
        exit(code);                                                            \
    } while (0)

typedef enum UserEvent {
    USEREVENT_SET_TITLE,
} UserEvent;

typedef struct GFX_Context {
    SDL_Window *window;
    SDL_Renderer *renderer;
    TTF_Font *fonts[4];
    int f_width, f_height;
} GFX_Context;

extern const GFX_Context *gfx;

#endif
