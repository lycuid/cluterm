#ifndef __CLUTERM_H__
#define __CLUTERM_H__

#include <cluterm/pty.h>
#include <cluterm/vt/buffer.h>
#include <cluterm/vt/parser.h>

typedef uint16_t cluterm_mode_t;
#define MODE_ORIGIN          (1 << 0)
#define MODE_ALT_BUFFER      (1 << 1)
#define MODE_BRACKETED_PASTE (1 << 2)

typedef struct Cluterm {
    pty_t pty;
    VT_Parser vt_parser;
    ClutermBuffer buffer[2];
    cluterm_mode_t mode;
} Cluterm;

#define ACTIVE_BUFFER(term)                                                    \
    (&(term)->buffer[IS_SET((term)->mode, MODE_ALT_BUFFER)])

void cluterm_init(Cluterm *);
void cluterm_write(Cluterm *, char *, uint32_t);
void cluterm_resize(Cluterm *, int, int);
void cluterm_destroy(Cluterm *);

#endif
