#include <cluterm/vte.h>
#include <cluterm/vte/parser.h>
#include <criterion/criterion.h>
#include <stdio.h>

#define TestCSIPayload(buffer, _expected)                                      \
    do {                                                                       \
        parser_feed(&vt_parser, buffer, strlen(buffer));                       \
        cr_assert(parser_run(&vt_parser) == EVENT_CSI);                        \
        CSI_Payload *actual = &vt_parser.payload.csi, *expected = (_expected); \
                                                                               \
        cr_assert(actual->action == expected->action);                         \
                                                                               \
        cr_assert(actual->nparam == expected->nparam);                         \
        cr_assert(actual->ninterm == expected->ninterm);                       \
                                                                               \
        cr_assert_arr_eq(actual->param, expected->param, expected->nparam);    \
        cr_assert_arr_eq(actual->interm, expected->interm, expected->ninterm); \
                                                                               \
        cr_assert(actual->final_byte == expected->final_byte);                 \
    } while (0)

static VT_Parser vt_parser;

#define REPR(x) [x] = #x
static const char *const CSI_Action_Repr[] = {
    REPR(CSI_CUU),     REPR(CSI_CUD),     REPR(CSI_CUF),    REPR(CSI_CUB),
    REPR(CSI_VPA),     REPR(CSI_CNL),     REPR(CSI_CPL),    REPR(CSI_CHA),
    REPR(CSI_CUP),     REPR(CSI_CHT),     REPR(CSI_CBT),    REPR(CSI_TBC),
    REPR(CSI_ED),      REPR(CSI_EL),      REPR(CSI_IL),     REPR(CSI_DL),
    REPR(CSI_ICH),     REPR(CSI_DCH),     REPR(CSI_ECH),    REPR(CSI_SU),
    REPR(CSI_SD),      REPR(CSI_HVP),     REPR(CSI_SGR),    REPR(CSI_SC),
    REPR(CSI_RC),      REPR(CSI_DECSTBM), REPR(CSI_DECSET), REPR(CSI_DECRST),
    REPR(CSI_UNKNOWN),
};
#undef REPR

static void init(void) { parser_init(&vt_parser); }

static void fini(void)
{
    printf("[\033[32mPASS\033[m]: CSI::%s.\n",
           CSI_Action_Repr[vt_parser.payload.csi.action]);
}

TestSuite(CSI, .init = init, .fini = fini);

Test(CSI, CUU)
{
    TestCSIPayload("\033[A", (&(CSI_Payload){.action     = CSI_CUU,
                                             .nparam     = 1,
                                             .param      = {0},
                                             .ninterm    = 0,
                                             .final_byte = 'A'}));

    TestCSIPayload("\033[69A", (&(CSI_Payload){.action     = CSI_CUU,
                                               .nparam     = 1,
                                               .param      = {69},
                                               .ninterm    = 0,
                                               .final_byte = 'A'}));
}

Test(CSI, CUD)
{
    TestCSIPayload("\033[B", (&(CSI_Payload){.action     = CSI_CUD,
                                             .nparam     = 1,
                                             .param      = {0},
                                             .ninterm    = 0,
                                             .final_byte = 'B'}));

    TestCSIPayload("\033[69B", (&(CSI_Payload){.action     = CSI_CUD,
                                               .nparam     = 1,
                                               .param      = {69},
                                               .ninterm    = 0,
                                               .final_byte = 'B'}));
}

Test(CSI, CUF)
{
    TestCSIPayload("\033[C", (&(CSI_Payload){.action     = CSI_CUF,
                                             .nparam     = 1,
                                             .param      = {0},
                                             .ninterm    = 0,
                                             .final_byte = 'C'}));

    TestCSIPayload("\033[69C", (&(CSI_Payload){.action     = CSI_CUF,
                                               .nparam     = 1,
                                               .param      = {69},
                                               .ninterm    = 0,
                                               .final_byte = 'C'}));
}

Test(CSI, CUB)
{
    TestCSIPayload("\033[D", (&(CSI_Payload){.action     = CSI_CUB,
                                             .nparam     = 1,
                                             .param      = {0},
                                             .ninterm    = 0,
                                             .final_byte = 'D'}));

    TestCSIPayload("\033[69D", (&(CSI_Payload){.action     = CSI_CUB,
                                               .nparam     = 1,
                                               .param      = {69},
                                               .ninterm    = 0,
                                               .final_byte = 'D'}));
}

Test(CSI, CNL)
{
    TestCSIPayload("\033[E", (&(CSI_Payload){.action     = CSI_CNL,
                                             .nparam     = 1,
                                             .param      = {0},
                                             .ninterm    = 0,
                                             .final_byte = 'E'}));

    TestCSIPayload("\033[69E", (&(CSI_Payload){.action     = CSI_CNL,
                                               .nparam     = 1,
                                               .param      = {69},
                                               .ninterm    = 0,
                                               .final_byte = 'E'}));
}

Test(CSI, CPL)
{
    TestCSIPayload("\033[F", (&(CSI_Payload){.action     = CSI_CPL,
                                             .nparam     = 1,
                                             .param      = {0},
                                             .ninterm    = 0,
                                             .final_byte = 'F'}));

    TestCSIPayload("\033[69F", (&(CSI_Payload){.action     = CSI_CPL,
                                               .nparam     = 1,
                                               .param      = {69},
                                               .ninterm    = 0,
                                               .final_byte = 'F'}));
}

Test(CSI, SGR_no_params)
{
    TestCSIPayload("\033[m", (&(CSI_Payload){.action     = CSI_SGR,
                                             .nparam     = 1,
                                             .param      = {0},
                                             .ninterm    = 0,
                                             .final_byte = 'm'}));
}

Test(CSI, SGR_0)
{
    TestCSIPayload("\033[0m", (&(CSI_Payload){.action     = CSI_SGR,
                                              .nparam     = 1,
                                              .param      = {0},
                                              .ninterm    = 0,
                                              .final_byte = 'm'}));
}

Test(CSI, SGR_1)
{
    TestCSIPayload("\033[1m", (&(CSI_Payload){.action     = CSI_SGR,
                                              .nparam     = 1,
                                              .param      = {1},
                                              .ninterm    = 0,
                                              .final_byte = 'm'}));
}

Test(CSI, SGR_38_colors_256)
{
    TestCSIPayload("\033[38;5;110m", (&(CSI_Payload){.action     = CSI_SGR,
                                                     .nparam     = 3,
                                                     .param      = {38, 5, 110},
                                                     .ninterm    = 0,
                                                     .final_byte = 'm'}));
}

Test(CSI, SGR_38_colors_rgb)
{
    TestCSIPayload("\033[38;2;255;255;255m",
                   (&(CSI_Payload){.action     = CSI_SGR,
                                   .nparam     = 5,
                                   .param      = {38, 2, 0, 0, 0},
                                   .ninterm    = 0,
                                   .final_byte = 'm'}));
}

Test(CSI, SGR_48_colors_256)
{
    TestCSIPayload("\033[48;5;110m", (&(CSI_Payload){.action     = CSI_SGR,
                                                     .nparam     = 3,
                                                     .param      = {48, 5, 110},
                                                     .ninterm    = 0,
                                                     .final_byte = 'm'}));
}

Test(CSI, SGR_48_colors_rgb)
{
    TestCSIPayload("\033[48;2;255;255;255m",
                   (&(CSI_Payload){.action     = CSI_SGR,
                                   .nparam     = 5,
                                   .param      = {48, 2, 0, 0, 0},
                                   .ninterm    = 0,
                                   .final_byte = 'm'}));
}

Test(CSI, SGR_single_param)
{
    TestCSIPayload("\033[2m", (&(CSI_Payload){.action     = CSI_SGR,
                                              .nparam     = 1,
                                              .param      = {2},
                                              .ninterm    = 0,
                                              .final_byte = 'm'}));
}

Test(CSI, SGR_multiple_params)
{
    TestCSIPayload("\033[2;3;1m", (&(CSI_Payload){.action     = CSI_SGR,
                                                  .nparam     = 3,
                                                  .param      = {2, 3, 1},
                                                  .ninterm    = 0,
                                                  .final_byte = 'm'}));
}
