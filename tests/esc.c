#include <cluterm/vte.h>
#include <cluterm/vte/parser.h>
#include <criterion/criterion.h>
#include <stdio.h>

#define TestESCPayload(buffer, _expected)                                      \
    do {                                                                       \
        parser_feed(&vt_parser, buffer, strlen(buffer));                       \
        cr_assert(parser_run(&vt_parser) == EVENT_ESC);                        \
                                                                               \
        ESC_Payload *actual = &vt_parser.payload.esc, *expected = (_expected); \
                                                                               \
        cr_assert(actual->action == expected->action);                         \
                                                                               \
        cr_assert(actual->ninterm == expected->ninterm);                       \
        cr_assert_arr_eq(actual->interm, expected->interm, expected->ninterm); \
                                                                               \
        cr_assert(actual->final_byte == expected->final_byte);                 \
    } while (0)

static VT_Parser vt_parser;

#define REPR(x) [x] = #x
static const char *const ESC_Action_Repr[] = {
    REPR(ESC_IND),        REPR(ESC_RI),         REPR(ESC_HTS),
    REPR(ESC_CS_LINEGFX), REPR(ESC_CS_USASCII), REPR(ESC_DECSC),
    REPR(ESC_DECRC),      REPR(ESC_UNKNOWN),
};
#undef REPR

static void init(void) { parser_init(&vt_parser); }

static void fini(void)
{
    printf("[\033[32mPASS\033[m]: ESC::%s.\n",
           ESC_Action_Repr[vt_parser.payload.csi.action]);
}

TestSuite(ESC, .init = init, .fini = fini);

Test(ESC, ESC_IND)
{
    TestESCPayload("\033D",
                   (&(ESC_Payload){.action = ESC_IND, .final_byte = 'D'}));
}

Test(ESC, ESC_RI)
{
    TestESCPayload("\033M",
                   (&(ESC_Payload){.action = ESC_RI, .final_byte = 'M'}));
}

Test(ESC, ESC_HTS)
{
    TestESCPayload("\033H",
                   (&(ESC_Payload){.action = ESC_HTS, .final_byte = 'H'}));
}

Test(ESC, ESC_CS_LINEGFX)
{
    TestESCPayload("\033(0", (&(ESC_Payload){.action     = ESC_CS_LINEGFX,
                                             .ninterm    = 1,
                                             .interm     = "(",
                                             .final_byte = '0'}));
    TestESCPayload("\033)0", (&(ESC_Payload){.action     = ESC_CS_LINEGFX,
                                             .ninterm    = 1,
                                             .interm     = ")",
                                             .final_byte = '0'}));
    TestESCPayload("\033*0", (&(ESC_Payload){.action     = ESC_CS_LINEGFX,
                                             .ninterm    = 1,
                                             .interm     = "*",
                                             .final_byte = '0'}));
    TestESCPayload("\033+0", (&(ESC_Payload){.action     = ESC_CS_LINEGFX,
                                             .ninterm    = 1,
                                             .interm     = "+",
                                             .final_byte = '0'}));
}

Test(ESC, ESC_CS_USASCII)
{
    TestESCPayload("\033(B", (&(ESC_Payload){.action     = ESC_CS_USASCII,
                                             .ninterm    = 1,
                                             .interm     = "(",
                                             .final_byte = 'B'}));
    TestESCPayload("\033)B", (&(ESC_Payload){.action     = ESC_CS_USASCII,
                                             .ninterm    = 1,
                                             .interm     = ")",
                                             .final_byte = 'B'}));
    TestESCPayload("\033*B", (&(ESC_Payload){.action     = ESC_CS_USASCII,
                                             .ninterm    = 1,
                                             .interm     = "*",
                                             .final_byte = 'B'}));
    TestESCPayload("\033+B", (&(ESC_Payload){.action     = ESC_CS_USASCII,
                                             .ninterm    = 1,
                                             .interm     = "+",
                                             .final_byte = 'B'}));
}

Test(ESC, ESC_DECSC)
{
    TestESCPayload("\0337",
                   (&(ESC_Payload){.action = ESC_DECSC, .final_byte = '7'}));
}

Test(ESC, ESC_DECRC)
{
    TestESCPayload("\0338",
                   (&(ESC_Payload){.action = ESC_DECRC, .final_byte = '8'}));
}
