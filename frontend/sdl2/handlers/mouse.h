#ifndef __SDL2__HANDLERS__MOUSE_H__
#define __SDL2__HANDLERS__MOUSE_H__

#include <SDL2/SDL.h>
#include <cluterm.h>

void mouse_button(const Cluterm *term, const SDL_MouseButtonEvent *);
void mouse_wheel(const Cluterm *term, const SDL_MouseWheelEvent *);
void mouse_motion(const Cluterm *term, const SDL_MouseMotionEvent *);

#endif
