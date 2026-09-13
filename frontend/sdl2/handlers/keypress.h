#ifndef __SDL2__HANDLERS__KEYPRESS_H__
#define __SDL2__HANDLERS__KEYPRESS_H__

#include <SDL2/SDL.h>
#include <cluterm.h>

#define send_arrow_up(mods)    send_arrow('A', mods)
#define send_arrow_down(mods)  send_arrow('B', mods)
#define send_arrow_right(mods) send_arrow('C', mods)
#define send_arrow_left(mods)  send_arrow('D', mods)

ssize_t send_arrow(char final, Uint16);
void handle_keydown(const SDL_KeyboardEvent *);

#endif
