#include "main.h"
#include "SDL_keyboard.h"
#include "SDL_video.h"
#include "cli.h"
#include "frame.h"
#include "glyph_cache.h"
#include "handlers/keypress.h"
#include "handlers/mouse.h"
#include "handlers/osc.h"
#include <SDL2/SDL.h>
#include <cluterm.h>
#include <cluterm/debug.h>
#include <cluterm/pty.h>
#include <cluterm/vt/buffer.h>
#include <config.h>
#include <fontconfig/fontconfig.h>
#include <signal.h>
#include <stdatomic.h>
#include <time.h>

static GFX_Context ctx;
const GFX_Context *gfx     = &ctx;
static Frame frame         = {0};
static SDL_mutex *vt_mutex = NULL;

#define IS_ASCII(val) (val < 0x7f)

#define GUARD(mu)                                                              \
    for (int i = SDL_LockMutex((mu)) == 0; i; i = (SDL_UnlockMutex((mu)), 0))

static atomic_bool         //
    running           = 1, //
    render_request    = 0, //
    full_frame_render = 0;

#define is_running() atomic_load_explicit(&running, memory_order_relaxed)
#define quit()       atomic_store_explicit(&running, 0, memory_order_relaxed)

void gfx_request_render(bool fresh)
{
    if (fresh)
        atomic_store_explicit(&full_frame_render, 1, memory_order_relaxed);
    atomic_store_explicit(&render_request, 1, memory_order_release);
}
#define should_render()                                                        \
    atomic_exchange_explicit(&render_request, 0, memory_order_acquire)
#define fresh_render()                                                         \
    atomic_exchange_explicit(&full_frame_render, 0, memory_order_relaxed)

#ifdef DEBUG_ATLAS
SDL_Window *debug_window;
SDL_Renderer *debug_renderer;
SDL_Texture *debug_texture;
#endif

void sigquit(__attribute__((unused)) int _) { quit(); }

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

void load_font(FcConfig *config, const char *family, int size,
               const char *style, TTF_Font **font)
{
    FcPattern *pat =
        FcPatternBuild(NULL,                                   //
                       FC_FAMILY, FcTypeString, family,        // font family.
                       FC_STYLE, FcTypeString, style,          // font style.
                       FC_SIZE, FcTypeDouble, (double)size,    // font size.
                       FC_DPI, FcTypeDouble, (double)ctx.hdpi, // font dpi.
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
            *font = TTF_OpenFontDPI((const char *)font_file, font_size,
                                    ctx.hdpi, ctx.vdpi);
        debug_1("font file: %s (%d).\n", font_file, font_size);
    }
    FcPatternDestroy(font_pat);
}

static inline void calculate_font_metrics(void)
{
    TTF_SizeText(ctx.fonts[FontBold], "M", &ctx.f_width, NULL);
    ctx.f_height = TTF_FontLineSkip(ctx.fonts[FontBold]);
}

void gfx_rebuild(Cluterm *term)
{
    calculate_font_metrics();
    int w, h;
    SDL_GetWindowSize(ctx.window, &w, &h);
    w -= term->config.padding.left + term->config.padding.right;
    h -= term->config.padding.top + term->config.padding.bottom;
    int cols = w / ctx.f_width, rows = h / ctx.f_height;

    GUARD(vt_mutex) { cluterm_resize(term, rows, cols); }
    frame_resize(&frame, rows, cols);

    gcache_destroy();
    gcache_init(rows, cols);
}

static inline void sdl_init(Cluterm *term)
{
    tryn(SDL_Init(SDL_INIT_VIDEO));
    tryn(TTF_Init());

    SDL_GetDisplayDPI(0, 0, &ctx.hdpi, &ctx.vdpi);
    {
        FcConfig *config = FcInitLoadConfigAndFonts();

        int size           = term->config.font_size;
        const char *family = term->config.font_family;

        load_font(config, family, size, "Regular", &ctx.fonts[FontRegular]);
        load_font(config, family, size, "Bold", &ctx.fonts[FontBold]);
        load_font(config, family, size, "Italic", &ctx.fonts[FontItalic]);
        load_font(config, family, size, "BoldItalic",
                  &ctx.fonts[FontBoldItalic]);

        FcConfigDestroy(config);
    }

    calculate_font_metrics();

    ctx.window = tryp(SDL_CreateWindow(
        term->config.title, 280, 100,
        ctx.f_width * term->config.cols + term->config.padding.left +
            term->config.padding.right,
        ctx.f_height * term->config.rows + term->config.padding.top +
            term->config.padding.bottom,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE));
    ctx.renderer =
        tryp(SDL_CreateRenderer(ctx.window, -1, SDL_RENDERER_ACCELERATED));

#ifdef DEBUG_ATLAS
    debug_window =
        tryp(SDL_CreateWindow(cfg->title, 0, 0, 0, 0, SDL_WINDOW_BORDERLESS));
    debug_renderer =
        tryp(SDL_CreateRenderer(debug_window, -1, SDL_RENDERER_ACCELERATED));
#endif

    frame_resize(&frame, term->config.rows, term->config.cols);
    gcache_init(term->config.rows, term->config.cols);

#ifdef DEBUG_ATLAS
    SDL_SetWindowSize(debug_window, 200 * ctx.f_width * 1.2f,
                      8 * ctx.f_height * 1.2f);
#endif
    SDL_StartTextInput();
}

static inline void render(Cluterm *term)
{
    bool fresh = fresh_render();

    if (term != NULL)
        GUARD(vt_mutex) { frame_capture(&frame, term); }
    frame_canvas_update(&frame, fresh);

    if (fresh) {
        SDL_SetRenderDrawColor(ctx.renderer, UNPACK(term->theme.bg), 0);
        SDL_RenderClear(ctx.renderer);
    }

    SDL_RenderCopy(
        ctx.renderer, frame.canvas.texture,
        &(SDL_Rect){
            .x = 0, .y = 0, .w = frame.canvas.dispw, .h = frame.canvas.disph},
        &(SDL_Rect){.x = term->config.padding.left,
                    .y = term->config.padding.top,
                    .w = frame.canvas.dispw,
                    .h = frame.canvas.disph});
    SDL_RenderPresent(ctx.renderer);
}

int pty_reader(void *arg)
{
    Cluterm *term      = (Cluterm *)arg;
    uchar stream[4096] = {0};
    ssize_t n          = 0;
    struct timespec ts = {.tv_nsec = 1e6};
    while (is_running()) {
        if ((n = pty_read(&term->pty, stream, sizeof(stream))) > 0) {
            GUARD(vt_mutex) { cluterm_write(term, stream, (size_t)n); }
            gfx_request_render(0);
        } else {
            nanosleep(&ts, &ts);
        }
    }
    return 0;
}

int main(int argc, char *const *argv)
{
    Cluterm term = {0};
    cluterm_init(&term);
    term.osc_handler = osc_handler;

    char *const *cmd = argparse(argc, argv, &term.config);
    char *shell[2]   = {0};
    if (!cmd || !*cmd) {
        if (!(*shell = getenv("SHELL")))
            *shell = "/bin/sh";
        cmd = shell;
    }
    cluterm_start(&term, cmd);

    sdl_init(&term);
    signal(SIGCHLD, sigquit); // shell exits/crashes.

    struct {
        uint64_t last;
        uint w, h, pending : 1;
    } resz = {0};

    vt_mutex           = SDL_CreateMutex();
    SDL_Thread *thread = SDL_CreateThread(pty_reader, NAME ":ptyread", &term);

    for (SDL_Event e; is_running();) {

        if (frame_tick(&frame))
            gfx_request_render(0);

        if (resz.pending && since(&resz.last, FPS(2))) {
            resz.pending = 0;
            GUARD(vt_mutex) { cluterm_resize(&term, resz.h, resz.w); }
            frame_resize(&frame, resz.h, resz.w);
            gcache_resize(resz.h, resz.w);
            gfx_request_render(1);
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
                    resz.w       = MAX((win->data1 - term.config.padding.left -
                                        term.config.padding.right) /
                                           ctx.f_width,
                                       10),
                    resz.h       = MAX((win->data2 - term.config.padding.top -
                                        term.config.padding.bottom) /
                                           ctx.f_height,
                                       10),
                    resz.pending = 1;
                } break;
                }
            } break;

            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP: mouse_button(&term, &e.button); break;
            case SDL_MOUSEWHEEL: mouse_wheel(&term, &e.wheel); break;
            case SDL_MOUSEMOTION: mouse_motion(&term, &e.motion); break;

            case SDL_TEXTINPUT: {
                pty_write(&term.pty, e.text.text, strlen(e.text.text));
                frame_activity(&frame);
            } break;

            case SDL_KEYDOWN: handle_keydown(&term, &e.key); break;
            default: break;
            }
        }

        if (should_render())
            render(&term);

#ifdef DEBUG_ATLAS
        SDL_SetRenderDrawColor(debug_renderer, UNPACK(0x0), 0);
        SDL_RenderClear(debug_renderer);
        SDL_RenderCopy(debug_renderer, debug_texture, NULL, NULL);
        SDL_RenderPresent(debug_renderer);
#endif

        SDL_Delay(FPS(1000));
    }

    SDL_WaitThread(thread, NULL);

    cluterm_destroy(&term);
    {
#ifdef DEBUG_ATLAS
        if (debug_renderer)
            SDL_DestroyRenderer(debug_renderer);
        if (debug_window)
            SDL_DestroyWindow(debug_window);
#endif
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
