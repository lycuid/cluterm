#include "tty.h"
#include <config.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

#define ASSERT(expr, ...)                                                      \
    do {                                                                       \
        if ((expr))                                                            \
            err(1, __VA_ARGS__);                                               \
    } while (0)

#define TRY(expr, msg)                                                         \
    ASSERT((expr) == -1, "%s: (%s)\n.", msg, strerror(errno));

void tty_init(TTY *tty)
{
    TRY((tty->ptmx = posix_openpt(O_RDWR)), "ptmx open");
    TRY(grantpt(tty->ptmx), "pts chown");
    TRY(unlockpt(tty->ptmx), "pts unlock"); // ioctl: TIOCSPTLCK
    fcntl(tty->ptmx, F_SETFL, fcntl(tty->ptmx, F_GETFL) | O_NONBLOCK);
}

void tty_start(TTY *tty, const char *cmd)
{
    const char *pts_path = ptsname(tty->ptmx); // ioctl: TIOCGPTN
    puts(pts_path);

    TRY((tty->shell = fork()), "starting child process for shell");
    if (tty->shell) {
        tty_resize(tty, Rows, Columns);
        return;
    }

    int pts;
    TRY((pts = open(pts_path, O_RDWR, 0)), "pts open");

    puts("child executed!.");
    TRY(setsid(), "setsid()");
    TRY(ioctl(pts, TIOCSCTTY, 0), "ioctl: TIOCSCTTY failed.");
    ASSERT(dup2(pts, STDIN_FILENO) != STDIN_FILENO, "[stdin] dup: failed!.\n");
    ASSERT(dup2(pts, STDOUT_FILENO) != STDOUT_FILENO,
           "[stdout] dup: failed!.\n");
    ASSERT(dup2(pts, STDERR_FILENO) != STDERR_FILENO,
           "[stderr] dup: failed!.\n");
    close(pts);
    close(tty->ptmx);
    TRY(execl(cmd, cmd, NULL), "execl()");
}

void tty_resize(TTY *tty, int rows, int cols)
{
    const struct winsize size = {.ws_row = rows, .ws_col = cols};
    printf("resize: master(%d) slave(%d).\n",
           ioctl(tty->ptmx, TIOCSWINSZ, &size), kill(tty->shell, SIGWINCH));
    printf("tty resized to %dx%d.\n", cols, rows);
    fflush(stdout);
}

void tty_destroy(TTY *tty)
{
    close(tty->ptmx);
    kill(tty->shell, SIGHUP);
    waitpid(tty->shell, NULL, 0);
}
