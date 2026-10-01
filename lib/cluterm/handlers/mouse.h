#ifndef __CLUTERM__HANDLERS__MOUSE_H__
#define __CLUTERM__HANDLERS__MOUSE_H__

#include <cluterm/handlers/key.h>
#include <include/cluterm.h>
#include <stdbool.h>

typedef uint16_t ReportEvent;
#define EVENT_BUTTON (1 << 0)
#define EVENT_DRAG   (1 << 1)
#define EVENT_ALL    (1 << 2)

typedef struct MouseReport {
    ReportEvent event;
    enum {
        ENC_LEGACY,
        ENC_UTF8     = 1005,
        ENC_SGR      = 1006,
        ENC_URXVT    = 1015,
        ENC_SGRPIXEL = 1016,
    } encoding;
} MouseReport;

bool handle_mouse_button(Cluterm *, const ClutermMouse *);
bool handle_mouse_wheel(Cluterm *, const ClutermMouse *);
bool handle_mouse_motion(Cluterm *, const ClutermMouse *);

#endif
