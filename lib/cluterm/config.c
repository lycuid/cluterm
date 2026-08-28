#include "config.h"
#include <config.h>

static Config config;
Config *cfg = &config;

void init_config(void)
{
    cfg->title     = Title;
    cfg->rows      = Rows;
    cfg->cols      = Columns;
    cfg->tab_width = TabWidth;
    cfg->theme.fg  = DefaultTheme.fg;
    cfg->theme.bg  = DefaultTheme.bg;
    for (size_t i = 0; i <= 255; ++i)
        cfg->theme.palette[i] = color256(i);
    cfg->font_family = FontFamily;
    cfg->font_size   = FontSize;
    cfg->cursor      = DefaultCursor;
}
