#ifndef __CLUTERM_H__
#define __CLUTERM_H__

#include <cluterm/pty.h>
#include <cluterm/vt/buffer.h>
#include <cluterm/vt/parser.h>

#define cluterm_set_osc_handler(term, handler) (term)->osc_handler = handler;

typedef uint16_t cluterm_mode_t;
#define MODE_ORIGIN          (1 << 0)
#define MODE_ALT_BUFFER      (1 << 1)
#define MODE_BRACKETED_PASTE (1 << 2)
#define MODE_MOUSE_TRACKING  (1 << 3)

typedef struct Cluterm Cluterm;

typedef void (*OSC_Handler)(Cluterm *term, OSC_Payload *osc);

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

typedef struct ClutermActions {
    void (*set_window_title)(const Cluterm *, const char *); // OSC 1,2
    void (*query_palette_index)(const Cluterm *, int);       // OSC 4
    void (*query_palette_fg)(const Cluterm *);               // OSC 10
    void (*query_palette_bg)(const Cluterm *);               // OSC 11
    void (*query_cursor_color)(const Cluterm *);             // OSC 12
    void (*device_state_report)(const Cluterm *);            // DSR 5
    void (*report_cursor_position)(const Cluterm *);         // DSR 6
    void (*send_device_attributes)(const Cluterm *);         // Primary DA (DA1)
} ClutermActions;

struct Cluterm {
    pty_t pty;
    VT_Parser vt_parser;
    ClutermBuffer buffer[2];
    cluterm_mode_t mode;

    Theme theme;
    Config config;

    MouseReport mouse_report;
    ClutermActions actions;
};

#define ACTIVE_BUFFER(term)                                                    \
    (&(term)->buffer[IS_SET((term)->mode, MODE_ALT_BUFFER)])

void cluterm_init(Cluterm *);
void cluterm_start(Cluterm *, char *const *);
void cluterm_write(Cluterm *, uchar *, size_t);
void cluterm_resize(Cluterm *, int, int);
void cluterm_destroy(Cluterm *);

#endif
