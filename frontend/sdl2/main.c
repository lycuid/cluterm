#include "main.h"
#include "SDL_keyboard.h"
#include "SDL_video.h"
#include "cli.h"
#include "frame.h"
#include "glyph_cache.h"
#include "handlers/keypress.h"
#include "handlers/mouse.h"
#include "pty.h"
#include "term_actions.h"
#include <SDL2/SDL.h>
#include <cluterm.h>
#include <cluterm/debug.h>
#include <cluterm/vt/buffer.h>
#include <fontconfig/fontconfig.h>
#include <signal.h>
#include <stdatomic.h>
#include <time.h>

static GFX_Context ctx;
const GFX_Context *gfx = &ctx;
static Frame frame     = {0};
static pty_t pty;
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

#define should_render()                                                        \
    atomic_exchange_explicit(&render_request, 0, memory_order_acquire)
#define fresh_render()                                                         \
    atomic_exchange_explicit(&full_frame_render, 0, memory_order_relaxed)

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

static inline void request_render(bool fresh)
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
    GUARD(vt_mutex) { cluterm_resize(&ctx.term, rows, cols); }
    pty_resize(&pty, rows, cols);
    frame_resize(&frame, rows, cols);
    gcache_resize(rows, cols);
    request_render(1);
}

ssize_t gfx_write(const char *buffer, size_t len)
{
    return pty_write(&pty, buffer, len);
}

void gfx_rebuild(void)
{
    calculate_font_metrics();
    gcache_destroy();
    gcache_init();

    int w, h;
    SDL_GetWindowSize(ctx.window, &w, &h);
    w -= ctx.term.config.padding.left + ctx.term.config.padding.right;
    h -= ctx.term.config.padding.top + ctx.term.config.padding.bottom;
    int cols = w / ctx.f_width, rows = h / ctx.f_height;
    resize(rows, cols);
}

static inline void sdl_init(void)
{
    Cluterm *term = &ctx.term;
    tryn(SDL_Init(SDL_INIT_VIDEO));
    tryn(TTF_Init());

    {
        FcConfig *config = FcInitLoadConfigAndFonts();

        int size         = term->config.font_size;
        const char *face = term->config.font_family;

        Dpi dpi;
        SDL_GetDisplayDPI(0, &dpi.d, &dpi.h, &dpi.v);

        load_font(config, &dpi, face, size, "Regular", &ctx.fonts[FontRegular]);
        load_font(config, &dpi, face, size, "Bold", &ctx.fonts[FontBold]);
        load_font(config, &dpi, face, size, "Italic", &ctx.fonts[FontItalic]);
        load_font(config, &dpi, face, size, "BoldItalic",
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

    gcache_init();
    resize(term->config.rows, term->config.cols);

    SDL_StartTextInput();
}

static inline void render(void)
{
    const Cluterm *term = &ctx.term;
    bool fresh          = fresh_render();

    GUARD(vt_mutex) { frame_capture(&frame); }
    frame_canvas_update(&frame, fresh);

    SDL_SetRenderDrawColor(ctx.renderer, UNPACK(term->theme.bg), 0);
    SDL_RenderClear(ctx.renderer);
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
        if ((n = pty_read(&pty, stream, sizeof(stream))) > 0) {
            GUARD(vt_mutex) { cluterm_feed(term, stream, (size_t)n); }
            request_render(0);
        } else {
            nanosleep(&ts, &ts);
        }
    }
    return 0;
}

int main(int argc, char *const *argv)
{
    Config cfg       = {0};
    char *const *cmd = argparse(argc, argv, &cfg);

    ctx.term = (Cluterm){0};
    cluterm_init(&ctx.term, &cfg);
    ctx.term.actions = (ClutermActions){
        .set_window_title       = set_window_title,
        .query_palette_index    = query_palette_index,
        .query_palette_fg       = query_palette_fg,
        .query_palette_bg       = query_palette_bg,
        .query_cursor_color     = query_cursor_color,
        .device_state_report    = device_state_report,
        .report_cursor_position = report_cursor_position,
        .send_device_attributes = send_device_attributes,
    };

    char *shell[2] = {0};
    if (!cmd || !*cmd) {
        if (!(*shell = getenv("SHELL")))
            *shell = "/bin/sh";
        cmd = shell;
    }
    pty_spawn(&pty, cmd);

    sdl_init();
    signal(SIGCHLD, sigquit); // shell exits/crashes.

    struct {
        uint64_t last;
        uint w, h, pending : 1;
    } resz = {0};

    vt_mutex = SDL_CreateMutex();
    SDL_Thread *thread =
        SDL_CreateThread(pty_reader, NAME ":ptyread", &ctx.term);

    for (SDL_Event e; is_running();) {

        if (frame_tick(&frame))
            request_render(0);

        if (resz.pending && since(&resz.last, FPS(2))) {
            resz.pending = 0;
            resize(resz.h, resz.w);
        }

        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT: quit(); break;

            case SDL_WINDOWEVENT: {
                SDL_WindowEvent *win = &e.window;
                switch (win->event) {
                case SDL_WINDOWEVENT_EXPOSED: request_render(1); break;
                case SDL_WINDOWEVENT_CLOSE: quit(); break;

                case SDL_WINDOWEVENT_SIZE_CHANGED: {
                    resz.w = MAX((win->data1 - ctx.term.config.padding.left -
                                  ctx.term.config.padding.right) /
                                     ctx.f_width,
                                 10),
                    resz.h = MAX((win->data2 - ctx.term.config.padding.top -
                                  ctx.term.config.padding.bottom) /
                                     ctx.f_height,
                                 10),
                    resz.pending = 1;
                } break;
                }
            } break;

            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP: mouse_button(&e.button); break;
            case SDL_MOUSEWHEEL: mouse_wheel(&e.wheel); break;
            case SDL_MOUSEMOTION: mouse_motion(&e.motion); break;

            case SDL_TEXTINPUT: {
                gfx_write(e.text.text, strlen(e.text.text));
                frame_activity(&frame);
            } break;

            case SDL_KEYDOWN: handle_keydown(&e.key); break;
            default: break;
            }
        }

        if (should_render())
            render();

        SDL_Delay(FPS(1000));
    }

    SDL_WaitThread(thread, NULL);

    pty_destroy(&pty);
    cluterm_destroy(&ctx.term);
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
