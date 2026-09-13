#ifndef __SDL2__HANDLERS__KEYPRESS_H__
#define __SDL2__HANDLERS__KEYPRESS_H__

#include <SDL2/SDL.h>
#include <cluterm.h>

#define send_arrow_up(term, ...)    send_arrow(term, 'A', __VA_ARGS__)
#define send_arrow_down(term, ...)  send_arrow(term, 'B', __VA_ARGS__)
#define send_arrow_right(term, ...) send_arrow(term, 'C', __VA_ARGS__)
#define send_arrow_left(term, ...)  send_arrow(term, 'D', __VA_ARGS__)

ssize_t send_arrow(const Cluterm *term, char final, Uint16);
void handle_keydown(Cluterm *, const SDL_KeyboardEvent *);

#endif
