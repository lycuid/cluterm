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

typedef uint16_t TrackingProtocol;
#define PROTO_BUTTON (1 << 0)
#define PROTO_DRAG   (1 << 1)
#define PROTO_ALL    (1 << 2)

typedef enum ProtoEncoding {
    ENC_LEGACY,
    ENC_UTF8     = 1005,
    ENC_SGR      = 1006,
    ENC_URXVT    = 1015,
    ENC_SGRPIXEL = 1016,
} ProtoEncoding;

typedef struct MouseTracking {
    TrackingProtocol proto;
    ProtoEncoding encoding;
} MouseTracking;

struct Cluterm {
    pty_t pty;
    VT_Parser vt_parser;
    ClutermBuffer buffer[2];
    cluterm_mode_t mode;
    MouseTracking mouse_tracking;
    OSC_Handler osc_handler;
    Theme theme;
    Config config;
};

#define ACTIVE_BUFFER(term)                                                    \
    (&(term)->buffer[IS_SET((term)->mode, MODE_ALT_BUFFER)])

void cluterm_init(Cluterm *);
void cluterm_start(Cluterm *, char *const *);
void cluterm_write(Cluterm *, uchar *, size_t);
void cluterm_resize(Cluterm *, int, int);
void cluterm_destroy(Cluterm *);

#endif
