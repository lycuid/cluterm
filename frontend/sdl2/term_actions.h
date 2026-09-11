#ifndef __SDL2__HANDLERS__TERM_ACTION_H__
#define __SDL2__HANDLERS__TERM_ACTION_H__

#include <cluterm.h>

void set_window_title(const Cluterm *, const char *);
void query_palette_index(const Cluterm *, int);
void query_palette_fg(const Cluterm *);
void query_palette_bg(const Cluterm *);
void query_cursor_color(const Cluterm *);
void device_state_report(const Cluterm *);
void report_cursor_position(const Cluterm *);
void send_device_attributes(const Cluterm *);

#endif
