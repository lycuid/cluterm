#ifndef __TERMINAL_H__
#define __TERMINAL_H__

#include <cluterm/terminal/buffer.h>
#include <cluterm/vte/tty.h>

typedef uint16_t TerminalMode;
#define MODE_ALT_BUFFER (1 << 0)
#define MODE_ORIGIN     (1 << 1)

typedef struct Terminal {
    TTY tty;
    VT_Parser vt_parser;
    TerminalBuffer buffer[2];
    Cursor saved_cursor[2];
    bool *tab;
    TerminalMode mode;
} Terminal;

#define ACTIVE_BUFFER(term)                                                    \
    (&(term)->buffer[IS_SET((term)->mode, MODE_ALT_BUFFER)])

void term_init(Terminal *);
void term_start(Terminal *);
void term_destroy(Terminal *);

#endif
