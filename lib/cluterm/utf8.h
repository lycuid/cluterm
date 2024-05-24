#ifndef __CLUTERM__UTF8_H__
#define __CLUTERM__UTF8_H__

#include <stdbool.h>
#include <stdint.h>

#define UTF8_MAX_LEN 4

// UTF8 encoded string.
typedef char UTF8_String[UTF8_MAX_LEN + 1];
// Unicode code point.
typedef uint32_t Rune;

typedef struct UTF8_Decoder {
    int need_input;
    Rune rune;
} UTF8_Decoder;

bool utf8decoder_check(UTF8_Decoder *, char);
void utf8decoder_feed(UTF8_Decoder *, char);

// Assumes the input string is valid utf8 encoded, for error handled utf8
// decoding use `UTF8_Decoder` instead.
Rune utf8_decode(const char *);
void utf8_encode(Rune, UTF8_String);

#endif
