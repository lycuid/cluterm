#ifndef __SDL2__MAIN_H__
#define __SDL2__MAIN_H__

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <cluterm.h>
#include <stdbool.h>

#define die(code, ...)                                                         \
    do {                                                                       \
        debug(__VA_ARGS__);                                                    \
        exit(code);                                                            \
    } while (0)

typedef struct GFX_Context {
    SDL_Window *window;
    SDL_Renderer *renderer;
    TTF_Font *fonts[4];
    int f_width, f_height;
    float hdpi, vdpi;
} GFX_Context;

extern const GFX_Context *gfx;

void gfx_rebuild(Cluterm *);
void gfx_request_render(bool);

#ifdef DEBUG_ATLAS
extern SDL_Window *debug_window;
extern SDL_Renderer *debug_renderer;
extern SDL_Texture *debug_texture;
#endif

#endif
