#include "args.h"
#include "../../default_config.h"
#include "main.h"
#include <cluterm/colors.h>
#include <cluterm/debug.h>
#include <stdio.h>
#include <string.h>

static const char usage[] =
    "Usage: " NAME " [options] [-e command [args...]]\n"
    "\n"
    "Options:\n"
    "  -h                  Show this help.\n"
    "  -t   <title>        Set window title.\n"
    "  -g   <geometry>     Set window geometry (COLSxROWS).\n"
    "  -c   <shape>        Set cursor shape (block|underline|bar).\n"
    "  -cb                 Enable blinking cursor.\n"
    "  -fg  <color>        Set foreground color (#RRGGBB).\n"
    "  -bg  <color>        Set background color (#RRGGBB).\n"
    "  -tw  <width>        Set tab width.\n"
    "  -pt  <size>         Set top padding.\n"
    "  -pr  <size>         Set right padding.\n"
    "  -pb  <size>         Set bottom padding.\n"
    "  -pl  <size>         Set left padding.\n"
    "  -fn  <font>         Set font family.\n"
    "  -fs  <size>         Set font size.\n"
    "  -e   <command> ...  Execute command and pass remaining arguments.\n";

static inline Rgb color256(uint8_t n)
{
    static const int cube[] = {0x00, 0x5f, 0x87, 0xaf, 0xd7, 0xff};

    Rgb color = 0;
    if (n <= 15)
        color = DefaultTheme.palette[n];
    else if (BETWEEN(n, 16, 231))
        for (int i = 0, m = n - 16; m; m /= 6)
            color |= cube[m % 6] << (8 * i++);
    else if (n >= 232)
        n = (n - 232) * 10 + 8, color = (n << 16) | (n << 8) | n;
    return color;
}

char *const *args_parse(int argc, char *const *argv, Config *cfg)
{
    cfg->title        = Title;
    cfg->rows         = Rows;
    cfg->cols         = Columns;
    cfg->tab_width    = TabWidth;
    cfg->padding      = Padding;
    cfg->font_family  = FontFamily;
    cfg->font_size    = FontSize;
    cfg->cursor_style = DefaultCursorStyle;
    cfg->cursor_shape = DefaultCursorShape;

    cfg->theme.fg     = DefaultTheme.fg;
    cfg->theme.bg     = DefaultTheme.bg;
    cfg->theme.cursor = DefaultTheme.cursor;
    for (size_t i = 0; i <= 255; ++i)
        cfg->theme.palette[i] = color256(i);

    for (--argc, ++argv; argc > 0; --argc, ++argv) {
        if (strcmp(*argv, "-h") == 0)
            die(0, "%s", usage);

        if (strcmp(*argv, "-t") == 0) {
            if (--argc <= 0)
                break;
            cfg->title = *++argv;
            continue;
        }

        if (strcmp(*argv, "-g") == 0) {
            if (--argc <= 0)
                break;
            int cols;
            if (sscanf(*++argv, "%dx%d", &cols, &cfg->rows) != 2)
                debug("Invalid geometry: '%s'.\n", *argv);
            else
                cfg->cols = cols;
            continue;
        }

        if (strcmp(*argv, "-c") == 0) {
            if (--argc <= 0)
                break;
            const char *const shape = *++argv;
            if (strcmp(shape, "block") == 0)
                cfg->cursor_shape = CursorBlock;
            else if (strcmp(shape, "underline") == 0)
                cfg->cursor_shape = CursorUnderline;
            else if (strcmp(shape, "bar") == 0)
                cfg->cursor_shape = CursorBar;
            else
                debug("Invalid cursor shape: '%s'.\n", *argv);
            continue;
        }

        if (strcmp(*argv, "-cb") == 0) {
            cfg->cursor_style = CursorBlink;
            continue;
        }

        if (strcmp(*argv, "-fg") == 0) {
            if (--argc <= 0)
                break;
            ++argv;
            Scanner s = SCANNER((const uchar *)*argv, strlen(*argv));
            if (!parse_rgb(&s, &cfg->theme.fg))
                debug("Invalid color format: '%s'.\n", *argv);
            continue;
        }

        if (strcmp(*argv, "-bg") == 0) {
            if (--argc <= 0)
                break;
            ++argv;
            Scanner s = SCANNER((const uchar *)*argv, strlen(*argv));
            if (!parse_rgb(&s, &cfg->theme.bg))
                debug("Invalid color format: '%s'.\n", *argv);
            continue;
        }

        if (strcmp(*argv, "-tw") == 0) {
            if (--argc <= 0)
                break;
            if (sscanf(*++argv, "%d", &cfg->tab_width) != 1)
                debug("Invalid tab width: '%s'.\n", *argv);
            continue;
        }

        if (strcmp(*argv, "-pt") == 0) {
            if (--argc <= 0)
                break;
            if (sscanf(*++argv, "%d", &cfg->padding.top) != 1)
                debug("Invalid top padding value: '%s'.\n", *argv);
            continue;
        }

        if (strcmp(*argv, "-pr") == 0) {
            if (--argc <= 0)
                break;
            if (sscanf(*++argv, "%d", &cfg->padding.right) != 1)
                debug("Invalid right padding value: '%s'.\n", *argv);
            continue;
        }

        if (strcmp(*argv, "-pb") == 0) {
            if (--argc <= 0)
                break;
            if (sscanf(*++argv, "%d", &cfg->padding.bottom) != 1)
                debug("Invalid bottom padding value: '%s'.\n", *argv);
            continue;
        }

        if (strcmp(*argv, "-pl") == 0) {
            if (--argc <= 0)
                break;
            if (sscanf(*++argv, "%d", &cfg->padding.left) != 1)
                debug("Invalid left padding value: '%s'.\n", *argv);
            continue;
        }

        if (strcmp(*argv, "-fn") == 0) {
            if (--argc <= 0)
                break;
            cfg->font_family = *++argv;
            continue;
        }

        if (strcmp(*argv, "-fs") == 0) {
            if (--argc <= 0)
                break;
            if (sscanf(*++argv, "%d", &cfg->font_size) != 1)
                debug("Invalid font size: '%s'.\n", *argv);
            continue;
        }

        if (strcmp(*argv, "-e") == 0)
            return ++argv;
    }
    return NULL;
}
