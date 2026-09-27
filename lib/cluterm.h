#ifndef __CLUTERM_H__
#define __CLUTERM_H__

#include <cluterm/handlers/key.h>
#include <cluterm/handlers/mouse.h>
#include <cluterm/vt/buffer.h>
#include <cluterm/vt/parser.h>

typedef uint16_t ClutermMode;
#define MODE_ORIGIN          (1 << 0)
#define MODE_APP_CURSOR_KEYS (1 << 1)
#define MODE_ALT_BUFFER      (1 << 2)
#define MODE_ALT_SCROLL      (1 << 3)
#define MODE_BRACKETED_PASTE (1 << 4)
#define MODE_MOUSE_TRACKING  (1 << 5)

struct Cluterm {
    Config config;

    VT_Parser vt_parser;
    ClutermBuffer buffer[2];
    ClutermMode mode;
    ClutermActions actions;

    Theme theme;
    MouseReport mouse_report;
};

#define active_buffer(term)                                                    \
    (&(term)->buffer[IS_SET((term)->mode, MODE_ALT_BUFFER)])

#endif
