#include "pty.h"
#include <cluterm/debug.h>
#include <config.h>
#include <err.h>
#include <errno.h>
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

void pty_open(pty_t *pty)
{
    TRY((pty->ptmx = posix_openpt(O_RDWR)), "ptmx open");
    TRY(grantpt(pty->ptmx), "pts chown");
    TRY(unlockpt(pty->ptmx), "pts unlock"); // ioctl: TIOCSPTLCK
    fcntl(pty->ptmx, F_SETFL, fcntl(pty->ptmx, F_GETFL) | O_NONBLOCK);
}

void pty_spawn(pty_t *pty, const char *cmd)
{
    const char *pts_path = ptsname(pty->ptmx); // ioctl: TIOCGPTN
    debug_1("pts_path: '%s'.\n", pts_path);

    TRY((pty->shell = fork()), "starting child process for shell");
    if (pty->shell) {
        pty_resize(pty, Rows, Columns);
        return;
    }

    int pts;
    TRY((pts = open(pts_path, O_RDWR, 0)), "pts open");

    debug_1("child executed!.\n");
    TRY(setsid(), "setsid()");
    TRY(ioctl(pts, TIOCSCTTY, 0), "ioctl: TIOCSCTTY failed.");
    ASSERT(dup2(pts, STDIN_FILENO) != STDIN_FILENO, "[stdin] dup: failed!.\n");
    ASSERT(dup2(pts, STDOUT_FILENO) != STDOUT_FILENO,
           "[stdout] dup: failed!.\n");
    ASSERT(dup2(pts, STDERR_FILENO) != STDERR_FILENO,
           "[stderr] dup: failed!.\n");
    close(pts);
    close(pty->ptmx);
    TRY(execl(cmd, cmd, NULL), "execl()");
}

void pty_resize(pty_t *pty, int rows, int cols)
{
    const struct winsize size = {.ws_row = rows, .ws_col = cols};
    debug_var int r_master    = ioctl(pty->ptmx, TIOCSWINSZ, &size);
    debug_var int r_slave     = kill(pty->shell, SIGWINCH);

    debug_1("resize: master(%d) slave(%d).\n", r_master, r_slave);
    debug_1("pty resized to %dx%d.\n", cols, rows);
}

void pty_destroy(pty_t *pty)
{
    close(pty->ptmx);
    kill(pty->shell, SIGHUP);
    waitpid(pty->shell, NULL, 0);
}
