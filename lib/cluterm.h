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

typedef struct Cluterm Cluterm;

typedef void (*OSC_Handler)(Cluterm *term, OSC_Payload *osc);

struct Cluterm {
    pty_t pty;
    VT_Parser vt_parser;
    ClutermBuffer buffer[2];
    cluterm_mode_t mode;
    OSC_Handler osc_handler;

    MEMBER_COLORS;
};

#define ACTIVE_BUFFER(term)                                                    \
    (&(term)->buffer[IS_SET((term)->mode, MODE_ALT_BUFFER)])

void cluterm_init(Cluterm *, char *const *);
void cluterm_write(Cluterm *, uchar *, uint32_t);
void cluterm_resize(Cluterm *, int, int);
void cluterm_destroy(Cluterm *);

#endif
