#ifndef __VTE__TTY_H__
#define __VTE__TTY_H__

#include <sys/types.h>

typedef struct TTY {
    pid_t shell;
    int ptmx;
} TTY;

#define tty_read(tty, ...)  read((tty)->ptmx, __VA_ARGS__)
#define tty_write(tty, ...) write((tty)->ptmx, __VA_ARGS__)

void tty_init(TTY *);
void tty_start(TTY *, const char *);
void tty_resize(TTY *, int, int);
void tty_destroy(TTY *);

#endif
