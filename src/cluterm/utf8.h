#ifndef __UTF8_H__
#define __UTF8_H__

#include <stdint.h>
#include <stdlib.h>

#define UTF8_MAX_LEN 4

// UTF8 encoded string.
typedef char UTF8_String[UTF8_MAX_LEN + 1];
// Unicode code point.
typedef uint32_t Rune;

uint32_t utf8_decode(const char *, uint32_t, Rune *);
void utf8_encode(Rune, UTF8_String);

#endif
