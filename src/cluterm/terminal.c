#include "terminal.h"
#include <SDL.h>
#include <cluterm/gfx.h>
#include <cluterm/terminal/actions.h>
#include <cluterm/terminal/actions/csi.h>
#include <cluterm/terminal/actions/ctrl.h>
#include <cluterm/terminal/actions/esc.h>
#include <cluterm/terminal/actions/osc.h>
#include <cluterm/vte/tty.h>
#include <config.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int running = 1;

void quit(__attribute__((unused)) int _arg) { running = 0; }

void term_init(Terminal *term)
{
    gfx->init();
    {
        buffer_init(&term->buffer[0], Rows, Columns, 0); // primary.
        buffer_init(&term->buffer[1], Rows, Columns, 0); // alt.
        term->saved_cursor[0] = term->buffer[0].cursor;
        term->saved_cursor[1] = term->buffer[1].cursor;
        term->tab             = (bool *)calloc(Columns + 1, sizeof(bool));
        for (int i = TabWidth; i <= Columns; i += TabWidth)
            term->tab[i] = true;
    }
    parser_init(&term->vt_parser);
    {
        tty_init(&term->tty);
        setenv("TERM", "st-256color", 1);
        char *shell = getenv("SHELL");
        if (!shell)
            shell = "/bin/bash";
        signal(SIGCHLD, quit);
        tty_start(&term->tty, shell);
    }
}

static inline void term_create_page(Terminal *term)
{
    TerminalBuffer *b = ACTIVE_BUFFER(term);
    Cursor *c         = &b->cursor;
    for (int y = 0; y < b->rows; ++y) {
        for (int x = b->cols - 1; x >= 0; --x) {
            Cell cell = b->lines[tline(b, y)][x];
            if (y == c->y && x == c->x && (c->state & CursorHide) == 0) {
                Rgb tmp       = cell.attrs.fg;
                cell.attrs.fg = cell.attrs.bg;
                cell.attrs.bg = tmp;
            }
            if (cell.value == ' ' && cell.attrs.fg == DefaultFG &&
                cell.attrs.bg == DefaultBG)
                continue;
            gfx->draw_cell(cell, y, x);
        }
    }
}

// clang-format off
static inline void term_write(Terminal *term, char *stream, uint32_t slen)
{
    VT_Parser *vt_parser = &term->vt_parser;
    TerminalBuffer *b    = ACTIVE_BUFFER(term);
    parser_feed(vt_parser, stream, slen);
    for (FSM_Event fsm_event;;) {
        switch (fsm_event = parser_run(vt_parser)) {
        case EVENT_NOOP: goto DONE;
        case EVENT_PRINT: {
            term_putc(term, (Cell)CELL(vt_parser->payload.value, b->cell_attrs));
        } break;
        case EVENT_CTRL: action_ctrl(term, &vt_parser->payload.ctrl);   break;
        case EVENT_ESC:  action_esc(term, &vt_parser->payload.esc);     break;
        case EVENT_CSI:  action_csi(term, &vt_parser->payload.csi);     break;
        case EVENT_OSC:  action_osc(term, &vt_parser->payload.osc);     break;
        }
    }
DONE:
    return;
} // clang-format on

#define STREAM_SIZE 4096
void term_start(Terminal *term)
{
    SDL_Event e;
    ssize_t n    = 0;
    char repr[2] = {0, 0}, stream[STREAM_SIZE] = {0};

    for (gfx->clear(), gfx->display(); running;) {
        if ((n = tty_read(&term->tty, stream, STREAM_SIZE)) > 0) {
            term_write(term, stream, n);
            gfx->clear(), term_create_page(term), gfx->display();
        }
        while (SDL_PollEvent(&e)) {
            switch (e.type) { // clang-format off
            case SDL_QUIT: { running = 0; } break;
            case SDL_WINDOWEVENT: {
                SDL_WindowEvent *win = &e.window;
                switch (win->event) {
                case SDL_WINDOWEVENT_EXPOSED: {
                    gfx->clear(), term_create_page(term), gfx->display();
                } break;
                case SDL_WINDOWEVENT_SIZE_CHANGED: {
                    int cols          = win->data1 / gfx->f_width,
                        rows          = win->data2 / gfx->f_height;
                    TerminalBuffer *b = ACTIVE_BUFFER(term);
                    if (b->rows != rows || b->cols != cols) {
                        buffer_resize(&term->buffer[0], rows, cols);
                        buffer_resize(&term->buffer[1], rows, cols);
                        tty_resize(&term->tty, rows, cols);
                        gfx->clear(), term_create_page(term), gfx->display();
                    }
                } break;
                }
            } break;
            case SDL_KEYDOWN: {
                SDL_KeyboardEvent *key = &e.key;
                repr[0] = key->keysym.sym, repr[1] = 0, n = 1;
                switch (key->keysym.sym) {
                case SDLK_a: // fallthrough
                case SDLK_b: // fallthrough
                case SDLK_c: // fallthrough
                case SDLK_d: // fallthrough
                case SDLK_e: // fallthrough
                case SDLK_f: // fallthrough
                case SDLK_g: // fallthrough
                case SDLK_h: // fallthrough
                case SDLK_i: // fallthrough
                case SDLK_j: // fallthrough
                case SDLK_k: // fallthrough
                case SDLK_l: // fallthrough
                case SDLK_m: // fallthrough
                case SDLK_n: // fallthrough
                case SDLK_o: // fallthrough
                case SDLK_p: // fallthrough
                case SDLK_q: // fallthrough
                case SDLK_r: // fallthrough
                case SDLK_s: // fallthrough
                case SDLK_t: // fallthrough
                case SDLK_u: // fallthrough
                case SDLK_v: // fallthrough
                case SDLK_w: // fallthrough
                case SDLK_x: // fallthrough
                case SDLK_y: // fallthrough
                case SDLK_z: {
                    if (key->keysym.mod & KMOD_CTRL)
                        repr[0] -= 96;
                    else if (key->keysym.mod & KMOD_SHIFT)
                        repr[0] -= 32;
                } break;
                case SDLK_0: { if (key->keysym.mod & KMOD_SHIFT) repr[0] = ')'; } break;
                case SDLK_1: { if (key->keysym.mod & KMOD_SHIFT) repr[0] = '!'; } break;
                case SDLK_2: { if (key->keysym.mod & KMOD_SHIFT) repr[0] = '@'; } break;
                case SDLK_3: { if (key->keysym.mod & KMOD_SHIFT) repr[0] = '#'; } break;
                case SDLK_4: { if (key->keysym.mod & KMOD_SHIFT) repr[0] = '$'; } break;
                case SDLK_5: { if (key->keysym.mod & KMOD_SHIFT) repr[0] = '%'; } break;
                case SDLK_6: { if (key->keysym.mod & KMOD_SHIFT) repr[0] = '^'; } break;
                case SDLK_7: { if (key->keysym.mod & KMOD_SHIFT) repr[0] = '&'; } break;
                case SDLK_8: { if (key->keysym.mod & KMOD_SHIFT) repr[0] = '*'; } break;
                case SDLK_9: { if (key->keysym.mod & KMOD_SHIFT) repr[0] = '('; } break;
                case SDLK_MINUS:     { if (key->keysym.mod & KMOD_SHIFT) repr[0] = '_'; } break;
                case SDLK_EQUALS:    { if (key->keysym.mod & KMOD_SHIFT) repr[0] = '+'; } break;
                case SDLK_SEMICOLON: { if (key->keysym.mod & KMOD_SHIFT) repr[0] = ':'; } break;
                case SDLK_QUOTE:     { if (key->keysym.mod & KMOD_SHIFT) repr[0] = '"'; } break;
                case SDLK_COMMA:     { if (key->keysym.mod & KMOD_SHIFT) repr[0] = '<'; } break;
                case SDLK_PERIOD:    { if (key->keysym.mod & KMOD_SHIFT) repr[0] = '>'; } break;
                case SDLK_BACKSLASH: { if (key->keysym.mod & KMOD_SHIFT) repr[0] = '|'; } break;
                case SDLK_BACKQUOTE: { if (key->keysym.mod & KMOD_SHIFT) repr[0] = '~'; } break;
                case SDLK_LEFTBRACKET: /* fallthrough. */
                case SDLK_RIGHTBRACKET: {
                    if (key->keysym.mod & KMOD_SHIFT)
                        repr[0] += 32;
                } break;
                case SDLK_UP:    { tty_write(&term->tty, "\x1b[A", 3); } continue;
                case SDLK_DOWN:  { tty_write(&term->tty, "\x1b[B", 3); } continue;
                case SDLK_RIGHT: { tty_write(&term->tty, "\x1b[C", 3); } continue;
                case SDLK_LEFT:  { tty_write(&term->tty, "\x1b[D", 3); } continue;
                default: break;
                }
                if (BETWEEN(repr[0], 1, 127))
                    tty_write(&term->tty, repr, n);
            }
            default: break;
            } // clang-format on
        }
        SDL_Delay(1);
    }
}

void term_destroy(Terminal *term)
{
    free(term->tab);
    tty_destroy(&term->tty);
    buffer_destroy(&term->buffer[0]);
    buffer_destroy(&term->buffer[1]);
    gfx->destroy();
}
