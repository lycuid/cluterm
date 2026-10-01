#include "main.h"
#include "SDL_keyboard.h"
#include "SDL_mutex.h"
#include "SDL_video.h"
#include "args.h"
#include "glyph_cache.h"
#include "handlers/keypress.h"
#include "pty.h"
#include <SDL2/SDL.h>
#include <cluterm.h>
#include <cluterm/debug.h>
#include <fontconfig/fontconfig.h>
#include <signal.h>
#include <stdatomic.h>
#include <sys/wait.h>
#include <time.h>

static GFX_Context ctx;
const GFX_Context *gfx = &ctx; // exported as a global const ptr.

static pty_t pty           = {0};
static Cluterm *term       = NULL;
static SDL_mutex *vt_mutex = NULL;

#define guard(mutex)                                                           \
    for (int i = (SDL_LockMutex(mutex), 1); i; i = (SDL_UnlockMutex(mutex), 0))

#define IS_ASCII(val) (val < 0x7f)

static atomic_bool         //
    running           = 1, //
    render_request    = 0, //
    full_frame_render = 0;

#define is_running() atomic_load_explicit(&running, memory_order_relaxed)
#define quit()       atomic_store_explicit(&running, 0, memory_order_relaxed)

#define should_render()                                                        \
    atomic_exchange_explicit(&render_request, 0, memory_order_acquire)
#define fresh_render()                                                         \
    atomic_exchange_explicit(&full_frame_render, 0, memory_order_relaxed)

static inline int get_keycode(SDL_KeyCode code)
{
    switch (code) {
    case SDLK_BACKSPACE: return CLUTERM_KEYCODE_BACKSPACE;
    case SDLK_TAB: return CLUTERM_KEYCODE_TAB;
    case SDLK_RETURN: // fallthrough
    case SDLK_RETURN2: return CLUTERM_KEYCODE_RETURN;
    case SDLK_ESCAPE: return CLUTERM_KEYCODE_ESCAPE;
    case SDLK_a: return CLUTERM_KEYCODE_a;
    case SDLK_b: return CLUTERM_KEYCODE_b;
    case SDLK_c: return CLUTERM_KEYCODE_c;
    case SDLK_d: return CLUTERM_KEYCODE_d;
    case SDLK_e: return CLUTERM_KEYCODE_e;
    case SDLK_f: return CLUTERM_KEYCODE_f;
    case SDLK_g: return CLUTERM_KEYCODE_g;
    case SDLK_h: return CLUTERM_KEYCODE_h;
    case SDLK_i: return CLUTERM_KEYCODE_i;
    case SDLK_j: return CLUTERM_KEYCODE_j;
    case SDLK_k: return CLUTERM_KEYCODE_k;
    case SDLK_l: return CLUTERM_KEYCODE_l;
    case SDLK_m: return CLUTERM_KEYCODE_m;
    case SDLK_n: return CLUTERM_KEYCODE_n;
    case SDLK_o: return CLUTERM_KEYCODE_o;
    case SDLK_p: return CLUTERM_KEYCODE_p;
    case SDLK_q: return CLUTERM_KEYCODE_q;
    case SDLK_r: return CLUTERM_KEYCODE_r;
    case SDLK_s: return CLUTERM_KEYCODE_s;
    case SDLK_t: return CLUTERM_KEYCODE_t;
    case SDLK_u: return CLUTERM_KEYCODE_u;
    case SDLK_v: return CLUTERM_KEYCODE_v;
    case SDLK_w: return CLUTERM_KEYCODE_w;
    case SDLK_x: return CLUTERM_KEYCODE_x;
    case SDLK_y: return CLUTERM_KEYCODE_y;
    case SDLK_z: return CLUTERM_KEYCODE_z;
    case SDLK_LEFTBRACKET: return CLUTERM_KEYCODE_LBRACKET;
    case SDLK_RIGHTBRACKET: return CLUTERM_KEYCODE_RBRACKET;
    case SDLK_F1: return CLUTERM_KEYCODE_F1;
    case SDLK_F2: return CLUTERM_KEYCODE_F2;
    case SDLK_F3: return CLUTERM_KEYCODE_F3;
    case SDLK_F4: return CLUTERM_KEYCODE_F4;
    case SDLK_F5: return CLUTERM_KEYCODE_F5;
    case SDLK_F6: return CLUTERM_KEYCODE_F6;
    case SDLK_F7: return CLUTERM_KEYCODE_F7;
    case SDLK_F8: return CLUTERM_KEYCODE_F8;
    case SDLK_F9: return CLUTERM_KEYCODE_F9;
    case SDLK_F10: return CLUTERM_KEYCODE_F10;
    case SDLK_F11: return CLUTERM_KEYCODE_F11;
    case SDLK_F12: return CLUTERM_KEYCODE_F12;
    case SDLK_UP: return CLUTERM_KEYCODE_UP;
    case SDLK_DOWN: return CLUTERM_KEYCODE_DOWN;
    case SDLK_RIGHT: return CLUTERM_KEYCODE_RIGHT;
    case SDLK_LEFT: return CLUTERM_KEYCODE_LEFT;
    case SDLK_HOME: return CLUTERM_KEYCODE_HOME;
    case SDLK_END: return CLUTERM_KEYCODE_END;
    case SDLK_INSERT: return CLUTERM_KEYCODE_INSERT;
    case SDLK_DELETE: return CLUTERM_KEYCODE_DELETE;
    case SDLK_PAGEUP: return CLUTERM_KEYCODE_PAGEUP;
    case SDLK_PAGEDOWN: return CLUTERM_KEYCODE_PAGEDOWN;
    default: return -1;
    }
}

static inline ClutermMod get_mod(SDL_Keymod keymod)
{
    ClutermMod mod = 0;
    if (IS_SET_ANY(keymod, KMOD_CTRL))
        SET(mod, CLUTERM_MOD_CTRL);
    if (IS_SET_ANY(keymod, KMOD_SHIFT))
        SET(mod, CLUTERM_MOD_SHIFT);
    if (IS_SET_ANY(keymod, KMOD_ALT))
        SET(mod, CLUTERM_MOD_ALT);
    return mod;
}

static inline void get_mouse_button(ClutermMouse *mouse,
                                    const SDL_MouseButtonEvent *button)
{

    mouse->button = button->button == SDL_BUTTON_LEFT     ? CLUTERM_MOUSE_LEFT
                    : button->button == SDL_BUTTON_MIDDLE ? CLUTERM_MOUSE_MIDDLE
                    : button->button == SDL_BUTTON_RIGHT  ? CLUTERM_MOUSE_RIGHT
                                                          : 0;
    int y         = button->y - gfx->config->padding.top,
        x         = button->x - gfx->config->padding.left;
    mouse->y      = y;
    mouse->x      = x;
    mouse->gridy  = y / ctx.f_height;
    mouse->gridx  = x / ctx.f_width;
    mouse->pressed = button->state == SDL_PRESSED;
}

static inline void get_mouse_wheel(ClutermMouse *mouse,
                                   const SDL_MouseWheelEvent *wheel)
{
    mouse->button  = 0;
    int y          = wheel->mouseY - gfx->config->padding.top,
        x          = wheel->mouseX - gfx->config->padding.left;
    mouse->y       = y;
    mouse->x       = x;
    mouse->gridy   = y / ctx.f_height;
    mouse->gridx   = x / ctx.f_width;
    mouse->wheely  = wheel->y;
    mouse->wheelx  = wheel->x;
    mouse->pressed = true;
}

static inline void get_mouse_motion(ClutermMouse *mouse,
                                    const SDL_MouseMotionEvent *motion)
{
    mouse->button = 0;
    if (IS_SET(motion->state, SDL_BUTTON_LMASK))
        SET(mouse->button, CLUTERM_MOUSE_LEFT);
    if (IS_SET(motion->state, SDL_BUTTON_MMASK))
        SET(mouse->button, CLUTERM_MOUSE_MIDDLE);
    if (IS_SET(motion->state, SDL_BUTTON_RMASK))
        SET(mouse->button, CLUTERM_MOUSE_RIGHT);

    int y          = motion->y - gfx->config->padding.top,
        x          = motion->x - gfx->config->padding.left;
    mouse->y       = y;
    mouse->x       = x;
    mouse->gridy   = y / ctx.f_height;
    mouse->gridx   = x / ctx.f_width;
    mouse->pressed = mouse->button > 0;
}

void signal_child(__attribute__((unused)) int _)
{
    pid_t pid;
    while ((pid = waitpid(-1, NULL, WNOHANG)) > 0) {
        if (pid == pty.shell)
            quit();
    }
}

void signal_quit(__attribute__((unused)) int sig) { quit(); }

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

void gfx_request_render(bool fresh)
{
    if (fresh)
        atomic_store_explicit(&full_frame_render, 1, memory_order_relaxed);
    atomic_store_explicit(&render_request, 1, memory_order_release);
}

static inline void destroy_fonts(void)
{
    for (size_t i = 0; i < LENGTH(ctx.fonts); ++i)
        if (ctx.fonts[i])
            TTF_CloseFont(ctx.fonts[i]);
}

void load_font(FcConfig *config, Dpi *dpi, const char *family, int size,
               const char *style, TTF_Font **font)
{
    FcPattern *pat =
        FcPatternBuild(NULL,                                 //
                       FC_FAMILY, FcTypeString, family,      // font family.
                       FC_STYLE, FcTypeString, style,        // font style.
                       FC_SIZE, FcTypeDouble, (double)size,  // font size.
                       FC_DPI, FcTypeDouble, (double)dpi->h, // font dpi.
                       NULL);

    FcConfigSubstitute(config, pat, FcMatchPattern);
    FcDefaultSubstitute(pat);

    FcResult res;
    FcPattern *font_pat = FcFontMatch(config, pat, &res);
    FcPatternDestroy(pat);
    if (font_pat) {
        FcChar8 *font_file = NULL;
        int font_size      = 11;
        FcPatternGetInteger(font_pat, FC_SIZE, 0, &font_size);
        if (FcPatternGetString(font_pat, FC_FILE, 0, &font_file) ==
            FcResultMatch)
            *font = TTF_OpenFontDPI((const char *)font_file, font_size, dpi->h,
                                    dpi->v);
        debug_1("font file: %s (%d).\n", font_file, font_size);
    }
    FcPatternDestroy(font_pat);
}

static inline void calculate_font_metrics(void)
{
    TTF_SizeText(ctx.fonts[FontBold], "M", &ctx.f_width, NULL);
    ctx.f_height = TTF_FontLineSkip(ctx.fonts[FontBold]);
}

static inline void resize(int rows, int cols)
{
    guard(vt_mutex) { cluterm_resize(term, rows, cols); }
    pty_resize(&pty, rows, cols);
    frame_resize(&ctx.frame, rows, cols);
    gcache_resize(rows, cols);
    gfx_request_render(1);
}

void gfx_rebuild(void)
{
    calculate_font_metrics();
    gcache_destroy();
    gcache_init();

    int w, h;
    SDL_GetWindowSize(ctx.window, &w, &h);
    w -= ctx.config->padding.left + ctx.config->padding.right;
    h -= ctx.config->padding.top + ctx.config->padding.bottom;
    int cols = w / ctx.f_width, rows = h / ctx.f_height;
    resize(rows, cols);
}

static inline void set_window_title(const char *title)
{
    if (strlen(title))
        SDL_SetWindowTitle(gfx->window, title);
}

static inline ssize_t send(const char *buffer, size_t len)
{
    frame_activity(&ctx.frame);
    return pty_write(&pty, buffer, len);
}

static inline void sdl_init(void)
{
    tryn(SDL_Init(SDL_INIT_VIDEO));
    tryn(TTF_Init());

    {
        FcConfig *fconf = FcInitLoadConfigAndFonts();

        int size         = ctx.config->font_size;
        const char *face = ctx.config->font_family;

        Dpi dpi;
        SDL_GetDisplayDPI(0, &dpi.d, &dpi.h, &dpi.v);

        load_font(fconf, &dpi, face, size, "Regular", &ctx.fonts[FontRegular]);
        load_font(fconf, &dpi, face, size, "Bold", &ctx.fonts[FontBold]);
        load_font(fconf, &dpi, face, size, "Italic", &ctx.fonts[FontItalic]);
        load_font(fconf, &dpi, face, size, "BoldItalic",
                  &ctx.fonts[FontBoldItalic]);

        FcConfigDestroy(fconf);
    }

    calculate_font_metrics();

    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");

    ctx.window = tryp(SDL_CreateWindow(
        ctx.config->title, 0, 0,
        ctx.f_width * ctx.config->cols + ctx.config->padding.left +
            ctx.config->padding.right,
        ctx.f_height * ctx.config->rows + ctx.config->padding.top +
            ctx.config->padding.bottom,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE));
    ctx.renderer =
        tryp(SDL_CreateRenderer(ctx.window, -1, SDL_RENDERER_ACCELERATED));

    int rows = ctx.config->rows, cols = ctx.config->cols;

    gcache_init();
    resize(rows, cols);

    SDL_StartTextInput();
}

static inline void render(void)
{
    ClutermSnapshot *snap = &ctx.frame.snapshot;
    bool fresh            = fresh_render();

    guard(vt_mutex) { cluterm_snapshot(term, snap); }
    frame_canvas_update(&ctx.frame, fresh);

    SDL_SetRenderDrawColor(ctx.renderer, UNPACK(snap->theme.bg), 0);
    SDL_RenderClear(ctx.renderer);
    SDL_RenderCopy(ctx.renderer, ctx.frame.canvas.texture,
                   &(SDL_Rect){.x = 0,
                               .y = 0,
                               .w = ctx.frame.canvas.dispw,
                               .h = ctx.frame.canvas.disph},
                   &(SDL_Rect){.x = ctx.config->padding.left,
                               .y = ctx.config->padding.top,
                               .w = ctx.frame.canvas.dispw,
                               .h = ctx.frame.canvas.disph});
    SDL_RenderPresent(ctx.renderer);
}

int pty_reader(__attribute__((unused)) void *arg)
{
    uint8_t stream[4096] = {0};
    ssize_t n            = 0;
    struct timespec ts   = {.tv_nsec = 1e6};

    while (is_running()) {
        if ((n = pty_read(&pty, stream, sizeof(stream))) > 0) {
            guard(vt_mutex) { cluterm_feed(term, stream, (size_t)n); }
            gfx_request_render(0);
        } else {
            nanosleep(&ts, &ts);
        }
    }
    return 0;
}

int main(int argc, char *const *argv)
{
    signal(SIGCHLD, signal_child);
    signal(SIGTERM, signal_quit);

    Config cfg       = {0};
    char *const *cmd = args_parse(argc, argv, &cfg);

    ctx.config = &cfg;
    ctx.frame  = (Frame){0};

    pty_open(&pty);
    term                   = cluterm_init(ctx.config);
    ClutermActions actions = {
        .set_window_title = set_window_title,
        .send             = send,
    };
    cluterm_set_actions(term, actions);

    sdl_init();

    char *shell[2] = {0};
    if (!cmd || !*cmd) {
        if (!(*shell = getenv("SHELL")))
            *shell = "/bin/sh";
        cmd = shell;
    }
    pty_spawn(&pty, cmd);

    struct {
        uint64_t last;
        uint rows, cols, pending : 1;
    } resz = {0};

    vt_mutex           = SDL_CreateMutex();
    SDL_Thread *thread = SDL_CreateThread(pty_reader, NAME ":ptyread", NULL);

    ClutermMouse mouse;
    for (SDL_Event e; is_running();) {

        if (frame_tick(&ctx.frame))
            gfx_request_render(0);

        if (resz.pending && since(&resz.last, FPS(2))) {
            resz.pending = 0;
            resize(resz.rows, resz.cols);
        }

        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT: quit(); break;

            case SDL_WINDOWEVENT: {
                SDL_WindowEvent *win = &e.window;
                switch (win->event) {
                case SDL_WINDOWEVENT_EXPOSED: gfx_request_render(1); break;
                case SDL_WINDOWEVENT_CLOSE: quit(); break;

                case SDL_WINDOWEVENT_SIZE_CHANGED: {
                    resz.rows    = MAX((win->data2 - ctx.config->padding.top -
                                        ctx.config->padding.bottom) /
                                           ctx.f_height,
                                       10),
                    resz.cols    = MAX((win->data1 - ctx.config->padding.left -
                                        ctx.config->padding.right) /
                                           ctx.f_width,
                                       10);
                    resz.pending = 1;
                } break;
                }
            } break;

            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP: {
                get_mouse_button(&mouse, &e.button);
                guard(vt_mutex) { cluterm_mouse_button(term, &mouse); }
            } break;
            case SDL_MOUSEWHEEL: {
                get_mouse_wheel(&mouse, &e.wheel);
                guard(vt_mutex) { cluterm_mouse_wheel(term, &mouse); }
            } break;
            case SDL_MOUSEMOTION: {
                get_mouse_motion(&mouse, &e.motion);
                guard(vt_mutex) { cluterm_mouse_motion(term, &mouse); }
            } break;

            case SDL_TEXTINPUT: {
                size_t len = strlen(e.text.text);
                if (len > 0) {
                    guard(vt_mutex)
                    {
                        cluterm_text_input(term, e.text.text, len);
                    }
                }
            } break;

            case SDL_KEYDOWN: {
                int code  = get_keycode(e.key.keysym.sym);
                bool done = false;
                if (code != -1) {
                    guard(vt_mutex)
                    {
                        ClutermKey key = {
                            .code = (ClutermKeyCode)code,
                            .mod  = get_mod(e.key.keysym.mod),
                        };
                        done = cluterm_keydown(term, &key);
                    }
                }
                if (!done)
                    handle_keydown(&e.key, ctx.config);
            } break;
            default: break;
            }
        }

        bool frame_ready = false;
        guard(vt_mutex) { frame_ready = cluterm_frame_ready(term); }

        if (should_render() || frame_ready)
            render();

        SDL_Delay(FPS(1000));
    }

    SDL_WaitThread(thread, NULL);

    pty_destroy(&pty);
    cluterm_destroy(term);
    {
        SDL_DestroyMutex(vt_mutex);
        frame_destroy(&ctx.frame);
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
