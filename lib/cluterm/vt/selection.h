#ifndef __CLUTERM__VT__SELECTION_H__
#define __CLUTERM__VT__SELECTION_H__

#include <include/cluterm.h>
#include <stdbool.h>

typedef struct Selection {
    int anchor, pointer;
} Selection;

void selection_start(Cluterm *, int, int);
void selection_extend(Cluterm *, int, int);
void selection_word(Cluterm *, int, int);
void selection_line(Cluterm *, int);
void selection_clear(Cluterm *);
char *selection_get_text(Cluterm *);

#endif
