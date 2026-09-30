#include "mouse.h"
#include "main.h"

void mouse_button_selection(const SDL_MouseButtonEvent *mouse)
{
    if (mouse->state == SDL_PRESSED) {
        int y = (mouse->y - gfx->config->padding.top) / gfx->f_height,
            x = (mouse->x - gfx->config->padding.left) / gfx->f_width;

        switch (mouse->clicks) {
        case 1: selection_start(y, x); break;
        case 2: selection_word(y, x); break;
        case 3: selection_line(y); break;
        }
    }
}

void mouse_wheel_selection(const SDL_MouseWheelEvent *wheel)
{
    if (wheel->y != 0)
        gfx_scroll(wheel->y);
}

void mouse_motion_selection(const SDL_MouseMotionEvent *motion)
{
    if (motion->state == SDL_PRESSED) {
        int w, h, y = -1, x = -1;
        SDL_GetWindowSize(gfx->window, &w, &h);
        if (BETWEEN(motion->y, 0, h))
            y = (motion->y - gfx->config->padding.top) / gfx->f_height;
        if (BETWEEN(motion->x, 0, w))
            x = (motion->x - gfx->config->padding.left) / gfx->f_width;
        selection_extend(y, x);
    }
}
