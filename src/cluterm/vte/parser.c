// vim:fdm=marker
#include "parser.h"
#include <cluterm/utf8.h>
#include <cluterm/utils.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// clang-format off

typedef unsigned char uchar;

#define IS_C0(ch)   ((ch) <= 0x1f || (ch) == 0x7f)
#define IS_C1(ch)   BETWEEN(ch, 0x80, 0x9f)
#define IS_CTRL(ch) (IS_C0(ch) || IS_C1(ch))

#define IS_CSI_PARAM(ch) BETWEEN(ch, 0x30, 0x3f)
#define IS_INTERM(ch)    BETWEEN(ch, 0x20, 0x2f)
#define IS_ESC_FINAL(ch) BETWEEN(ch, 0x30, 0x7e)
#define IS_CSI_FINAL(ch) BETWEEN(ch, 0x40, 0x7e)
#define IS_PRINTABLE(ch) BETWEEN(ch, 0x20, 0x7f)

#define r_buffer(r)          ((r)->buffer + (r)->cursor)
#define r_buflen(r)          ((r)->size - (r)->cursor)
#define r_peek(r)            ((r)->cursor < (r)->size ? &(r)->buffer[(r)->cursor] : NULL)
#define r_next(r)            ((r)->buffer[(r)->cursor++])
#define r_advance(r)         (r_advance_by(r, 1))
#define r_advance_by(r, inc) ((r)->cursor += inc)
#define r_rollback(r)        (--(r)->cursor)
#define r_consume(r, ch)     ((r)->buffer[(r)->cursor] == ch && r_advance(r) > 0)

#define r_consume_param_delim(p) (r_consume((p), ';') || r_consume((p), ':'))

static inline int r_consume_number(Reader *r)
{
    int n = 0;
    for (const char *ch = r_peek(r); ch && BETWEEN(*ch, '0', '9');
         ch             = r_peek(r))
        n = n * 10 + r_next(r) - '0';
    return n;
}

static inline void collect(VT_Parser *, uchar);
static inline void forward(VT_Parser *, FSM_State);
static inline void transition(VT_Parser *, FSM_State);
static inline void dispatch(VT_Parser *, FSM_Event);

static inline void prepare_ctrl_payload(VT_Parser *, CTRL_Payload *);
static inline void prepare_esc_payload(VT_Parser *, ESC_Payload *);
static inline void prepare_csi_payload(VT_Parser *, CSI_Payload *);
static inline void prepare_osc_payload(VT_Parser *, OSC_Payload *);

void parser_init(VT_Parser *vtp) { transition(vtp, STATE_GROUND); }

void parser_feed(VT_Parser *vtp, const char *stream, uint32_t slen)
{
    vtp->reader = READER(stream, slen);
}

FSM_Event parser_run(VT_Parser *vtp)
{
    if (vtp->fsm.dispatching)
        transition(vtp, STATE_GROUND);
    vtp->fsm.dispatching = false;

    for (Reader *r = &vtp->reader;
         !vtp->fsm.dispatching && r_peek(r) != NULL;) {
        uchar input = r_next(r);

        switch (vtp->fsm.state) {
        case STATE_GROUND: {
            switch (input) {
            case 0x1b: transition(vtp, STATE_ESC); break;
            case 0x9b: transition(vtp, STATE_CSI_PARAM); break;
            case 0x9d: transition(vtp, STATE_OSC_STRING); break;
            default: {
                if (IS_CTRL(input))
                    dispatch(vtp, EVENT_CTRL);
                else if (utf8decoder_check(&vtp->utf8_decoder, input))
                    vtp->utf8_decoder.need_input
                        ? transition(vtp, STATE_UTF8_DECODE)
                        : dispatch(vtp, EVENT_PRINT);
            } break;
            }
        } break;
        case STATE_UTF8_DECODE: {
            if (vtp->utf8_decoder.need_input)
                utf8decoder_feed(&vtp->utf8_decoder, input);
            // 'need_input' gets updated in the function call above.
            if (!vtp->utf8_decoder.need_input)
                dispatch(vtp, EVENT_PRINT);
        } break;
        case STATE_ESC: {
            switch (input) {
            case '[': transition(vtp, STATE_CSI_PARAM); break;
            case ']': transition(vtp, STATE_OSC_STRING); break;
            default: forward(vtp, STATE_ESC_INTERM); break;
            }
        } break;
        case STATE_ESC_INTERM: {
            if (IS_INTERM(input))
                collect(vtp, input);
            else
                forward(vtp, STATE_ESC_FINAL);
        } break;
        case STATE_ESC_FINAL: {
            if (IS_ESC_FINAL(input))
                collect(vtp, input);
            else
                forward(vtp, STATE_GROUND);
        } break;
        case STATE_CSI_PARAM: {
            if (IS_CSI_PARAM(input))
                collect(vtp, input);
            else
                forward(vtp, STATE_CSI_INTERM);
        } break;
        case STATE_CSI_INTERM: {
            if (IS_INTERM(input))
                collect(vtp, input);
            else
                forward(vtp, STATE_CSI_FINAL);
        } break;
        case STATE_CSI_FINAL: {
            if (IS_CSI_FINAL(input))
                collect(vtp, input);
            else
                forward(vtp, STATE_CSI_IGNORE);
        } break;
        case STATE_CSI_IGNORE: {
            if (IS_CTRL(input))
                forward(vtp, STATE_GROUND);
            else if (IS_CSI_FINAL(input))
                transition(vtp, STATE_GROUND);
        } break;
        case STATE_OSC_STRING: {
            switch (input) {
            case 0x9c: // fallthrough.
            case 0x07: dispatch(vtp, EVENT_OSC); break;
            case 0x27: transition(vtp, STATE_OSC_ST); break;
            default: {
                if (IS_PRINTABLE(input))
                    collect(vtp, input);
                else
                    forward(vtp, STATE_GROUND);
            } break;
            }
        } break;
        case STATE_OSC_ST: {
            switch (input) {
            case '\\': dispatch(vtp, EVENT_OSC); break;
            default: forward(vtp, STATE_ESC); break;
            }
        } break;
        }
    }

    return vtp->fsm.event;
}

static inline void collect(VT_Parser *vtp, uchar input)
{
    switch (vtp->fsm.state) {
    case STATE_CSI_PARAM:  // fallthrough.
    case STATE_ESC_INTERM: // fallthrough.
    case STATE_CSI_INTERM: // fallthrough.
    case STATE_OSC_STRING: {
        vtp->seq[vtp->nseq++] = input;
    } break;
    case STATE_ESC_FINAL: {
        vtp->payload.esc.final_byte = input;
        dispatch(vtp, EVENT_ESC);
    } break;
    case STATE_CSI_FINAL: {
        vtp->payload.csi.final_byte = input;
        dispatch(vtp, EVENT_CSI);
    } break;
    default: break;
    }
}

static inline void forward(VT_Parser *vtp, FSM_State state)
{
    r_rollback(&vtp->reader);
    transition(vtp, state);
}

static inline void transition(VT_Parser *vtp, FSM_State next_state)
{
#if defined(_DEBUG__VTE_PARSER_)
    // {{{
    printf("Tranistion { ");
#define FROM_REPR(sym)                                                         \
    case sym: printf(#sym " -> "); break;
    switch (vtp->fsm.state) {
        FROM_REPR(STATE_GROUND);
        FROM_REPR(STATE_UTF8_DECODE);
        FROM_REPR(STATE_ESC);
        FROM_REPR(STATE_ESC_INTERM);
        FROM_REPR(STATE_ESC_FINAL);
        FROM_REPR(STATE_CSI_PARAM);
        FROM_REPR(STATE_CSI_INTERM);
        FROM_REPR(STATE_CSI_FINAL);
        FROM_REPR(STATE_CSI_IGNORE);
        FROM_REPR(STATE_OSC_STRING);
        FROM_REPR(STATE_OSC_ST);
    }
#undef FROM_REPR
#define TO_REPR(sym)                                                           \
    case sym: printf(#sym); break;
    switch (next_state) {
        TO_REPR(STATE_GROUND);
        TO_REPR(STATE_UTF8_DECODE);
        TO_REPR(STATE_ESC);
        TO_REPR(STATE_ESC_INTERM);
        TO_REPR(STATE_ESC_FINAL);
        TO_REPR(STATE_CSI_PARAM);
        TO_REPR(STATE_CSI_INTERM);
        TO_REPR(STATE_CSI_FINAL);
        TO_REPR(STATE_CSI_IGNORE);
        TO_REPR(STATE_OSC_STRING);
        TO_REPR(STATE_OSC_ST);
    }
#undef TO_REPR
    printf(" }\n");
    // }}}
#endif

    switch (vtp->fsm.state) { // on Exit.
    case STATE_ESC_INTERM: {
        vtp->payload.esc.interm  = vtp->seq;
        vtp->payload.esc.ninterm = vtp->nseq;
    } break;
    case STATE_CSI_INTERM: {
        vtp->payload.csi.ninterm = vtp->nseq - vtp->payload.csi.ninterm;
    } break;
    default: break;
    }

    switch (vtp->fsm.state = next_state) { // on Enter.
    case STATE_GROUND: {
        memset(&vtp->payload, 0, sizeof(vtp->payload));
        vtp->fsm.event = EVENT_NOOP;
    } break;
    case STATE_ESC: {
        memset(vtp->seq, vtp->nseq = 0, sizeof(vtp->seq));
    } break;
    case STATE_CSI_INTERM: {
        vtp->payload.csi.interm = vtp->seq + vtp->nseq;
        // for calculating intermediate bytes count on csi-interm state exit.
        vtp->payload.csi.ninterm = vtp->nseq;
    } break;
    default: break;
    }
}

static inline void dispatch(VT_Parser *vtp, FSM_Event event)
{
    switch (vtp->fsm.event = event) {
    case EVENT_NOOP: break;
    case EVENT_PRINT: {
        vtp->payload.value = vtp->utf8_decoder.rune;
    } break;
    case EVENT_CTRL: {
        prepare_ctrl_payload(vtp, &vtp->payload.ctrl);
    } break;
    case EVENT_ESC: {
        prepare_esc_payload(vtp, &vtp->payload.esc);
    } break;
    case EVENT_CSI: {
        prepare_csi_payload(vtp, &vtp->payload.csi);
    } break;
    case EVENT_OSC: {
        prepare_osc_payload(vtp, &vtp->payload.osc);
    } break;
    }

#if defined(_DEBUG__VTE_PARSER_)
    // {{{
    if (true) {
        printf("EMIT { ");
        switch (vtp->fsm.event) {
#define CASE_REPR(sym)                                                         \
    case sym: printf("[" #sym "]"); break
        case EVENT_NOOP: {
            printf("[NOOP]");
            if (vtp->nseq) {
                printf(": '%s'", vtp->seq);
                fprintf(stderr, "[NOOP]: '%s'\n", vtp->seq);
            }
        } break;
        case EVENT_PRINT: {
            printf("[PRINT]");
            printf(BETWEEN(vtp->payload.value, 32, 126) ? ": '%c'" : ": %d",
                   vtp->payload.value);
        } break;
        case EVENT_CTRL: {
            CTRL_Payload *ctrl = &vtp->payload.ctrl;
            switch (ctrl->action) {
                CASE_REPR(C0_BEL);
                CASE_REPR(C0_BS);
                CASE_REPR(C0_HT);
                CASE_REPR(C0_LF);
                CASE_REPR(C0_VT);
                CASE_REPR(C0_FF);
                CASE_REPR(C0_CR);
                CASE_REPR(C0_SO);
                CASE_REPR(C0_SI);
            }
        } break;
        case EVENT_ESC: {
            ESC_Payload *esc = &vtp->payload.esc;
            switch (vtp->payload.esc.action) {
                CASE_REPR(ESC_IND);
                CASE_REPR(ESC_RI);
                CASE_REPR(ESC_HTS);
                CASE_REPR(ESC_CS_LINEGFX);
                CASE_REPR(ESC_CS_USASCII);
                CASE_REPR(ESC_DECSC);
                CASE_REPR(ESC_DECRC);
                CASE_REPR(ESC_UNKNOWN);
            }
            printf(": '%s'", esc->interm);
            if (esc->action == ESC_UNKNOWN) {
                fprintf(stderr, "ESC%s%c\n", esc->interm, esc->final_byte);
                printf(" ESC%s%c.\n", esc->interm, esc->final_byte);
            }
        } break;
        case EVENT_CSI: {
            CSI_Payload *csi = &vtp->payload.csi;
            switch (csi->action) {
                CASE_REPR(CSI_CUU);
                CASE_REPR(CSI_CUD);
                CASE_REPR(CSI_CUF);
                CASE_REPR(CSI_CUB);
                CASE_REPR(CSI_CNL);
                CASE_REPR(CSI_CPL);
                CASE_REPR(CSI_CHA);
                CASE_REPR(CSI_CUP);
                CASE_REPR(CSI_TBC);
                CASE_REPR(CSI_CHT);
                CASE_REPR(CSI_CBT);
                CASE_REPR(CSI_ED);
                CASE_REPR(CSI_EL);
                CASE_REPR(CSI_IL);
                CASE_REPR(CSI_DL);
                CASE_REPR(CSI_ICH);
                CASE_REPR(CSI_DCH);
                CASE_REPR(CSI_ECH);
                CASE_REPR(CSI_SU);
                CASE_REPR(CSI_SD);
                CASE_REPR(CSI_HVP);
                CASE_REPR(CSI_VPA);
                CASE_REPR(CSI_SGR);
                CASE_REPR(CSI_SC);
                CASE_REPR(CSI_RC);
                CASE_REPR(CSI_DECSTBM);
                CASE_REPR(CSI_DECSET);
                CASE_REPR(CSI_DECRST);
                CASE_REPR(CSI_UNKNOWN);
            }
            if (csi->nparam)
                printf(": %d", csi->param[0]);
            for (int i = 1; i < csi->nparam; ++i)
                printf(" %d", csi->param[i]);
            if (csi->ninterm)
                printf(" ([%d]: %s)", csi->ninterm, csi->interm);
            if (csi->action == CSI_UNKNOWN) {
                fprintf(stderr, "ESC[%s%c\n", vtp->seq, csi->final_byte);
                printf(" ESC[%s%c", vtp->seq, csi->final_byte);
            }
        } break;
        case EVENT_OSC: {
            printf("OSC Event.\n");
        } break;
#undef CASE_REPR
        }
        printf(" }\n");
    }
    fflush(stdout);
    // }}}
#endif

    vtp->fsm.dispatching = true;
}

static inline void prepare_ctrl_payload(VT_Parser *vtp, CTRL_Payload *ctrl)
{
    Reader *r = &vtp->reader;
    switch (r_rollback(r), ctrl->value = r_next(r)) {
    case 0x07: { ctrl->action = C0_BEL; } break;
    case 0x08: { ctrl->action = C0_BS;  } break;
    case 0x09: { ctrl->action = C0_HT;  } break;
    case 0x0a: { ctrl->action = C0_LF;  } break;
    case 0x0b: { ctrl->action = C0_VT;  } break;
    case 0x0c: { ctrl->action = C0_FF;  } break;
    case 0x0d: { ctrl->action = C0_CR;  } break;
    case 0x0e: { ctrl->action = C0_SO;  } break;
    case 0x0f: { ctrl->action = C0_SI;  } break;
    default:   { vtp->fsm.event = EVENT_NOOP; } break;
    }
}

static inline void prepare_esc_payload(VT_Parser *vtp, ESC_Payload *esc)
{
    switch (esc->action = ESC_UNKNOWN, esc->final_byte) {
    case 'D': { esc->action = ESC_IND; goto ENSURE_EMPTY_INTERM; }
    case 'M': { esc->action = ESC_RI;  goto ENSURE_EMPTY_INTERM; }
    case 'H': { esc->action = ESC_HTS; goto ENSURE_EMPTY_INTERM; }
    // ESC C.
ENSURE_EMPTY_INTERM: {
        if (vtp->nseq) {
            esc->action = ESC_UNKNOWN; goto DONE;
        }
    } break;

    case '0': { esc->action = ESC_CS_LINEGFX; goto ENSURE_CHARSET_INDEX; }
    case 'B': { esc->action = ESC_CS_USASCII; goto ENSURE_CHARSET_INDEX; }
    // ESC [()*+] C (ensure index to designate the character set).
ENSURE_CHARSET_INDEX: {
        if (!(vtp->nseq == 1 && BETWEEN(vtp->seq[0], '(', '+'))) {
            esc->action = ESC_UNKNOWN; goto DONE;
        }
    } break;

    case '7': { esc->action = ESC_DECSC;   goto DONE; }
    case '8': { esc->action = ESC_DECRC;   goto DONE; }
    default:  { esc->action = ESC_UNKNOWN; goto DONE; }
    }
DONE:
    return;
}

static inline void prepare_csi_payload(VT_Parser *vtp, CSI_Payload *csi)
{
    memset(csi->param, csi->nparam = 0, sizeof(csi->param));

    Reader param_r = READER(vtp->seq, vtp->nseq - csi->ninterm);
    switch (csi->action = CSI_UNKNOWN, csi->final_byte) {
    case 'A': { csi->action = CSI_CUU; goto ENSURE_SINGLE_PARAM; }
    case 'B': { csi->action = CSI_CUD; goto ENSURE_SINGLE_PARAM; }
    case 'C': { csi->action = CSI_CUF; goto ENSURE_SINGLE_PARAM; }
    case 'D': { csi->action = CSI_CUB; goto ENSURE_SINGLE_PARAM; }
    case 'd': { csi->action = CSI_VPA; goto ENSURE_SINGLE_PARAM; }
    case 'E': { csi->action = CSI_CNL; goto ENSURE_SINGLE_PARAM; }
    case 'F': { csi->action = CSI_CPL; goto ENSURE_SINGLE_PARAM; }
    case 'G': { csi->action = CSI_CHA; goto ENSURE_SINGLE_PARAM; }
    case 'g': { csi->action = CSI_TBC; goto ENSURE_SINGLE_PARAM; }
    case 'I': { csi->action = CSI_CHT; goto ENSURE_SINGLE_PARAM; }
    case 'Z': { csi->action = CSI_CBT; goto ENSURE_SINGLE_PARAM; }
    case 'J': { csi->action = CSI_ED;  goto ENSURE_SINGLE_PARAM; }
    case 'K': { csi->action = CSI_EL;  goto ENSURE_SINGLE_PARAM; }
    case 'S': { csi->action = CSI_SU;  goto ENSURE_SINGLE_PARAM; }
    case 'T': { csi->action = CSI_SD;  goto ENSURE_SINGLE_PARAM; }
    case 'L': { csi->action = CSI_IL;  goto ENSURE_SINGLE_PARAM; }
    case 'M': { csi->action = CSI_DL;  goto ENSURE_SINGLE_PARAM; }
    case '@': { csi->action = CSI_ICH; goto ENSURE_SINGLE_PARAM; }
    case 'P': { csi->action = CSI_DCH; goto ENSURE_SINGLE_PARAM; }
    case 'X': { csi->action = CSI_ECH; goto ENSURE_SINGLE_PARAM; }
    // CSI Ps C (force single param, default: 0).
ENSURE_SINGLE_PARAM: {
        csi->param[csi->nparam++] = r_consume_number(&param_r);
    } break;

    case 'H': { csi->action = CSI_CUP;     goto ENSURE_DOUBLE_PARAM; }
    case 'f': { csi->action = CSI_HVP;     goto ENSURE_DOUBLE_PARAM; }
    case 'r': { csi->action = CSI_DECSTBM; goto ENSURE_DOUBLE_PARAM; }
    // CSI Ps ; Ps C (force two delimited params, default: {0, 0}).
ENSURE_DOUBLE_PARAM: {
        csi->param[csi->nparam++] = r_consume_number(&param_r);
        (void)r_consume_param_delim(&param_r);
        csi->param[csi->nparam++] = r_consume_number(&param_r);
    } break;

    case 'h': { csi->action = CSI_DECSET; goto CHECK_PRIVATE_MODE; }
    case 'l': { csi->action = CSI_DECRST; goto CHECK_PRIVATE_MODE; }
    // CSI ? Pm C (check for private marker, e.g: '?').
CHECK_PRIVATE_MODE: {
        if (r_consume(&param_r, '?')) {
            goto ENSURE_MULTIPLE_PARAM;
        } else {
            csi->action = CSI_UNKNOWN; goto DONE;
        }
    } break;

    case 'm': { csi->action = CSI_SGR; goto ENSURE_MULTIPLE_PARAM; }
    // CSI Ps ; Pm C (delimited params).
ENSURE_MULTIPLE_PARAM: {
        do {
            csi->param[csi->nparam++] = r_consume_number(&param_r);
        } while (r_consume_param_delim(&param_r));
    } break;

    case 's': { csi->action = CSI_SC;      goto DONE; }
    case 'u': { csi->action = CSI_RC;      goto DONE; }
    default:  { csi->action = CSI_UNKNOWN; goto DONE; }
    }

DONE:
    if (r_buflen(&param_r)) // extra unparsed seq.
        csi->action = CSI_UNKNOWN;
}

static inline void prepare_osc_payload(VT_Parser *vtp, OSC_Payload *osc)
{
    (void)vtp;
    (void)osc;
    /* unhandled. */
}
