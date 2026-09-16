#include "term_actions.h"
#include "main.h"
#include <SDL2/SDL.h>

#define RRGGBB(i)                                                              \
    ((i) >> 16) & 0xff, ((i) >> 16) & 0xff, ((i) >> 8) & 0xff,                 \
        ((i) >> 8) & 0xff, (i) & 0xff, (i) & 0xff

void set_window_title(const char *title)
{
    if (strlen(title))
        SDL_SetWindowTitle(gfx->window, title);
}

void query_palette_index(int index)
{
    const Cluterm *term = &gfx->term;
    char osc_color[64]  = {0};
    int len = sprintf(osc_color, "\x1b]4;%d;rgb:%02x%02x/%02x%02x/%02x%02x\x07",
                      index, RRGGBB(term->theme.palette[index]));
    gfx_write(osc_color, len);
}

void query_palette_fg(void)
{
    const Cluterm *term = &gfx->term;
    char osc_color[64]  = {0};
    int len = sprintf(osc_color, "\x1b]10;rgb:%02x%02x/%02x%02x/%02x%02x\x07",
                      RRGGBB(term->theme.fg));
    gfx_write(osc_color, len);
}

void query_palette_bg(void)
{
    const Cluterm *term = &gfx->term;
    char osc_color[64]  = {0};
    int len = sprintf(osc_color, "\x1b]11;rgb:%02x%02x/%02x%02x/%02x%02x\x07",
                      RRGGBB(term->theme.bg));
    gfx_write(osc_color, len);
}

void query_cursor_color(void)
{
    const Cluterm *term = &gfx->term;
    char osc_color[64]  = {0};
    int len = sprintf(osc_color, "\x1b]12;rgb:%02x%02x/%02x%02x/%02x%02x\x07",
                      RRGGBB(term->theme.cursor));
    gfx_write(osc_color, len);
}

void device_state_report(void) { gfx_write("\x1b[0n", 4); }

void report_cursor_position(void)
{
    const Cluterm *term    = &gfx->term;
    const ClutermBuffer *b = ACTIVE_BUFFER(term);
    char seq[32]           = {0};

    sprintf(seq, "\x1b[%d;%dR", b->cursor.y + 1, b->cursor.x + 1);
    gfx_write(seq, strlen(seq));
}

void send_device_attributes(void) { gfx_write("\x1b[?62c", 6); }
