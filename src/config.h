#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <cluterm/gfx.h>

static const int Margin[] = {
    [Top]    = 0,
    [Right]  = 0,
    [Bottom] = 0,
    [Left]   = 0,
};

static const int Rows      = 30;
static const int Columns   = 100;
static const int TabWidth  = 8;
static const Rgb DefaultFG = 0xefefef;
static const Rgb DefaultBG = 0x090909;

__attribute__((unused)) static const char *Fonts[4] = {
    [FontRegular]    = "FiraCode Nerd Font-12",
    [FontBold]       = "FiraCode Nerd Font-12:bold",
    [FontItalic]     = "FiraCode Nerd Font-12:italic",
    [FontBoldItalic] = "FiraCode Nerd Font-12:bold:italic",
};

#endif
