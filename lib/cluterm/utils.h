#ifndef __CLUTERM__UTILS_H__
#define __CLUTERM__UTILS_H__

#include <stdint.h>

#define EXPORT __attribute__((unused)) static

#define LENGTH(iterable)         (sizeof(iterable) / sizeof(iterable[0]))
#define SET(state, mask)         ((state) |= (mask))
#define UNSET(state, mask)       ((state) &= ~(mask))
#define IS_SET(state, mask)      (((state) & (mask)) == (mask))
#define IS_SET_ANY(state, mask)  (((state) & (mask)) != 0)
#define UPDATE(state, mask, set) (set ? SET(state, mask) : UNSET(state, mask));

#define MIN(x, y)        ((x) < (y) ? (x) : (y))
#define MAX(x, y)        ((x) > (y) ? (x) : (y))
#define CLAMP(x, l, r)   ((x) < (l) ? (l) : (x) > (r) ? (r) : (x))
#define BETWEEN(i, l, h) ((l) <= (i) && (i) <= (h))
#define SWAP(i, j)                                                             \
    do {                                                                       \
        __typeof__(i) tmp = i;                                                 \
        i = j, j = tmp;                                                        \
    } while (0)

typedef uint32_t Rgb;

enum Margin { Top, Right, Bottom, Left };
enum FontType { FontRegular, FontBold, FontItalic, FontBoldItalic };

#endif
