#include "config.h"
#include <config.h>

static Config config;
Config *cfg = &config;

void init_config(void)
{
    cfg->title       = Title;
    cfg->rows        = Rows;
    cfg->cols        = Columns;
    cfg->tab_width   = TabWidth;
    cfg->theme       = DefaultTheme;
    cfg->font_family = FontFamily;
    cfg->font_size   = FontSize;
    cfg->cursor      = DefaultCursor;
}
