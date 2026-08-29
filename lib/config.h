#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <cluterm/vt/buffer.h>

static const char Title[]      = "cluterm";
static const int Rows          = 36;
static const int Columns       = 120;
static const int TabWidth      = 8;
static const char FontFamily[] = "JetBrainsMono Nerd Font";
static const int FontSize      = 12;

static const Theme DefaultTheme = {
    .palette =
        {
            [0]  = 0x000000,
            [1]  = 0xee0000,
            [2]  = 0x00ee00,
            [3]  = 0xeedd00,
            [4]  = 0x0000ee,
            [5]  = 0xee00ee,
            [6]  = 0x00eeee,
            [7]  = 0xeeeeee,
            [8]  = 0xdddddd,
            [9]  = 0xffdddd,
            [10] = 0xddffdd,
            [11] = 0xffffdd,
            [12] = 0xddddff,
            [13] = 0xffddff,
            [14] = 0xddffff,
            [15] = 0xffffff,
        },
    .fg = 0xeeeeee,
    .bg = 0x000000,
};

static const Rgb DefaultCursorColor = DefaultTheme.fg;

// CursorBlock | CursorUnderline | CursorBar
static const CursorShape DefaultCursorShape = CursorBlock;

// CursorSolid | CursorBlink
static const CursorStyle DefaultCursorStyle = CursorSolid;

#endif
