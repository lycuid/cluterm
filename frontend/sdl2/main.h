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

typedef struct Dpi {
    float d, h, v;
} Dpi;

typedef struct GFX_Context {
    SDL_Window *window;
    SDL_Renderer *renderer;
    TTF_Font *fonts[4];

    int f_width, f_height;

    Cluterm term;
} GFX_Context;

extern const GFX_Context *gfx;

ssize_t gfx_write(const char *, size_t);
void gfx_rebuild(void);
char *gfx_selected_text(void);

void select_start(int, int);
void select_update(int, int);
void select_word(int, int);
void select_line(int);
void select_clear(void);
bool select_contains(int, int, int);

#endif
