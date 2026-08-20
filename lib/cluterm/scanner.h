#ifndef __CLUTERM__SCANNER_H__
#define __CLUTERM__SCANNER_H__

#include <cluterm/util.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef unsigned char uchar;

typedef struct Scanner {
    const uchar *buffer;
    size_t cursor, size;
} Scanner;

#define SCANNER(str, len)                                                      \
    (Scanner) { .buffer = str, .cursor = 0, .size = len }

#define s_buffer(s) ((s)->buffer + (s)->cursor)
#define s_buflen(s) ((s)->size - (s)->cursor)
#define s_peek(s)   ((s)->cursor < (s)->size ? &(s)->buffer[(s)->cursor] : NULL)
#define s_advance(s) (s_advance_by(s, 1))

static inline size_t s_advance_by(Scanner *s, size_t inc)
{
    return s->cursor += inc;
}

static inline size_t s_consume(Scanner *s, uchar ch)
{
    return s->buffer[s->cursor] == ch && s_advance(s) > 0;
}

static inline size_t s_rollback(Scanner *s) { return --s->cursor; }

static inline uchar s_next(Scanner *s) { return s->buffer[s->cursor++]; }

static inline bool s_consume_string(Scanner *s, const char *str, size_t len)
{
    return s_peek(s) && (len <= s_buflen(s)) &&
           memcmp(str, s_buffer(s), len) == 0 && s_advance_by(s, len);
}

static inline int s_consume_number(Scanner *s)
{
    int n = 0;
    for (const uchar *ch = s_peek(s); ch && BETWEEN(*ch, '0', '9');
         ch              = s_peek(s))
        n = n * 10 + s_next(s) - '0';
    return n;
}

#endif
