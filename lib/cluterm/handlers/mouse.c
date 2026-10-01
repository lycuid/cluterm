#include <cluterm.h>
#include <cluterm/handlers/mouse.h>
#include <cluterm/utf8.h>
#include <stdio.h>

static inline int with_mods(int button, ClutermMod mod)
{
    if (IS_SET(mod, CLUTERM_MOD_SHIFT))
        SET(button, 4);
    if (IS_SET(mod, CLUTERM_MOD_ALT))
        SET(button, 8);
    if (IS_SET(mod, CLUTERM_MOD_CTRL))
        SET(button, 16);
    return button;
}

static inline void report_sgrpixel(Cluterm *term, int cb, int x, int y,
                                   bool pressed)
{
    char seq[32] = {0}, final = pressed ? 'M' : 'm';

    sprintf(seq, "\x1b[<%d;%d;%d%c", cb, x, y, final);
    cluterm_send(term, seq, strlen(seq));
}

static inline void report_x10(Cluterm *term, int cb, int cx, int cy)
{
    char seq[] = {'\x1b', '[', 'M', 32 + cb, 32 + cx, 32 + cy};
    cluterm_send(term, seq, 6);
}

static inline void report_utf8(Cluterm *term, int cb, int cx, int cy)
{
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

    cluterm_send(term, seq, strlen(seq));
}

static inline void report_sgr(Cluterm *term, int cb, int cx, int cy,
                              bool pressed)
{
    report_sgrpixel(term, cb, cx, cy, pressed);
}

static inline void report_urxvt(Cluterm *term, int cb, int cx, int cy)
{
    char seq[32] = {0};
    sprintf(seq, "\x1b[%d;%d;%dM", cb, cx, cy);
    cluterm_send(term, seq, strlen(seq));
}

static inline void report(Cluterm *term, int cb, const ClutermMouse *m)
{
    int cy = m->gridy + 1, cx = m->gridx + 1; // 1 based for cell grid.
    int y = m->y, x = m->x;

    switch (term->mouse_report.encoding) {
    case ENC_LEGACY: {
        if (!m->pressed)
            cb = (cb & ~3) | 3;
        report_x10(term, cb, cx, cy);
    } break;
    case ENC_UTF8: {
        if (!m->pressed)
            cb = (cb & ~3) | 3;
        report_utf8(term, cb, cx, cy);
    } break;
    case ENC_SGR: report_sgr(term, cb, cx, cy, m->pressed); break;
    case ENC_URXVT: {
        if (!m->pressed)
            cb = (cb & ~3) | 3;
        report_urxvt(term, cb, cx, cy);
    } break;
    case ENC_SGRPIXEL: report_sgrpixel(term, cb, x, y, m->pressed); break;
    }
}

bool mouse_button_report(Cluterm *term, const ClutermMouse *mouse)
{
    if (!IS_SET_ANY(term->mouse_report.event,
                    EVENT_BUTTON | EVENT_DRAG | EVENT_ALL))
        return false;

    int button = IS_SET(mouse->button, CLUTERM_MOUSE_LEFT)     ? 0
                 : IS_SET(mouse->button, CLUTERM_MOUSE_MIDDLE) ? 1
                 : IS_SET(mouse->button, CLUTERM_MOUSE_RIGHT)  ? 2
                                                               : -1;
    if (button == -1)
        return true;
    report(term, with_mods(button, mouse->mod), mouse);

    return true;
}

static inline bool mouse_wheel_report(Cluterm *term, const ClutermMouse *mouse)
{
    if (!IS_SET_ANY(term->mouse_report.event,
                    EVENT_BUTTON | EVENT_DRAG | EVENT_ALL))
        return false;
    int dy = mouse->wheely, dx = mouse->wheelx, mods = with_mods(0, mouse->mod);
    for (; dy > 0; --dy)
        report(term, 64 | mods, mouse);
    for (; dy < 0; ++dy)
        report(term, 65 | mods, mouse);
    for (; dx < 0; ++dx)
        report(term, 66 | mods, mouse);
    for (; dx > 0; --dx)
        report(term, 67 | mods, mouse);
    return true;
}

static inline bool mouse_wheel_scroll_mode(Cluterm *term,
                                           const ClutermMouse *mouse)
{
    if (!IS_SET(term->mode, MODE_ALT_BUFFER | MODE_ALT_SCROLL))
        return false;
    int dy = mouse->wheely, dx = mouse->wheelx;
    for (; dy > 0; --dy)
        send_arrow_up(term, 0);
    for (; dy < 0; ++dy)
        send_arrow_down(term, 0);
    for (; dx > 0; --dx)
        send_arrow_right(term, 0);
    for (; dx < 0; ++dx)
        send_arrow_left(term, 0);
    return true;
}

static inline bool mouse_wheel_scroll(Cluterm *term, const ClutermMouse *mouse)
{
    if (IS_SET_ANY(mouse->mod,
                   CLUTERM_MOD_ALT | CLUTERM_MOD_CTRL | CLUTERM_MOD_SHIFT))
        return false;
    ClutermBuffer *b  = active_buffer(term);
    int histsize      = CLAMP(b->last_row - b->rows, 0, b->history);
    b->scroll_offset  = CLAMP(b->scroll_offset + mouse->wheely, 0, histsize);
    term->render_flag = 1;
    dirty_buffer(b);
    return true;
}

bool handle_mouse_wheel(Cluterm *term, const ClutermMouse *mouse)
{
    bool handled = mouse_wheel_report(term, mouse);
    if (!handled)
        handled = mouse_wheel_scroll_mode(term, mouse);
    if (!handled)
        handled = mouse_wheel_scroll(term, mouse);
    return handled;
}

bool mouse_motion_report(Cluterm *term, const ClutermMouse *mouse)
{
    if (!IS_SET_ANY(term->mouse_report.event, EVENT_DRAG | EVENT_ALL))
        return false;

    if (!mouse->pressed)
        return false;

    int button = 0;
    if (IS_SET(mouse->button, CLUTERM_MOUSE_LEFT))
        button = 32;
    else if (IS_SET(mouse->button, CLUTERM_MOUSE_MIDDLE))
        button = 33;
    else if (IS_SET(mouse->button, CLUTERM_MOUSE_RIGHT))
        button = 34;

    if (button == 0) {
        if (!IS_SET(term->mouse_report.event, EVENT_ALL))
            return true;
        SET(button, 3);
    }

    report(term, with_mods(button, mouse->mod), mouse);

    return true;
}
