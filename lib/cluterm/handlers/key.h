#ifndef __CLUTERM__HANDLERS_KEY_H__
#define __CLUTERM__HANDLERS_KEY_H__

#include <include/cluterm.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#define send_arrow_up(term, mods)    send_arrow(term, 'A', mods)
#define send_arrow_down(term, mods)  send_arrow(term, 'B', mods)
#define send_arrow_right(term, mods) send_arrow(term, 'C', mods)
#define send_arrow_left(term, mods)  send_arrow(term, 'D', mods)

ssize_t send_arrow(Cluterm *, char final, ClutermMod mod);
bool keydown(Cluterm *, const ClutermKey *key);

#endif
