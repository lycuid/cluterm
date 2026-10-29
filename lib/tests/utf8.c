#include <cluterm.h>
#include <criterion/criterion.h>

Test(utf8, rfc3629_valid_boundaries)
{
    struct {
        const char *name;
        const uchar bytes[4];
        size_t length;
        Rune expected;
    } cases[] = {
        {"U+0080 minimum two-byte", {0xc2, 0x80}, 2, 0x0080},
        {"U+07FF maximum two-byte", {0xdf, 0xbf}, 2, 0x07ff},
        {"U+0800 minimum three-byte", {0xe0, 0xa0, 0x80}, 3, 0x0800},
        {"U+D7FF before surrogate range", {0xed, 0x9f, 0xbf}, 3, 0xd7ff},
        {"U+E000 after surrogate range", {0xee, 0x80, 0x80}, 3, 0xe000},
        {"U+FFFF maximum three-byte", {0xef, 0xbf, 0xbf}, 3, 0xffff},
        {"U+10000 minimum four-byte", {0xf0, 0x90, 0x80, 0x80}, 4, 0x10000},
        {"U+10FFFF maximum Unicode", {0xf4, 0x8f, 0xbf, 0xbf}, 4, 0x10ffff},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        VT_Parser parser = {0};
        parser_init(&parser);
        parser_feed(&parser, cases[i].bytes, cases[i].length);

        FSM_Event event = parser_run(&parser);
        cr_expect_eq(event, EVENT_PRINT, "%s: event", cases[i].name);
        if (event == EVENT_PRINT)
            cr_expect_eq(parser.payload.value, cases[i].expected, "%s: rune",
                         cases[i].name);
    }
}

Test(utf8, real_world_mixed_text)
{
    /* "Cafe\u0301 — 東京 — 👩‍💻" encoded as UTF-8. */
    const uchar text[] = {
        'C',  'a',  'f',  'e',  0xcc, 0x81, ' ',  0xe2, 0x80, 0x94, ' ',
        0xe6, 0x9d, 0xb1, 0xe4, 0xba, 0xac, ' ',  0xe2, 0x80, 0x94, ' ',
        0xf0, 0x9f, 0x91, 0xa9, 0xe2, 0x80, 0x8d, 0xf0, 0x9f, 0x92, 0xbb,
    };
    const Rune expected[] = {
        'C',    'a',    'f', 'e',    0x0301, ' ',     0x2014, ' ',
        0x6771, 0x4eac, ' ', 0x2014, ' ',    0x1f469, 0x200d, 0x1f4bb,
    };
    VT_Parser parser = {0};
    parser_init(&parser);
    parser_feed(&parser, text, sizeof(text));

    for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i) {
        FSM_Event event = parser_run(&parser);
        cr_expect_eq(event, EVENT_PRINT, "mixed UTF-8 text rune %zu event", i);
        if (event == EVENT_PRINT)
            cr_expect_eq(parser.payload.value, expected[i],
                         "mixed UTF-8 text rune %zu", i);
    }
    cr_expect_eq(parser.scanner.cursor, parser.scanner.size,
                 "mixed UTF-8 text fully consumed");
}

Test(utf8, four_byte_sequence_split_across_feeds)
{
    const uchar scalar[] = {0xf0, 0x9f, 0x8c, 0x8d}; /* U+1F30D */

    for (size_t split = 1; split < sizeof(scalar); ++split) {
        VT_Parser parser = {0};
        parser_init(&parser);

        parser_feed(&parser, scalar, split);
        cr_expect_eq(parser_run(&parser), EVENT_NOOP,
                     "split %zu: incomplete prefix emits no event", split);

        parser_feed(&parser, scalar + split, sizeof(scalar) - split);
        FSM_Event event = parser_run(&parser);
        cr_expect_eq(event, EVENT_PRINT,
                     "split %zu: completed scalar emits print", split);
        if (event == EVENT_PRINT)
            cr_expect_eq(parser.payload.value, 0x1f30d,
                         "split %zu: completed scalar value", split);
    }
}

Test(utf8, rejects_rfc3629_malformed_sequences)
{
    struct {
        const char *name;
        const uchar *bytes;
        size_t length;
    } cases[] = {
        {"stray continuation", (const uchar[]){0x80}, 1},
        {"overlong two-byte", (const uchar[]){0xc0, 0xaf}, 2},
        {"overlong three-byte", (const uchar[]){0xe0, 0x80, 0x80}, 3},
        {"overlong four-byte", (const uchar[]){0xf0, 0x80, 0x80, 0x80}, 4},
        {"surrogate U+D800", (const uchar[]){0xed, 0xa0, 0x80}, 3},
        {"above U+10FFFF", (const uchar[]){0xf4, 0x90, 0x80, 0x80}, 4},
        {"reserved lead byte F5", (const uchar[]){0xf5, 0x80, 0x80, 0x80}, 4},
        {"invalid continuation", (const uchar[]){0xe2, 0x28, 0xa1}, 3},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        VT_Parser parser = {0};
        parser_init(&parser);
        parser_feed(&parser, cases[i].bytes, cases[i].length);

        FSM_Event event = parser_run(&parser);
        cr_expect_eq(event, EVENT_PRINT, "%s must emit a print event",
                     cases[i].name);
        cr_expect_eq(parser.payload.value, UTF8_REPLACEMENT_CHAR,
                     "%s must emit a replacement character", cases[i].name);
    }
}

Test(utf8, noop_partial_sequences)
{
    struct {
        const char *name;
        const uchar *bytes;
        size_t length;
    } cases[] = {
        {"partial two-byte", (const uchar[]){0xd1}, 1},
        {"partial three-byte", (const uchar[]){0xe2, 0x82}, 2},
        {"partial four-byte", (const uchar[]){0xf0, 0x9f, 0x8c}, 3},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        VT_Parser parser = {0};
        parser_init(&parser);
        parser_feed(&parser, cases[i].bytes, cases[i].length);

        FSM_Event event = parser_run(&parser);
        cr_expect_eq(event, EVENT_NOOP, "%s must emit a noop event",
                     cases[i].name);
    }
}

Test(utf8, encodes_rfc3629_valid_boundaries)
{
    struct {
        const char *name;
        Rune rune;
        const uchar bytes[UTF8_MAX_LEN];
        size_t length;
    } cases[] = {
        {"U+0000 minimum scalar", 0x0000, {0x00}, 1},
        {"U+007F maximum one-byte", 0x007f, {0x7f}, 1},
        {"U+0080 minimum two-byte", 0x0080, {0xc2, 0x80}, 2},
        {"U+07FF maximum two-byte", 0x07ff, {0xdf, 0xbf}, 2},
        {"U+0800 minimum three-byte", 0x0800, {0xe0, 0xa0, 0x80}, 3},
        {"U+D7FF before surrogate range", 0xd7ff, {0xed, 0x9f, 0xbf}, 3},
        {"U+E000 after surrogate range", 0xe000, {0xee, 0x80, 0x80}, 3},
        {"U+FFFF maximum three-byte", 0xffff, {0xef, 0xbf, 0xbf}, 3},
        {"U+10000 minimum four-byte", 0x10000, {0xf0, 0x90, 0x80, 0x80}, 4},
        {"U+10FFFF maximum scalar", 0x10ffff, {0xf4, 0x8f, 0xbf, 0xbf}, 4},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        UTF8_String encoded = {0};
        size_t length = utf8_encode(cases[i].rune, encoded);

        cr_expect_eq(length, cases[i].length, "%s: encoded length",
                     cases[i].name);
        for (size_t j = 0; j < cases[i].length; ++j)
            cr_expect_eq((uchar)encoded[j], cases[i].bytes[j],
                         "%s: byte %zu", cases[i].name, j);
    }
}

Test(utf8, encodes_real_world_mixed_text)
{
    /* "Cafe\u0301 — 東京 — 👩‍💻" encoded as UTF-8. */
    const Rune runes[] = {
        'C',    'a',    'f', 'e',    0x0301, ' ',     0x2014, ' ',
        0x6771, 0x4eac, ' ', 0x2014, ' ',    0x1f469, 0x200d, 0x1f4bb,
    };
    const uchar expected[] = {
        'C',  'a',  'f',  'e',  0xcc, 0x81, ' ',  0xe2, 0x80, 0x94, ' ',
        0xe6, 0x9d, 0xb1, 0xe4, 0xba, 0xac, ' ',  0xe2, 0x80, 0x94, ' ',
        0xf0, 0x9f, 0x91, 0xa9, 0xe2, 0x80, 0x8d, 0xf0, 0x9f, 0x92, 0xbb,
    };
    size_t offset = 0;

    for (size_t i = 0; i < sizeof(runes) / sizeof(runes[0]); ++i) {
        UTF8_String encoded = {0};
        size_t length = utf8_encode(runes[i], encoded);

        cr_expect(length <= UTF8_MAX_LEN,
                  "mixed text rune %zu fits in a UTF-8 scalar", i);
        cr_expect(offset <= sizeof(expected),
                  "mixed text rune %zu starts within expected bytes", i);
        for (size_t j = 0; j < length && j < UTF8_MAX_LEN; ++j) {
            cr_expect_lt(offset + j, sizeof(expected),
                         "mixed text rune %zu byte %zu is in range", i, j);
            if (offset + j < sizeof(expected))
                cr_expect_eq((uchar)encoded[j], expected[offset + j],
                             "mixed text rune %zu byte %zu", i, j);
        }
        offset += MIN(length, UTF8_MAX_LEN);
    }

    cr_expect_eq(offset, sizeof(expected), "all mixed-text bytes encoded");
}
