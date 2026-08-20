#ifndef __CLUTERM__CONFIG_H__
#define __CLUTERM__CONFIG_H__

#include <cluterm/colors.h>
#include <cluterm/vt/buffer.h>

typedef struct Config {
    const char *title;

    int rows, cols, tab_width;
    Rgb fg, bg;

    const char *font_family;
    int font_size;

    Cursor cursor;
} Config;

extern Config *cfg;
void init_config(void);

#endif
