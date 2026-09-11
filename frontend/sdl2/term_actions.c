#include "term_actions.h"
#include "main.h"
#include <SDL2/SDL.h>

#define RRGGBB(i)                                                              \
    ((i) >> 16) & 0xff, ((i) >> 16) & 0xff, ((i) >> 8) & 0xff,                 \
        ((i) >> 8) & 0xff, (i) & 0xff, (i) & 0xff

void set_window_title(const Cluterm *term, const char *title)
{
    if (strlen(title))
        SDL_SetWindowTitle(gfx->window, title);
    (void)term;
}

void query_palette_index(const Cluterm *term, int index)
{
    char osc_color[64] = {0};
    int len = sprintf(osc_color, "\x1b]4;%d;rgb:%02x%02x/%02x%02x/%02x%02x\x07",
                      index, RRGGBB(term->theme.palette[index]));
    pty_write(&term->pty, osc_color, len);
}

void query_palette_fg(const Cluterm *term)
{
    char osc_color[64] = {0};
    int len = sprintf(osc_color, "\x1b]10;rgb:%02x%02x/%02x%02x/%02x%02x\x07",
                      RRGGBB(term->theme.fg));
    pty_write(&term->pty, osc_color, len);
}

void query_palette_bg(const Cluterm *term)
{
    char osc_color[64] = {0};
    int len = sprintf(osc_color, "\x1b]11;rgb:%02x%02x/%02x%02x/%02x%02x\x07",
                      RRGGBB(term->theme.bg));
    pty_write(&term->pty, osc_color, len);
}

void query_cursor_color(const Cluterm *term)
{
    char osc_color[64]     = {0};
    const ClutermBuffer *b = ACTIVE_BUFFER(term);
    int len = sprintf(osc_color, "\x1b]12;rgb:%02x%02x/%02x%02x/%02x%02x\x07",
                      RRGGBB(b->cursor.color));
    pty_write(&term->pty, osc_color, len);
}

void device_state_report(const Cluterm *term)
{
    pty_write(&term->pty, "\x1b[0n", 4);
}

void report_cursor_position(const Cluterm *term)
{
    char seq[32]           = {0};
    const ClutermBuffer *b = ACTIVE_BUFFER(term);

    sprintf(seq, "\x1b[%d;%dR", b->cursor.y + 1, b->cursor.x + 1);
    pty_write(&term->pty, seq, strlen(seq));
}

void send_device_attributes(const Cluterm *term)
{
    pty_write(&term->pty, "\x1b[?62c", 6);
}
