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
#include <unistd.h>

static int running = 1;

void quit(int _arg)
{
    (void)_arg;
    running = 0;
}

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

static inline void term_write(Terminal *term, char *stream, uint32_t slen)
{
    VT_Parser *vt_parser = &term->vt_parser;
    TerminalBuffer *b    = ACTIVE_BUFFER(term);
    parser_feed(vt_parser, stream, slen);
    for (FSM_Event fsm_event;;) {
        switch (fsm_event = parser_run(vt_parser)) {
        case EVENT_NOOP: {
            goto DONE;
        }
        case EVENT_PRINT: {
            put_cell(term, CELL(vt_parser->payload.value, b->cell_attrs));
        } break;
        case EVENT_CTRL: {
            ctrl_perform_action(term, &vt_parser->payload.ctrl);
        } break;
        case EVENT_ESC: {
            esc_perform_action(term, &vt_parser->payload.esc);
        } break;
        case EVENT_CSI: {
            csi_perform_action(term, &vt_parser->payload.csi);
        } break;
        case EVENT_OSC: {
            osc_perform_action(term, &vt_parser->payload.osc);
        } break;
        }
    }
DONE:
    return;
}

static inline void handle_keydown(Terminal *term, SDL_Event *e)
{
    SDL_KeyboardEvent *key = &e->key;

    char repr[2] = {key->keysym.sym, 0};
    ssize_t n    = 1;

    switch (key->keysym.sym) { // clang-format off
#define SHIFT(key) ((key)->keysym.mod & KMOD_SHIFT)
#define CTRL(key)  ((key)->keysym.mod & KMOD_CTRL)

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
    case SDLK_z: { repr[0] -= CTRL(key) ? 96 : SHIFT(key) ? 32 : 0; } break;

    case SDLK_0: { repr[0] = SHIFT(key) ? ')' : repr[0]; } break;
    case SDLK_1: { repr[0] = SHIFT(key) ? '!' : repr[0]; } break;
    case SDLK_2: { repr[0] = SHIFT(key) ? '@' : repr[0]; } break;
    case SDLK_3: { repr[0] = SHIFT(key) ? '#' : repr[0]; } break;
    case SDLK_4: { repr[0] = SHIFT(key) ? '$' : repr[0]; } break;
    case SDLK_5: { repr[0] = SHIFT(key) ? '%' : repr[0]; } break;
    case SDLK_6: { repr[0] = SHIFT(key) ? '^' : repr[0]; } break;
    case SDLK_7: { repr[0] = SHIFT(key) ? '&' : repr[0]; } break;
    case SDLK_8: { repr[0] = SHIFT(key) ? '*' : repr[0]; } break;
    case SDLK_9: { repr[0] = SHIFT(key) ? '(' : repr[0]; } break;

    case SDLK_MINUS:      { repr[0] = SHIFT(key) ? '_' : repr[0]; } break;
    case SDLK_EQUALS:     { repr[0] = SHIFT(key) ? '+' : repr[0]; } break;
    case SDLK_SEMICOLON:  { repr[0] = SHIFT(key) ? ':' : repr[0]; } break;
    case SDLK_QUOTE:      { repr[0] = SHIFT(key) ? '"' : repr[0]; } break;
    case SDLK_COMMA:      { repr[0] = SHIFT(key) ? '<' : repr[0]; } break;
    case SDLK_PERIOD:     { repr[0] = SHIFT(key) ? '>' : repr[0]; } break;
    case SDLK_BACKSLASH:  { repr[0] = SHIFT(key) ? '|' : repr[0]; } break;
    case SDLK_BACKQUOTE:  { repr[0] = SHIFT(key) ? '~' : repr[0]; } break;

    case SDLK_LEFTBRACKET: /* fallthrough. */
    case SDLK_RIGHTBRACKET: { repr[0] += SHIFT(key) ? 32 : 0; } break;
#undef CTRL
#undef SHIFT

    case SDLK_UP:     { tty_write(&term->tty, "\x1b[A", 3); } break;
    case SDLK_DOWN:   { tty_write(&term->tty, "\x1b[B", 3); } break;
    case SDLK_RIGHT:  { tty_write(&term->tty, "\x1b[C", 3); } break;
    case SDLK_LEFT:   { tty_write(&term->tty, "\x1b[D", 3); } break;
    default: break;
    }
    if (BETWEEN(repr[0], 1, 127))
        tty_write(&term->tty, repr, n);
} // clang-format on

static inline void resize_terminal_window(Terminal *term, int width, int height)
{
    int cols = width / gfx->f_width, rows = height / gfx->f_height;
    TerminalBuffer *b = ACTIVE_BUFFER(term);
    if (b->rows != rows || b->cols != cols) {
        buffer_resize(&term->buffer[0], rows, cols);
        buffer_resize(&term->buffer[1], rows, cols);
        tty_resize(&term->tty, rows, cols);
        gfx->clear(), term_create_page(term), gfx->display();
    }
}

void term_start(Terminal *term)
{
    SDL_Event e;
    ssize_t n = 0;

#define STREAM_SIZE 4096
    char stream[STREAM_SIZE] = {0};

    for (gfx->clear(), gfx->display(); running;) {
        if ((n = tty_read(&term->tty, stream, STREAM_SIZE)) > 0) {
            term_write(term, stream, n);
            gfx->clear();
            term_create_page(term);
            gfx->display();
        }
#undef STREAM_SIZE

        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT: running = 0; break;

            case SDL_WINDOWEVENT: {
                SDL_WindowEvent *win = &e.window;
                switch (win->event) {
                case SDL_WINDOWEVENT_EXPOSED: {
                    gfx->clear();
                    term_create_page(term);
                    gfx->display();
                } break;

                case SDL_WINDOWEVENT_SIZE_CHANGED:
                    resize_terminal_window(term, win->data1, win->data2);
                    break;
                }
            } break;

            case SDL_KEYDOWN: handle_keydown(term, &e); break;
            default: break;
            }
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
