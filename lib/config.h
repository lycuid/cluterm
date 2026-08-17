#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <cluterm/vt/buffer.h>
#include <stdbool.h>

static const int Rows      = 30;
static const int Columns   = 100;
static const int TabWidth  = 8;
static const Rgb DefaultFG = 0xefefef;
static const Rgb DefaultBG = 0x090909;

static const char FontFamily[] = "FiraCode Nerd Font";
static const int FontSize      = 13;

static const Cursor DefaultCursor = {
    .color = DefaultFG,
    .style = CursorSolid, // CursorSolid | CursorBlink
    .shape = CursorBlock, // CursorBlock | CursorUnderline | CursorBar
};

#endif
