#include <cluterm/vt/parser.h>
#include <criterion/criterion.h>
#include <string.h>

static inline void expect_event(const char *name, const char *seq,
                                FSM_Event expected, int expected_action)
{
    VT_Parser parser = {0};
    parser_init(&parser);

    parser_feed(&parser, (const uchar *)seq, strlen(seq));
    cr_assert_eq(parser_run(&parser), expected, "%s: event", name);

    switch (expected) {
    case EVENT_ESC:
        cr_assert_eq(parser.payload.esc.action, expected_action, "%s: action",
                     name);
        break;
    case EVENT_CSI:
        cr_assert_eq(parser.payload.csi.action, expected_action, "%s: action",
                     name);
        break;
    case EVENT_CTRL:
        cr_assert_eq(parser.payload.ctrl.action, expected_action, "%s: action",
                     name);
        break;
    case EVENT_PRINT:
        cr_assert_eq(parser.payload.value, (Rune)expected_action, "%s: rune",
                     name);
        break;
    default: cr_assert_eq(1, 0, "Unsupported vt event!."); break;
    }
}

static inline void expect_osc(const char *name, const char *seq,
                              OSC_Action action, const char *payload)
{
    VT_Parser parser = {0};
    parser_init(&parser);

    parser_feed(&parser, (const uchar *)seq, strlen(seq));
    cr_assert_eq(parser_run(&parser), EVENT_OSC, "%s: event", name);
    cr_assert_eq(parser.payload.osc.action, action, "%s: OSC action", name);
    cr_assert_eq(s_buflen(&parser.payload.osc.scanner), strlen(payload),
                 "%s: payload length", name);
    cr_assert_eq(
        memcmp(s_buffer(&parser.payload.osc.scanner), payload, strlen(payload)),
        0, "%s: payload bytes", name);
}

Test(parser, printable_and_control)
{
    expect_event("ASCII printable", "A", EVENT_PRINT, 'A');
    expect_event("CR control", "\r", EVENT_CTRL, C0_CR);
    expect_event("LF control", "\n", EVENT_CTRL, C0_LF);
    expect_event("TAB control", "\t", EVENT_CTRL, C0_HT);
}

Test(parser, esc_sequences)
{
    expect_event("ESC D",
                 "\x1b"
                 "D",
                 EVENT_ESC, ESC_IND);
    expect_event("ESC M",
                 "\x1b"
                 "M",
                 EVENT_ESC, ESC_RI);
    expect_event("ESC H",
                 "\x1b"
                 "H",
                 EVENT_ESC, ESC_HTS);
    expect_event("ESC 7",
                 "\x1b"
                 "7",
                 EVENT_ESC, ESC_DECSC);
    expect_event("ESC 8",
                 "\x1b"
                 "8",
                 EVENT_ESC, ESC_DECRC);
}

Test(parser, csi_sequences)
{
    struct {
        const char *name;
        const char *seq;
        CSI_Action action;
    } cases[] = {
        {"CSI CUU", "\x1b[2A", CSI_CUU},
        {"CSI CUD", "\x1b[2B", CSI_CUD},
        {"CSI CUF", "\x1b[2C", CSI_CUF},
        {"CSI CUB", "\x1b[2D", CSI_CUB},
        {"CSI VPA", "\x1b[2d", CSI_VPA},
        {"CSI CNL", "\x1b[2E", CSI_CNL},
        {"CSI CPL", "\x1b[2F", CSI_CPL},
        {"CSI CHA", "\x1b[2G", CSI_CHA},
        {"CSI CUP", "\x1b[10;20H", CSI_CUP},
        {"CSI CHT", "\x1b[2I", CSI_CHT},
        {"CSI CBT", "\x1b[2Z", CSI_CBT},
        {"CSI TBC", "\x1b[0g", CSI_TBC},
        {"CSI ED", "\x1b[2J", CSI_ED},
        {"CSI EL", "\x1b[2K", CSI_EL},
        {"CSI IL", "\x1b[2L", CSI_IL},
        {"CSI DL", "\x1b[2M", CSI_DL},
        {"CSI ICH", "\x1b[2@", CSI_ICH},
        {"CSI DCH", "\x1b[2P", CSI_DCH},
        {"CSI ECH", "\x1b[2X", CSI_ECH},
        {"CSI SU", "\x1b[2S", CSI_SU},
        {"CSI SD", "\x1b[2T", CSI_SD},
        {"CSI DSR", "\x1b[6n", CSI_DSR},
        {"CSI HVP", "\x1b[10;20f", CSI_HVP},
        {"CSI SGR", "\x1b[31;1m", CSI_SGR},
        {"CSI SC", "\x1b[s", CSI_SC},
        {"CSI RC", "\x1b[u", CSI_RC},
        {"CSI DECSCUSR", "\x1b[2 q", CSI_DECSCUSR},
        {"CSI DECSTBM", "\x1b[1;24r", CSI_DECSTBM},
        {"CSI DECSET", "\x1b[?25h", CSI_DECSET},
        {"CSI DECRST", "\x1b[?25l", CSI_DECRST},
        {"CSI DA1", "\x1b[c", CSI_DA1},
    };

    cr_assert_eq(sizeof(cases) / sizeof(cases[0]), 31,
                 "test table should cover all supported CSI actions");
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
        expect_event(cases[i].name, cases[i].seq, EVENT_CSI, cases[i].action);
}

Test(parser, sgr_units)
{
    struct {
        const char *name;
        const char *seq;
        int nparam;
        int params[16];
    } cases[] = {
        {"SGR reset", "\x1b[m", 1, {0}},
        {"SGR explicit reset", "\x1b[0m", 1, {0}},
        {"SGR red", "\x1b[31m", 1, {31}},
        {"SGR bold+red", "\x1b[1;31m", 2, {1, 31}},
        {"SGR reset+bold+underline+colors",
         "\x1b[0;1;4;31;44m",
         5,
         {0, 1, 4, 31, 44}},
        {"SGR attribute reset combo",
         "\x1b[22;23;24;27;29;39;49;59m",
         8,
         {22, 23, 24, 27, 29, 39, 49, 59}},
        {"SGR bright foreground/background", "\x1b[91;104m", 2, {91, 104}},
        {"SGR 256-color", "\x1b[38;5;196m", 3, {38, 5, 196}},
        {"SGR 256-color foreground+background",
         "\x1b[38;5;196;48;5;21m",
         6,
         {38, 5, 196, 48, 5, 21}},
        {"SGR RGB", "\x1b[38;2;255;0;0m", 5, {38, 2, 255, 0, 0}},
        {"SGR RGB foreground+background",
         "\x1b[38;2;255;0;0;48;2;0;0;255m",
         10,
         {38, 2, 255, 0, 0, 48, 2, 0, 0, 255}},
        {"SGR colon-delimited color", "\x1b[38:5:196m", 3, {38, 5, 196}},
        {"SGR omitted middle parameter", "\x1b[1;;31m", 3, {1, 0, 31}},
        {"SGR trailing omitted parameter", "\x1b[31;m", 2, {31, 0}},
        {"SGR leading omitted parameter", "\x1b[;31m", 2, {0, 31}},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        VT_Parser parser = {0};
        parser_init(&parser);

        parser_feed(&parser, (const uchar *)cases[i].seq, strlen(cases[i].seq));
        cr_assert_eq(parser_run(&parser), EVENT_CSI, "%s: event",
                     cases[i].name);
        cr_assert_eq(parser.payload.csi.action, CSI_SGR, "%s: action",
                     cases[i].name);
        cr_assert_eq(parser.payload.csi.nparam, cases[i].nparam,
                     "%s: parameter count", cases[i].name);

        for (int j = 0; j < cases[i].nparam; ++j)
            cr_assert_eq(parser.payload.csi.param[j], cases[i].params[j],
                         "%s: parameter %d", cases[i].name, j);
    }
}

Test(parser, osc_supported_actions)
{
    struct {
        const char *name;
        const char *seq;
        OSC_Action action;
    } cases[] = {
        {"OSC 0", "\x1b]0;payload\a", OSC_0},
        {"OSC 2", "\x1b]2;payload\a", OSC_2},
        {"OSC 4", "\x1b]4;payload\a", OSC_4},
        {"OSC 7", "\x1b]7;payload\a", OSC_7},
        {"OSC 10", "\x1b]10;payload\a", OSC_10},
        {"OSC 11", "\x1b]11;payload\a", OSC_11},
        {"OSC 12", "\x1b]12;payload\a", OSC_12},
        {"OSC 104", "\x1b]104;payload\a", OSC_104},
        {"OSC 110", "\x1b]110;payload\a", OSC_110},
        {"OSC 111", "\x1b]111;payload\a", OSC_111},
        {"OSC 112", "\x1b]112;payload\a", OSC_112},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
        expect_osc(cases[i].name, cases[i].seq, cases[i].action, ";payload");
}

Test(parser, osc_introducers_and_terminators)
{
    expect_osc("OSC terminated by BEL", "\x1b]2;title\a", OSC_2, ";title");
    expect_osc("OSC terminated by 7-bit ST", "\x1b]2;title\x1b\\", OSC_2,
               ";title");
    expect_osc("OSC terminated by 8-bit ST", "\x1b]2;title\x9c", OSC_2,
               ";title");
    expect_osc("8-bit OSC introducer and terminator",
               "\x9d"
               "2;title\x9c",
               OSC_2, ";title");
}

Test(parser, invalid_sequences)
{
    expect_event("invalid CSI numeric param", "\x1b[1;aH", EVENT_CSI,
                 CSI_UNKNOWN);
    expect_event("invalid private mode", "\x1b[?]h", EVENT_CSI, CSI_UNKNOWN);
    expect_event("invalid ESC final byte", "\x1bY", EVENT_ESC, ESC_UNKNOWN);
}

Test(parser, utf8_rune)
{
    VT_Parser parser = {0};
    parser_init(&parser);

    parser_feed(&parser, (const uchar *)"\xC3\xA9", 2);
    cr_assert_eq(parser_run(&parser), EVENT_PRINT,
                 "UTF-8 rune emits EVENT_PRINT");
    cr_assert_eq(parser.payload.value, 0x00E9, "UTF-8 rune value");
}
