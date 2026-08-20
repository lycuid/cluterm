#include "cli.h"
#include "main.h"
#include <cluterm/colors.h>
#include <cluterm/config.h>
#include <cluterm/debug.h>
#include <stdio.h>
#include <string.h>

static const char usage[] =
    "Usage: " NAME " [options] [-e command [args...]]\n"
    "\n"
    "Options:\n"
    "  -h  help        Show this help.\n"
    "  -t  title       Set window title.\n"
    "  -g  geometry    Set window window (COLSxROWS).\n"
    "  -fg color       Set foreground color (#RRGGBB).\n"
    "  -bg color       Set background color (#RRGGBB).\n"
    "  -tw width       Set tab width.\n"
    "  -fn font        Set font family.\n"
    "  -fs size        Set font size.\n"
    "  -e  command...  Execute command and pass remaining arguments.\n";

char *const *argparse(int argc, char *const *argv)
{

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

        if (strcmp(*argv, "-fg") == 0) {
            if (--argc <= 0)
                break;
            ++argv;
            Scanner s = SCANNER((const uchar *)*argv, strlen(*argv));
            if (!parse_rgb(&s, &cfg->fg))
                debug("Invalid color format: '%s'.\n", *argv);
            continue;
        }

        if (strcmp(*argv, "-bg") == 0) {
            if (--argc <= 0)
                break;
            ++argv;
            Scanner s = SCANNER((const uchar *)*argv, strlen(*argv));
            if (!parse_rgb(&s, &cfg->bg))
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
