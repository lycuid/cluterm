#include "main.h"
#include "SDL_keyboard.h"
#include "SDL_mutex.h"
#include "SDL_video.h"
#include "args.h"
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
const GFX_Context *gfx = &ctx; // exported as a global const ptr.

static pty_t pty           = {0};
static Cluterm term        = {0};
static SDL_mutex *vt_mutex = NULL;

#define GUARD(mutex)                                                           \
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

void signal_quit(__attribute__((unused)) int _) { quit(); }

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
    selection_clear();
    GUARD(vt_mutex) { cluterm_resize(&term, rows, cols); }
    pty_resize(&pty, rows, cols);
    frame_resize(&ctx.frame, rows, cols);
    gcache_resize(rows, cols);
    gfx_request_render(1);
}

ssize_t gfx_write(const char *buffer, size_t len)
{
    selection_clear();
    return pty_write(&pty, buffer, len);
}

void gfx_rebuild(const Config *config)
{
    calculate_font_metrics();
    gcache_destroy();
    gcache_init();

    int w, h;
    SDL_GetWindowSize(ctx.window, &w, &h);
    w -= config->padding.left + config->padding.right;
    h -= config->padding.top + config->padding.bottom;
    int cols = w / ctx.f_width, rows = h / ctx.f_height;
    resize(rows, cols);
}

static inline void sdl_init(void)
{
    tryn(SDL_Init(SDL_INIT_VIDEO));
    tryn(TTF_Init());

    {
        FcConfig *fconf = FcInitLoadConfigAndFonts();

        int size         = term.config.font_size;
        const char *face = term.config.font_family;

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

    ctx.window = tryp(SDL_CreateWindow(
        term.config.title, 280, 100,
        ctx.f_width * term.config.cols + term.config.padding.left +
            term.config.padding.right,
        ctx.f_height * term.config.rows + term.config.padding.top +
            term.config.padding.bottom,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE));
    ctx.renderer =
        tryp(SDL_CreateRenderer(ctx.window, -1, SDL_RENDERER_ACCELERATED));

    int rows = term.config.rows, cols = term.config.cols;

    gcache_init();
    resize(rows, cols);

    SDL_StartTextInput();
}

static inline void render(void)
{
    ClutermSnapshot *snap = &ctx.frame.term_snapshot;
    bool fresh            = fresh_render();

    GUARD(vt_mutex) { cluterm_snapshot(&term, snap); }
    frame_canvas_update(&ctx.frame, fresh);

    SDL_SetRenderDrawColor(ctx.renderer, UNPACK(snap->theme.bg), 0);
    SDL_RenderClear(ctx.renderer);
    SDL_RenderCopy(ctx.renderer, ctx.frame.canvas.texture,
                   &(SDL_Rect){.x = 0,
                               .y = 0,
                               .w = ctx.frame.canvas.dispw,
                               .h = ctx.frame.canvas.disph},
                   &(SDL_Rect){.x = term.config.padding.left,
                               .y = term.config.padding.top,
                               .w = ctx.frame.canvas.dispw,
                               .h = ctx.frame.canvas.disph});
    SDL_RenderPresent(ctx.renderer);
}

int pty_reader(__attribute__((unused)) void *arg)
{
    uchar stream[4096] = {0};
    ssize_t n          = 0;
    struct timespec ts = {.tv_nsec = 1e6};

    while (is_running()) {
        if ((n = pty_read(&pty, stream, sizeof(stream))) > 0) {
            GUARD(vt_mutex) { cluterm_feed(&term, stream, (size_t)n); }
            gfx_request_render(0);
        } else {
            nanosleep(&ts, &ts);
        }
    }
    return 0;
}

int main(int argc, char *const *argv)
{
    signal(SIGCHLD, signal_quit); // shell exits/crashes.
    signal(SIGTERM, signal_quit);

    static Selection sel = {-1, -1};
    ctx.sel              = &sel;
    ctx.frame            = (Frame){0};

    Config cfg       = {0};
    char *const *cmd = args_parse(argc, argv, &cfg);

    cluterm_init(&term, &cfg);
    term.actions = (ClutermActions){
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

    struct {
        uint64_t last;
        uint rows, cols, pending : 1;
    } resz = {0};

    vt_mutex           = SDL_CreateMutex();
    SDL_Thread *thread = SDL_CreateThread(pty_reader, NAME ":ptyread", NULL);

    for (SDL_Event e; is_running();) {

        if (frame_tick(&ctx.frame))
            gfx_request_render(0);

        if (resz.pending && since(&resz.last, FPS(2))) {
            resz.pending = 0;
            resize(resz.rows, resz.cols);
        }

        MouseReport mreport;
        GUARD(vt_mutex) { mreport = term.mouse_report; }

        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT: quit(); break;

            case SDL_WINDOWEVENT: {
                SDL_WindowEvent *win = &e.window;
                switch (win->event) {
                case SDL_WINDOWEVENT_EXPOSED: gfx_request_render(1); break;
                case SDL_WINDOWEVENT_CLOSE: quit(); break;

                case SDL_WINDOWEVENT_SIZE_CHANGED: {
                    resz.rows = MAX(
                        (win->data2 - cfg.padding.top - cfg.padding.bottom) /
                            ctx.f_height,
                        10),
                    resz.cols = MAX(
                        (win->data1 - cfg.padding.left - cfg.padding.right) /
                            ctx.f_width,
                        10);
                    resz.pending = 1;
                } break;
                }
            } break;

            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP: mouse_button(&e.button, &mreport); break;
            case SDL_MOUSEWHEEL: mouse_wheel(&e.wheel, &mreport); break;
            case SDL_MOUSEMOTION: mouse_motion(&e.motion, &mreport); break;

            case SDL_TEXTINPUT: {
                gfx_write(e.text.text, strlen(e.text.text));
                frame_activity(&ctx.frame);
            } break;

            case SDL_KEYDOWN: keydown(&e.key, &cfg); break;
            default: break;
            }
        }

        if (should_render())
            render();

        SDL_Delay(FPS(1000));
    }

    SDL_WaitThread(thread, NULL);

    pty_destroy(&pty);
    cluterm_destroy(&term);
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
