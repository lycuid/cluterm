#include "main.h"
#include "SDL_keycode.h"
#include "SDL_video.h"
#include "cli.h"
#include "font.h"
#include "frame.h"
#include "glyph_cache.h"
#include "osc_handler.h"
#include <SDL2/SDL.h>
#include <cluterm.h>
#include <cluterm/config.h>
#include <cluterm/debug.h>
#include <cluterm/pty.h>
#include <cluterm/vt/buffer.h>
#include <fontconfig/fontconfig.h>
#include <signal.h>
#include <stdatomic.h>
#include <time.h>

#define IS_ASCII(val) (val < 0x7f)

#define GUARD(mu)                                                              \
    for (int i = SDL_LockMutex((mu)) == 0; i; i = (SDL_UnlockMutex((mu)), 0))

static bool fresh                 = 0;
static atomic_bool render_request = false;
static inline void request_render(bool full)
{
    atomic_store(&render_request, 1);
    fresh = full;
}

#define should_render() atomic_exchange(&render_request, 0)

static GFX_Context ctx;
const GFX_Context *gfx     = &ctx;
static Frame frame         = {0};
static SDL_mutex *vt_mutex = NULL;
static atomic_bool running = 1;
static int f_delta         = 0;

void quit(__attribute__((unused)) int _arg) { running = 0; }

static inline void *tryp(void *res)
{
    if (!res)
        die(1, "%s\n", SDL_GetError());
    return res;
}

static inline int tryn(int res)
{
    if (res < 0)
        die(1, "%s\n", SDL_GetError());
    return res;
}

static inline void destroy_fonts(void)
{
    for (size_t i = 0; i < LENGTH(ctx.fonts); ++i)
        if (ctx.fonts[i])
            TTF_CloseFont(ctx.fonts[i]);
}

static inline void reload_fonts(void)
{
    destroy_fonts();

    FcConfig *config = FcInitLoadConfigAndFonts();

    int size           = cfg->font_size + f_delta;
    const char *family = cfg->font_family;

    load_font(config, family, size, "Regular", &ctx.fonts[FontRegular]);
    load_font(config, family, size, "Bold", &ctx.fonts[FontBold]);
    load_font(config, family, size, "Italic", &ctx.fonts[FontItalic]);
    load_font(config, family, size, "BoldItalic", &ctx.fonts[FontBoldItalic]);

    FcConfigDestroy(config);

    TTF_SizeText(ctx.fonts[FontBold], "M", &ctx.f_width, NULL);
    ctx.f_height = TTF_FontLineSkip(ctx.fonts[FontBold]);
}

static inline void sdl_init(void)
{
    tryn(SDL_Init(SDL_INIT_VIDEO));
    tryn(TTF_Init());
    ctx.window = tryp(SDL_CreateWindow(
        cfg->title, 280, 100, 0, 0, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE));
    ctx.renderer =
        tryp(SDL_CreateRenderer(ctx.window, -1, SDL_RENDERER_ACCELERATED));

    reload_fonts();
    frame_resize(&frame, cfg->rows, cfg->cols);
    gcache_init();

    int w = ctx.f_width * cfg->cols, h = ctx.f_height * cfg->rows;
    SDL_SetWindowSize(ctx.window, w, h);
    SDL_StartTextInput();
}

static inline ssize_t clipboard_paste(const Cluterm *term)
{
    char *text = SDL_GetClipboardText();
    if (!text)
        return -1;

    if (IS_SET(term->mode, MODE_BRACKETED_PASTE))
        pty_write(&term->pty, "\x1b[200~", 6);

    size_t len = strlen(text);
    ssize_t n  = pty_write(&term->pty, text, len);

    if (IS_SET(term->mode, MODE_BRACKETED_PASTE))
        pty_write(&term->pty, "\x1b[201~", 6);

    SDL_free(text);
    return n;
}

static inline void handle_keydown(Cluterm *term, SDL_KeyboardEvent *key)
{
    bool ctrl  = IS_SET_ANY(key->keysym.mod, KMOD_CTRL),
         shift = IS_SET_ANY(key->keysym.mod, KMOD_SHIFT),
         alt   = IS_SET_ANY(key->keysym.mod, KMOD_ALT);

    switch (key->keysym.sym) {
    case SDLK_a: goto mod_put;
    case SDLK_b: goto mod_put;
    case SDLK_c: goto mod_put;
    case SDLK_d: goto mod_put;
    case SDLK_e: goto mod_put;
    case SDLK_f: goto mod_put;
    case SDLK_g: goto mod_put;
    case SDLK_h: goto mod_put;
    case SDLK_i: goto mod_put;
    case SDLK_j: goto mod_put;
    case SDLK_k: goto mod_put;
    case SDLK_l: goto mod_put;
    case SDLK_m: goto mod_put;
    case SDLK_n: goto mod_put;
    case SDLK_o: goto mod_put;
    case SDLK_p: goto mod_put;
    case SDLK_q: goto mod_put;
    case SDLK_r: goto mod_put;
    case SDLK_s: goto mod_put;
    case SDLK_t: goto mod_put;
    case SDLK_u: goto mod_put;
    case SDLK_v: {
        if (ctrl && shift)
            clipboard_paste(term);
        else
            goto mod_put;
    } break;
    case SDLK_w: goto mod_put;
    case SDLK_x: goto mod_put;
    case SDLK_y: goto mod_put;
    case SDLK_z: {
    mod_put:
        if (ctrl)
            pty_write(&term->pty, (char[]){key->keysym.sym - 'a' + 1}, 1);
        else if (alt)
            pty_write(&term->pty, (char[]){0x1b, key->keysym.sym}, 2);
    } break;

    case SDLK_0: // fallthrough
    case SDLK_KP_0:
        if (ctrl) {
            f_delta = 0;
            goto gfx_rebuild;
        }
        break;
    case SDLK_EQUALS: // fallthrough
    case SDLK_KP_EQUALS:
        if (ctrl) {
            f_delta++;
            goto gfx_rebuild;
        }
        break;
    case SDLK_MINUS: // fallthrough
    case SDLK_KP_MINUS:
        if (ctrl) {
            f_delta = MAX(1 - cfg->font_size, f_delta - 1);
            goto gfx_rebuild;
        }
        break;
    gfx_rebuild: {
        reload_fonts();

        int w, h;
        SDL_GetWindowSize(ctx.window, &w, &h);
        cfg->cols = w / ctx.f_width, cfg->rows = h / ctx.f_height;

        GUARD(vt_mutex) { cluterm_resize(term, cfg->rows, cfg->cols); }
        frame_resize(&frame, cfg->rows, cfg->cols);

        gcache_destroy();
        gcache_init();

        request_render(1);
    } break;

        // clang-format off
    case SDLK_RETURN:    // fallthrough
    case SDLK_RETURN2:   pty_write(&term->pty, "\r", 1);     break;
    case SDLK_TAB:       pty_write(&term->pty, "\t", 1);     break;
    case SDLK_BACKSPACE: pty_write(&term->pty, "\b", 1);     break;
    case SDLK_ESCAPE:    pty_write(&term->pty, "\x1b", 1);   break;
    case SDLK_UP:        pty_write(&term->pty, "\x1b[A", 3); break;
    case SDLK_DOWN:      pty_write(&term->pty, "\x1b[B", 3); break;
    case SDLK_RIGHT:     pty_write(&term->pty, "\x1b[C", 3); break;
    case SDLK_LEFT:      pty_write(&term->pty, "\x1b[D", 3); break;
    case SDLK_HOME:      pty_write(&term->pty, "\x1b[H", 3); break;
    case SDLK_END:       pty_write(&term->pty, "\x1b[F", 3); break;
    case SDLK_INSERT: {
        shift ? clipboard_paste(term) : pty_write(&term->pty, "\x1b[2~", 4);
    } break;
    case SDLK_DELETE:    pty_write(&term->pty, "\x1b[3~", 4); break;
    case SDLK_PAGEUP:    pty_write(&term->pty, "\x1b[5~", 4); break;
    case SDLK_PAGEDOWN:  pty_write(&term->pty, "\x1b[6~", 4); break;
        // clang-format on
    default: break;
    }
}

static inline void handle_userevent(SDL_UserEvent *user)
{
    switch (user->code) {
    case USEREVENT_SET_TITLE:
        if (user->data1) {
            SDL_SetWindowTitle(gfx->window, user->data1);
            free(user->data1);
        }
        break;
    }
}

static inline void render(Cluterm *term, bool fresh)
{
    if (term != NULL)
        GUARD(vt_mutex) { frame_capture(&frame, term); }
    frame_canvas_update(&frame, fresh);

    if (fresh) {
        SDL_SetRenderDrawColor(ctx.renderer, UNPACK(term->bg), 0);
        SDL_RenderClear(ctx.renderer);
    }
    SDL_Rect rect = {
        .x = 0, .y = 0, .w = frame.canvas.dispw, .h = frame.canvas.disph};
    SDL_RenderCopy(ctx.renderer, frame.canvas.texture, &rect, &rect);
    SDL_RenderPresent(ctx.renderer);
}

int read_thread(void *arg)
{
    Cluterm *term      = (Cluterm *)arg;
    uchar stream[4096] = {0};
    ssize_t n          = 0;
    struct timespec ts = {.tv_nsec = 1e6};
    while (atomic_load_explicit(&running, memory_order_relaxed)) {
        if ((n = pty_read(&term->pty, stream, sizeof(stream))) > 0) {
            GUARD(vt_mutex) { cluterm_write(term, stream, n); }
            request_render(0);
        } else {
            nanosleep(&ts, &ts);
        }
    }
    return 0;
}

int main(int argc, char *const *argv)
{
    init_config();

    char *const *cmd = argparse(argc, argv);
    char *shell[2]   = {0};
    if (!cmd || !*cmd) {
        if (!(*shell = getenv("SHELL")))
            *shell = "/bin/sh";
        cmd = shell;
    }

    debug_1("cfg->title(%s).\n", cfg->title);
    debug_1("cfg->fg(#%x).\n", cfg->fg);
    debug_1("cfg->bg(#%x).\n", cfg->bg);
    debug_1("cfg->tab_width(%d).\n", cfg->tab_width);
    debug_1("cfg->font_family(%s).\n", cfg->font_family);
    debug_1("cfg->font_size(%d).\n", cfg->font_size);

    Cluterm term = {0};
    cluterm_init(&term, cmd);
    term.osc_handler = osc_handler;

    sdl_init();
    signal(SIGCHLD, quit); // shell exits/crashes.

    struct {
        uint64_t last;
        uint w, h, pending : 1;
    } resz = {0};

    vt_mutex           = SDL_CreateMutex();
    SDL_Thread *thread = SDL_CreateThread(read_thread, "read_thread", &term);

    for (SDL_Event e; atomic_load_explicit(&running, memory_order_relaxed);) {

        if (frame_tick(&frame))
            request_render(0);

        if (resz.pending && since(&resz.last, FPS(2))) {
            resz.pending = 0;
            GUARD(vt_mutex) { cluterm_resize(&term, resz.h, resz.w); }
            frame_resize(&frame, resz.h, resz.w);
            gcache_resize(resz.h, resz.w);
            request_render(1);
        }

        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT: running = 0; break;

            case SDL_WINDOWEVENT: {
                SDL_WindowEvent *win = &e.window;
                switch (win->event) {
                case SDL_WINDOWEVENT_EXPOSED: request_render(0); break;
                case SDL_WINDOWEVENT_CLOSE: atomic_store(&running, 0); break;

                case SDL_WINDOWEVENT_SIZE_CHANGED: {
                    resz.w       = MAX(win->data1 / ctx.f_width, 10),
                    resz.h       = MAX(win->data2 / ctx.f_height, 10),
                    resz.pending = 1;
                } break;
                }
            } break;

            case SDL_TEXTINPUT: {
                pty_write(&term.pty, e.text.text, strlen(e.text.text));
                frame.cursor_blink_state.last    = SDL_GetTicks64(),
                frame.cursor_blink_state.visible = 1;
            } break;

            case SDL_KEYDOWN: handle_keydown(&term, &e.key); break;
            case SDL_USEREVENT: handle_userevent(&e.user); break;
            default: break;
            }
        }

        if (should_render()) {
            render(&term, fresh);
            fresh = 0;
        }

        SDL_Delay(FPS(1000));
    }

    SDL_WaitThread(thread, NULL);

    cluterm_destroy(&term);
    {
        SDL_DestroyMutex(vt_mutex);
        frame_destroy(&frame);
        gcache_destroy();
        destroy_fonts();
        if (ctx.renderer)
            SDL_DestroyRenderer(ctx.renderer);
        if (ctx.window)
            SDL_DestroyWindow(ctx.window);
        TTF_Quit();
        SDL_StopTextInput();
        SDL_Quit();
        FcFini();
        debug_1("SDL cleanup: Done!.\n");
    }
    return 0;
}
// vim:fdm=marker
