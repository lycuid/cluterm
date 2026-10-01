#ifndef __INCLUDE__CLUTERM_H__
#define __INCLUDE__CLUTERM_H__

#include "cluterm/vt/buffer_line.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define LENGTH(iterable)         (sizeof(iterable) / sizeof(iterable[0]))
#define SET(state, mask)         ((state) |= (mask))
#define UNSET(state, mask)       ((state) &= ~(mask))
#define IS_SET(state, mask)      (((state) & (mask)) == (mask))
#define IS_SET_ANY(state, mask)  (((state) & (mask)) != 0)
#define UPDATE(state, mask, set) (set ? SET(state, mask) : UNSET(state, mask))

#define MIN(x, y)      ((x) < (y) ? (x) : (y))
#define MAX(x, y)      ((x) > (y) ? (x) : (y))
#define CLAMP(x, l, r) ((x) < (l) ? (l) : (x) > (r) ? (r) : (x))
#define SWAP(i, j)                                                             \
    do {                                                                       \
        __typeof__(i) tmp = i;                                                 \
        i = j, j = tmp;                                                        \
    } while (0)

enum FontType { FontRegular, FontBold, FontItalic, FontBoldItalic };

typedef enum ClutermKeyCode {
    CLUTERM_KEYCODE_BACKSPACE = 8,
    CLUTERM_KEYCODE_TAB       = 9,
    CLUTERM_KEYCODE_RETURN    = 10,
    CLUTERM_KEYCODE_ESCAPE    = 27,

    CLUTERM_KEYCODE_LBRACKET = '[',
    CLUTERM_KEYCODE_RBRACKET = ']',

    CLUTERM_KEYCODE_a = 97,
    CLUTERM_KEYCODE_b = 98,
    CLUTERM_KEYCODE_c = 99,
    CLUTERM_KEYCODE_d = 100,
    CLUTERM_KEYCODE_e = 101,
    CLUTERM_KEYCODE_f = 102,
    CLUTERM_KEYCODE_g = 103,
    CLUTERM_KEYCODE_h = 104,
    CLUTERM_KEYCODE_i = 105,
    CLUTERM_KEYCODE_j = 106,
    CLUTERM_KEYCODE_k = 107,
    CLUTERM_KEYCODE_l = 108,
    CLUTERM_KEYCODE_m = 109,
    CLUTERM_KEYCODE_n = 110,
    CLUTERM_KEYCODE_o = 111,
    CLUTERM_KEYCODE_p = 112,
    CLUTERM_KEYCODE_q = 113,
    CLUTERM_KEYCODE_r = 114,
    CLUTERM_KEYCODE_s = 115,
    CLUTERM_KEYCODE_t = 116,
    CLUTERM_KEYCODE_u = 117,
    CLUTERM_KEYCODE_v = 118,
    CLUTERM_KEYCODE_w = 119,
    CLUTERM_KEYCODE_x = 120,
    CLUTERM_KEYCODE_y = 121,
    CLUTERM_KEYCODE_z = 122,

    CLUTERM_KEYCODE_F1,
    CLUTERM_KEYCODE_F2,
    CLUTERM_KEYCODE_F3,
    CLUTERM_KEYCODE_F4,
    CLUTERM_KEYCODE_F5,
    CLUTERM_KEYCODE_F6,
    CLUTERM_KEYCODE_F7,
    CLUTERM_KEYCODE_F8,
    CLUTERM_KEYCODE_F9,
    CLUTERM_KEYCODE_F10,
    CLUTERM_KEYCODE_F11,
    CLUTERM_KEYCODE_F12,

    CLUTERM_KEYCODE_UP,
    CLUTERM_KEYCODE_DOWN,
    CLUTERM_KEYCODE_RIGHT,
    CLUTERM_KEYCODE_LEFT,
    CLUTERM_KEYCODE_HOME,
    CLUTERM_KEYCODE_END,
    CLUTERM_KEYCODE_INSERT,
    CLUTERM_KEYCODE_DELETE,
    CLUTERM_KEYCODE_PAGEUP,
    CLUTERM_KEYCODE_PAGEDOWN,
} ClutermKeyCode;

typedef uint8_t ClutermMod;
#define CLUTERM_MOD_CTRL  (1 << 0)
#define CLUTERM_MOD_SHIFT (1 << 1)
#define CLUTERM_MOD_ALT   (1 << 2)

typedef struct ClutermKey {
    ClutermKeyCode code;
    ClutermMod mod;
} ClutermKey;

typedef uint8_t ClutermMouseButton;
#define CLUTERM_MOUSE_LEFT   (1 << 0)
#define CLUTERM_MOUSE_MIDDLE (1 << 1)
#define CLUTERM_MOUSE_RIGHT  (1 << 2)

typedef struct ClutermMouse {
    int y, x;
    int gridy, gridx;
    int wheely, wheelx;
    bool pressed;
    ClutermMouseButton button;
    ClutermMod mod;
} ClutermMouse;

typedef enum CursorStyle { CursorSolid, CursorBlink } CursorStyle;
typedef enum CursorShape {
    CursorBlock,
    CursorUnderline,
    CursorBar
} CursorShape;

typedef struct Cursor {
    int y, x;
    bool visible : 1;
    CursorStyle style;
    CursorShape shape;
} Cursor;

#define MEMBERS_FRAME_BUFFER                                                   \
    int rows, cols;                                                            \
    BufferLine *lines;                                                         \
    bool *dirty;                                                               \
    Cursor cursor;

typedef struct Box {
    int top, right, bottom, left;
} Box;

typedef struct Config {
    const char *title;

    int rows, cols, tab_width;
    Box padding;

    const char *font_family;
    int font_size;

    Theme theme;

    CursorStyle cursor_style;
    CursorShape cursor_shape;
} Config;

typedef struct ClutermSnapshot {
    MEMBERS_FRAME_BUFFER

    Theme theme;
} ClutermSnapshot;

typedef struct ClutermActions {
    void (*set_window_title)(const char *); // OSC 1,2
    ssize_t (*send)(const char *, size_t);  // write to pty/tcp/ssh etc.
} ClutermActions;

typedef struct Cluterm Cluterm;

Cluterm *cluterm_init(const Config *);
void cluterm_set_actions(Cluterm *, ClutermActions);
void cluterm_feed(Cluterm *, const uchar *, size_t);
void cluterm_snapshot(Cluterm *, ClutermSnapshot *);
ssize_t cluterm_paste(Cluterm *, const char *);
void cluterm_resize(Cluterm *, int, int);
void cluterm_destroy(Cluterm *);
bool cluterm_keydown(Cluterm *, ClutermKey *);
bool cluterm_mouse_button(Cluterm *, ClutermMouse *);
bool cluterm_mouse_wheel(Cluterm *, ClutermMouse *);
bool cluterm_mouse_motion(Cluterm *, ClutermMouse *);
bool cluterm_frame_ready(Cluterm *);
ssize_t cluterm_text_input(Cluterm *, const char *, size_t);

#endif
