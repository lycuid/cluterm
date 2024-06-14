#include <cluterm/vte.h>
#include <cluterm/vte/parser.h>
#include <criterion/criterion.h>
#include <stdio.h>

#define TestCTRLPayload(buffer, _expected)                                     \
    do {                                                                       \
        parser_feed(&vt_parser, buffer, strlen(buffer));                       \
        cr_assert(parser_run(&vt_parser) == EVENT_CTRL);                       \
                                                                               \
        CTRL_Payload *actual   = &vt_parser.payload.ctrl,                      \
                     *expected = (_expected);                                  \
                                                                               \
        cr_assert(actual->action == expected->action);                         \
        cr_assert(actual->value == expected->value);                           \
    } while (0)

static VT_Parser vt_parser;

#define REPR(x) [x] = #x
static const char *const CTRL_Action_Repr[] = {
    REPR(C0_BEL), REPR(C0_BS), REPR(C0_HT), REPR(C0_LF), REPR(C0_VT),
    REPR(C0_FF),  REPR(C0_CR), REPR(C0_SO), REPR(C0_SI),
};
#undef REPR

static void init(void) { parser_init(&vt_parser); }

static void fini(void)
{
    printf("[\033[32mPASS\033[m]: CTRL::%s.\n",
           CTRL_Action_Repr[vt_parser.payload.csi.action]);
}

TestSuite(CTRL, .init = init, .fini = fini);

Test(CTRL, C0_BEL)
{
    TestCTRLPayload("\x07", (&(CTRL_Payload){.action = C0_BEL, .value = 7}));
}

Test(CTRL, C0_BS)
{
    TestCTRLPayload("\x08", (&(CTRL_Payload){.action = C0_BS, .value = 8}));
}

Test(CTRL, C0_HT)
{
    TestCTRLPayload("\x09", (&(CTRL_Payload){.action = C0_HT, .value = 9}));
}

Test(CTRL, C0_LF)
{
    TestCTRLPayload("\x0a", (&(CTRL_Payload){.action = C0_LF, .value = 10}));
}

Test(CTRL, C0_VT)
{
    TestCTRLPayload("\x0b", (&(CTRL_Payload){.action = C0_VT, .value = 11}));
}

Test(CTRL, C0_FF)
{
    TestCTRLPayload("\x0c", (&(CTRL_Payload){.action = C0_FF, .value = 12}));
}

Test(CTRL, C0_CR)
{
    TestCTRLPayload("\x0d", (&(CTRL_Payload){.action = C0_CR, .value = 13}));
}

Test(CTRL, C0_SO)
{
    TestCTRLPayload("\x0e", (&(CTRL_Payload){.action = C0_SO, .value = 14}));
}

Test(CTRL, C0_SI)
{
    TestCTRLPayload("\x0f", (&(CTRL_Payload){.action = C0_SI, .value = 15}));
}
