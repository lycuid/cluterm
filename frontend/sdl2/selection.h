#ifndef __SDL2__SELECTION_H__
#define __SDL2__SELECTION_H__

#include <stdbool.h>

typedef struct Selection {
    int anchor, pointer;
} Selection;

void selection_start(int, int);
void selection_extend(int, int);
void selection_word(int, int);
void selection_line(int);
void selection_clear(void);
bool selection_contains(int, int);
char *selection_get_text(void);

#endif
