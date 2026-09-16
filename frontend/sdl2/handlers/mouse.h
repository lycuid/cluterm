#ifndef __SDL2__HANDLERS__MOUSE_H__
#define __SDL2__HANDLERS__MOUSE_H__

#include <SDL2/SDL.h>
#include <cluterm.h>

void mouse_button(const SDL_MouseButtonEvent *, const MouseReport *);
void mouse_wheel(const SDL_MouseWheelEvent *, const MouseReport *, ClutermMode);
void mouse_motion(const SDL_MouseMotionEvent *, const MouseReport *);

#endif
