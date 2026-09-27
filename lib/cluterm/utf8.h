#ifndef __CLUTERM__UTF8_H__
#define __CLUTERM__UTF8_H__

#include <include/cluterm/utf8.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct UTF8_Decoder {
    int need_input;
    Rune rune;
} UTF8_Decoder;

bool utf8decoder_check(UTF8_Decoder *, char);
void utf8decoder_feed(UTF8_Decoder *, char);

// Assumes the input string is valid utf8 encoded, for error handled utf8
// decoding use `UTF8_Decoder` instead.
Rune utf8_decode(const char *);

#endif
