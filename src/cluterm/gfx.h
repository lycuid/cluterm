#ifndef __GFX_H__
#define __GFX_H__

#include <SDL.h>
#include <SDL_ttf.h>
#include <cluterm/gfx/glyph_table.h>
#include <cluterm/terminal/buffer.h>
#include <stdio.h>

#define die(...) (fprintf(stderr, __VA_ARGS__), exit(EXIT_FAILURE))

enum { FontRegular, FontBold, FontItalic, FontBoldItalic };

typedef struct Gfx {
    SDL_Window *window;
    SDL_Renderer *renderer;
    TTF_Font *fonts[4];
    int f_width, f_height;

    void (*init)(void);
    void (*clear)(void);
    void (*draw_cell)(Cell, int, int);
    void (*display)(void);
    void (*destroy)(void);
} Gfx;

extern const Gfx *const gfx;

#endif
