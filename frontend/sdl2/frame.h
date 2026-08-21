#ifndef __SDL2__FRAME_H__
#define __SDL2__FRAME_H__

#include <SDL2/SDL.h>
#include <cluterm.h>

#define FPS(n) (1000 / n)

typedef struct FrameCanvas {
    SDL_Texture *texture;
    size_t w, h, dispw, disph;
} FrameCanvas;

typedef struct Frame {
    struct FrameBuffer {
        MEMBERS_FRAME_BUFFER;
    } buffer;

    struct {
        bool visible;
        uint64_t last;
    } cursor_blink_state;

    FrameCanvas canvas;
} Frame;

static inline bool since(uint64_t *time, uint64_t ms)
{
    if (time == NULL)
        return false;
    uint64_t tick = SDL_GetTicks64();
    bool result   = tick - *time > ms;
    if (result)
        *time = tick;
    return result;
}

void frame_resize(Frame *, int, int);
void frame_capture(Frame *, const Cluterm *);
void frame_canvas_update(Frame *, bool);
bool frame_tick(Frame *);
void frame_cursor_activity(Frame *);
void frame_destroy(Frame *);

#endif
