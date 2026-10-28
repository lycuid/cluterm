#ifndef __CLUTERM__UTF8_H__
#define __CLUTERM__UTF8_H__

#include <include/cluterm/scanner.h>
#include <include/cluterm/utf8.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct UTF8_Decoder {
    size_t remaining;
    Rune rune;
    uchar leading;
} UTF8_Decoder;

typedef enum UTF8DecodeResult {
    UTF8_DECODE_RESULT_INVALID = -1,
    UTF8_DECODE_RESULT_REJECT,
    UTF8_DECODE_RESULT_ACCEPT,
    UTF8_DECODE_RESULT_SUCCESS,
} UTF8DecodeResult;

#define utf8decoder_reset(decoder) memset((decoder), 0, sizeof(UTF8_Decoder))

UTF8DecodeResult utf8decoder_feed(UTF8_Decoder *, uchar);

// Assumes the input string is valid utf8 encoded, for error handled utf8
// decoding use `UTF8_Decoder` instead.
Rune utf8_decode(const char *);

#endif
