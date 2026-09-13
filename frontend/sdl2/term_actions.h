#ifndef __SDL2__HANDLERS__TERM_ACTION_H__
#define __SDL2__HANDLERS__TERM_ACTION_H__

#include <cluterm.h>

void set_window_title(const char *);
void query_palette_index(int);
void query_palette_fg(void);
void query_palette_bg(void);
void query_cursor_color(void);
void device_state_report(void);
void report_cursor_position(void);
void send_device_attributes(void);

#endif
