#ifndef __INCLUDE__CLUTERM__UTF8_H__
#define __INCLUDE__CLUTERM__UTF8_H__

#include <stdint.h>
#include <sys/types.h>

#define UTF8_MAX_LEN 4

// Unicode code point.
typedef uint32_t Rune;

// UTF8 encoded string.
typedef char UTF8_String[UTF8_MAX_LEN + 1];
size_t utf8_encode(Rune, UTF8_String);

#endif
