#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <cluterm/gfx.h>
#include <stdint.h>

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
    [FontRegular]    = "monospace-11",
    [FontBold]       = "monospace-11:bold",
    [FontItalic]     = "monospace-11:italic",
    [FontBoldItalic] = "monospace-11:bold:italic",
};

#endif
