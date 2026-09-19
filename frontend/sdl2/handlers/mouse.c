#include "mouse.h"
#include "keypress.h"
#include "main.h"
#include <cluterm/utf8.h>

static inline int with_mods(int button)
{
    SDL_Keymod mod = SDL_GetModState();
    if (IS_SET_ANY(mod, KMOD_SHIFT))
        SET(button, 4);
    if (IS_SET_ANY(mod, KMOD_ALT))
        SET(button, 8);
    if (IS_SET_ANY(mod, KMOD_CTRL))
        SET(button, 16);
    return button;
}

static inline void report_sgrpixel(int cb, int x, int y, bool pressed)
{
    char seq[32] = {0}, final = pressed ? 'M' : 'm';

    sprintf(seq, "\x1b[<%d;%d;%d%c", cb, x, y, final);
    gfx_write(seq, strlen(seq));
}

static inline void report_x10(int cb, int x, int y)
{
    int cy = y / gfx->f_height + 1, cx = x / gfx->f_width + 1;
    char seq[] = {'\x1b', '[', 'M', 32 + cb, 32 + cx, 32 + cy};
    gfx_write(seq, 6);
}

static inline void report_utf8(int cb, int x, int y)
{
    int cy = y / gfx->f_height + 1, cx = x / gfx->f_width + 1;
    if (cy > 2015 || cx > 2015)
        return;

    UTF8_String ucb = {0}, ux = {0}, uy = {0};
    utf8_encode(32 + cb, ucb);
    utf8_encode(32 + cx, ux);
    utf8_encode(32 + cy, uy);

    char seq[32] = "\x1b[M";
    strcat(seq, ucb);
    strcat(seq, ux);
    strcat(seq, uy);

    gfx_write(seq, strlen(seq));
}

static inline void report_sgr(int cb, int x, int y, bool pressed)
{
    int cy = y / gfx->f_height + 1, cx = x / gfx->f_width + 1;
    report_sgrpixel(cb, cx, cy, pressed);
}

static inline void report_urxvt(int cb, int x, int y)
{
    int cy = y / gfx->f_height + 1, cx = x / gfx->f_width + 1;
    char seq[32] = {0};
    sprintf(seq, "\x1b[%d;%d;%dM", cb, cx, cy);
    gfx_write(seq, strlen(seq));
}

static inline void report(const MouseReport *mouse_report, int cb, int x, int y,
                          bool pressed)
{
    switch (mouse_report->encoding) {
    case ENC_LEGACY: {
        if (!pressed)
            cb = (cb & ~3) | 3;
        report_x10(cb, x, y);
    } break;
    case ENC_UTF8: {
        if (!pressed)
            cb = (cb & ~3) | 3;
        report_utf8(cb, x, y);
    } break;
    case ENC_SGR: report_sgr(cb, x, y, pressed); break;
    case ENC_URXVT: {
        if (!pressed)
            cb = (cb & ~3) | 3;
        report_urxvt(cb, x, y);
    } break;
    case ENC_SGRPIXEL: report_sgrpixel(cb, x, y, pressed); break;
    }
}

static inline void report_mouse_button(const SDL_MouseButtonEvent *mouse,
                                       const MouseReport *mreport)
{
    int button = mouse->button == SDL_BUTTON_LEFT     ? 0
                 : mouse->button == SDL_BUTTON_MIDDLE ? 1
                 : mouse->button == SDL_BUTTON_RIGHT  ? 2
                                                      : -1;
    if (button == -1)
        return;

    report(mreport, with_mods(button), mouse->x, mouse->y,
           mouse->state == SDL_PRESSED);
}

static inline void selection_mouse_button(const SDL_MouseButtonEvent *mouse)
{
    if (mouse->state == SDL_PRESSED) {
        int y = mouse->y / gfx->f_height, x = mouse->x / gfx->f_width;

        switch (mouse->clicks) {
        case 1: selection_start(y, x); break;
        case 2: selection_word(y, x); break;
        case 3: selection_line(y); break;
        }
    }
}

static inline void report_mouse_wheel(const SDL_MouseWheelEvent *wheel,
                                      const MouseReport *mreport)
{
    int dy = wheel->y, dx = wheel->x, mods = with_mods(0);
    for (; dy > 0; --dy)
        report(mreport, 64 | mods, wheel->mouseX, wheel->mouseY, 1);
    for (; dy < 0; ++dy)
        report(mreport, 65 | mods, wheel->mouseX, wheel->mouseY, 1);
    for (; dx < 0; ++dx)
        report(mreport, 66 | mods, wheel->mouseX, wheel->mouseY, 1);
    for (; dx > 0; --dx)
        report(mreport, 67 | mods, wheel->mouseX, wheel->mouseY, 1);
}

static inline void selection_mouse_wheel(const SDL_MouseWheelEvent *wheel)
{
    if (IS_SET(gfx->frame.term_snapshot.term_mode,
               MODE_ALT_BUFFER | MODE_ALT_SCROLL)) {
        int dy = wheel->y, dx = wheel->x;
        for (; dy > 0; --dy)
            send_arrow_up(0);
        for (; dy < 0; ++dy)
            send_arrow_down(0);
        for (; dx > 0; --dx)
            send_arrow_right(0);
        for (; dx < 0; ++dx)
            send_arrow_left(0);
    }
}

static inline void report_mouse_motion(const SDL_MouseMotionEvent *motion,
                                       const MouseReport *mreport)
{
    if (motion->state != SDL_PRESSED)
        return;

    int button = 0;
    if (IS_SET_ANY(motion->state, SDL_BUTTON_LMASK))
        button = 32;
    else if (IS_SET_ANY(motion->state, SDL_BUTTON_MMASK))
        button = 33;
    else if (IS_SET_ANY(motion->state, SDL_BUTTON_RMASK))
        button = 34;

    if (button == 0) {
        if (!IS_SET(mreport->event, EVENT_ALL))
            return;
        SET(button, 3);
    }

    report(mreport, with_mods(button), motion->x, motion->y, 0);
}

static inline void selection_mouse_motion(const SDL_MouseMotionEvent *motion)
{
    if (motion->state == SDL_PRESSED) {
        int w, h, y = -1, x = -1;
        SDL_GetWindowSize(gfx->window, &w, &h);
        if (BETWEEN(motion->y, 0, h))
            y = motion->y / gfx->f_height;
        if (BETWEEN(motion->x, 0, w))
            x = motion->x / gfx->f_width;
        selection_extend(y, x);
    }
}

void mouse_button(const SDL_MouseButtonEvent *mouse, const MouseReport *mreport)
{
    if (IS_SET_ANY(mreport->event, EVENT_BUTTON | EVENT_DRAG | EVENT_ALL))
        report_mouse_button(mouse, mreport);
    else
        selection_mouse_button(mouse);
}

void mouse_wheel(const SDL_MouseWheelEvent *wheel, const MouseReport *mreport)
{
    if (IS_SET_ANY(mreport->event, EVENT_BUTTON | EVENT_DRAG | EVENT_ALL))
        report_mouse_wheel(wheel, mreport);
    else
        selection_mouse_wheel(wheel);
}

void mouse_motion(const SDL_MouseMotionEvent *motion,
                  const MouseReport *mreport)
{
    if (IS_SET_ANY(mreport->event, EVENT_DRAG | EVENT_ALL))
        report_mouse_motion(motion, mreport);
    else
        selection_mouse_motion(motion);
}
