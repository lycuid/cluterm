#ifndef __SDL2__MAIN_H__
#define __SDL2__MAIN_H__

#include "pty.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <cluterm.h>
#include <stdbool.h>

#define die(code, ...)                                                         \
    do {                                                                       \
        debug(__VA_ARGS__);                                                    \
        exit(code);                                                            \
    } while (0)

typedef struct Dpi {
    float d, h, v;
} Dpi;

typedef struct GFX_Context {
    SDL_Window *window;
    SDL_Renderer *renderer;
    TTF_Font *fonts[4];

    int f_width, f_height;

    Cluterm term;
    pty_t pty;
} GFX_Context;

extern const GFX_Context *gfx;

void gfx_rebuild(void);
void gfx_request_render(bool);
void gfx_display_dpi(int, Dpi *);

#endif
