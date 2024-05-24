#ifndef __CLUTERM_H__
#define __CLUTERM_H__

#include <cluterm/buffer.h>
#include <cluterm/vte/tty.h>

typedef uint16_t CluTermMode;
#define MODE_ALT_BUFFER (1 << 0)
#define MODE_ORIGIN     (1 << 1)

typedef struct CluTerm {
    TTY tty;
    VT_Parser vt_parser;
    CluTermBuffer buffer[2];
    Cursor saved_cursor[2];
    bool *tab;
    CluTermMode mode;
} CluTerm;

#define ACTIVE_BUFFER(term)                                                    \
    (&(term)->buffer[IS_SET((term)->mode, MODE_ALT_BUFFER)])

void cluterm_init(CluTerm *);
void cluterm_write(CluTerm *, char *, uint32_t);
void cluterm_resize(CluTerm *, int, int);
void cluterm_destroy(CluTerm *);

#endif
