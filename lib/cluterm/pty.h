#ifndef __CLUTERM__PTY_H__
#define __CLUTERM__PTY_H__

#include <sys/types.h>
#include <unistd.h>

typedef struct pty_t {
    pid_t shell;
    int ptmx;
} pty_t;

#define pty_read(pty, ...)  read((pty)->ptmx, __VA_ARGS__)
#define pty_write(pty, ...) write((pty)->ptmx, __VA_ARGS__)

void pty_open(pty_t *);
void pty_spawn(pty_t *, const char *);
void pty_resize(pty_t *, int, int);
void pty_destroy(pty_t *);

#endif
