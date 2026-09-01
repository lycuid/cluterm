#ifndef __SDL2__HANDLERS__MOUSE_H__
#define __SDL2__HANDLERS__MOUSE_H__

#include <SDL2/SDL.h>
#include <cluterm.h>

void mouse_button(const Cluterm *term, SDL_MouseButtonEvent *);
void mouse_wheel(const Cluterm *term, SDL_MouseWheelEvent *);
void mouse_motion(const Cluterm *term, SDL_MouseMotionEvent *);

#endif
