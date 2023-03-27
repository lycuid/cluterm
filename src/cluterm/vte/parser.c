// vim:fdm=marker
#include "parser.h"
#include <cluterm/utf8.h>
#include <cluterm/utils.h>
#include <stdbool.h>
// clang-format off

typedef unsigned char uchar;

#define parser_unlock(vtp)                                                     \
    if ((vtp)->fsm.state == STATE_DISPATCH)                                    \
        transition(vtp, STATE_GROUND);

#define IS_C0(ch)   ((ch) <= 0x1f || (ch) == 0x7f)
#define IS_C1(ch)   BETWEEN(ch, 0x80, 0x9f)
#define IS_CTRL(ch) (IS_C0(ch) || IS_C1(ch))

#define IS_CSI_PARAM(ch) BETWEEN(ch, 0x30, 0x3f)
#define IS_INTERM(ch)    BETWEEN(ch, 0x20, 0x2f)
#define IS_ESC_FINAL(ch) BETWEEN(ch, 0x30, 0x7e)
#define IS_CSI_FINAL(ch) BETWEEN(ch, 0x40, 0x7e)
#define IS_PRINTABLE(ch) BETWEEN(ch, 0x20, 0x7f)

#define p_buffer(p)          ((p)->buffer + (p)->cursor)
#define p_buflen(p)          ((p)->size - (p)->cursor)
#define p_peek(p)            ((p)->cursor < (p)->size ? &(p)->buffer[(p)->cursor] : NULL)
#define p_next(p)            ((p)->buffer[(p)->cursor++])
#define p_advance(p)         (p_advance_by(p, 1))
#define p_advance_by(p, inc) ((p)->cursor += inc)
#define p_rollback(p)        (--(p)->cursor)
#define p_consume(p, ch)     ((p)->buffer[(p)->cursor] == ch && p_advance(p) > 0)

#define p_consume_param_delim(p) (p_consume((p), ';') || p_consume((p), ':'))

static inline int p_consume_number(Parser *p)
{
    int n = 0;
    for (const char *ch = p_peek(p); ch && BETWEEN(*ch, '0', '9');
         ch             = p_peek(p))
        n = n * 10 + p_next(p) - '0';
    return n;
}

static inline void collect(VT_Parser *, uchar);
static inline void forward(VT_Parser *, FSM_State);
static inline void transition(VT_Parser *, FSM_State);
static inline void dispatch(VT_Parser *, FSM_Event);

static inline void prepare_ctrl(VT_Parser *, CTRL_Payload *);
static inline void prepare_esc(VT_Parser *, ESC_Payload *);
static inline void prepare_csi(VT_Parser *, CSI_Payload *);
static inline void prepare_osc(VT_Parser *, OSC_Payload *);

#define Action(e, _action, label) { (e)->action = _action; goto label; }

void parser_init(VT_Parser *vtp) { transition(vtp, STATE_GROUND); }

void parser_feed(VT_Parser *vtp, const char *stream, uint32_t slen)
{
    vtp->inner = PARSER(stream, slen);
}

FSM_Event parser_run(VT_Parser *vtp)
{
    parser_unlock(vtp);
    for (Parser *p = &vtp->inner; p_peek(p);) {
        uchar input = p_next(p);
        switch (vtp->fsm.state) {
        case STATE_DISPATCH: {
            p_rollback(p);
            goto DISPATCH;
        }
        case STATE_GROUND: {
            switch (input) {
            case 0x1b: transition(vtp, STATE_ESC);          break;
            case 0x9b: transition(vtp, STATE_CSI_PARAM);    break;
            case 0x9d: transition(vtp, STATE_OSC_STRING);   break;
            default: {
                if (IS_CTRL(input))
                    dispatch(vtp, EVENT_CTRL);
                else dispatch(vtp, EVENT_PRINT);
            } break;
            }
        } break;
        case STATE_ESC: {
            switch (input) {
            case '[': transition(vtp, STATE_CSI_PARAM);     break;
            case ']': transition(vtp, STATE_OSC_STRING);    break;
            default:  forward(vtp, STATE_ESC_INTERM);       break;
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
            case 0x07: dispatch(vtp, EVENT_OSC);        break;
            case 0x27: transition(vtp, STATE_OSC_ST);   break;
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
            case '\\':  dispatch(vtp, EVENT_OSC);   break;
            default:    forward(vtp, STATE_ESC);    break;
            }
        } break;
        }
    }
DISPATCH:
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
    p_rollback(&vtp->inner);
    transition(vtp, state);
}

static inline void transition(VT_Parser *vtp, FSM_State next_state)
{
#if 0
    // {{{
    printf("Tranistion { ");
#define FROM_REPR(sym) case sym: printf(#sym " -> "); break;
    switch (vtp->fsm.state) {
        FROM_REPR(STATE_DISPATCH);
        FROM_REPR(STATE_GROUND);
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
#define TO_REPR(sym) case sym: printf(#sym); break;
    switch (next_state) {
        TO_REPR(STATE_DISPATCH);
        TO_REPR(STATE_GROUND);
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
        p_rollback(&vtp->inner);
        uint32_t len = utf8_decode(p_buffer(&vtp->inner), p_buflen(&vtp->inner),
                                   &vtp->payload.value);
        if (!len) {                                    // invalid or incomplete.
            if (p_buflen(&vtp->inner) >= UTF8_MAX_LEN) // invalid.
                len++;
            vtp->fsm.event = EVENT_NOOP;
        }
        p_advance_by(&vtp->inner, len);
    } break;
    case EVENT_CTRL: prepare_ctrl(vtp, &vtp->payload.ctrl); break;
    case EVENT_ESC:  prepare_esc(vtp, &vtp->payload.esc);   break;
    case EVENT_CSI:  prepare_csi(vtp, &vtp->payload.csi);   break;
    case EVENT_OSC:  prepare_osc(vtp, &vtp->payload.osc);   break;
    }

#if 0
    // {{{
    if (true) {
        printf("Dispatch { ");
        switch (vtp->fsm.event) {
#define CASE_REPR(sym)                                                         \
    case sym:                                                                  \
        printf("[" #sym "]");                                                  \
        break
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

    transition(vtp, STATE_DISPATCH);
}

static inline void prepare_ctrl(VT_Parser *vtp, CTRL_Payload *ctrl)
{
    Parser *p = &vtp->inner;
    switch (p_rollback(p), ctrl->value = p_next(p)) {
    case 0x07: ctrl->action = C0_BEL;       break;
    case 0x08: ctrl->action = C0_BS;        break;
    case 0x09: ctrl->action = C0_HT;        break;
    case 0x0a: ctrl->action = C0_LF;        break;
    case 0x0b: ctrl->action = C0_VT;        break;
    case 0x0c: ctrl->action = C0_FF;        break;
    case 0x0d: ctrl->action = C0_CR;        break;
    case 0x0e: ctrl->action = C0_SO;        break;
    case 0x0f: ctrl->action = C0_SI;        break;
    default:   vtp->fsm.event = EVENT_NOOP; break;
    }
}

static inline void prepare_esc(VT_Parser *vtp, ESC_Payload *esc)
{
    switch (esc->action = ESC_UNKNOWN, esc->final_byte) {
    case 'D': Action(esc, ESC_IND, ENSURE_EMPTY_INTERM);
    case 'M': Action(esc, ESC_RI,  ENSURE_EMPTY_INTERM);
    case 'H': Action(esc, ESC_HTS, ENSURE_EMPTY_INTERM);
    // ESC C.
ENSURE_EMPTY_INTERM: {
        if (vtp->nseq)
            Action(esc, ESC_UNKNOWN, DONE);
    } break;

    case '0': Action(esc, ESC_CS_LINEGFX, ENSURE_CHARSET_INDEX);
    case 'B': Action(esc, ESC_CS_USASCII, ENSURE_CHARSET_INDEX);
    // ESC [()*+] C (ensure index to designate the character set).
ENSURE_CHARSET_INDEX: {
        if (!(vtp->nseq == 1 && BETWEEN(vtp->seq[0], '(', '+')))
            Action(esc, ESC_UNKNOWN, DONE);
    } break;

    case '7': Action(esc, ESC_DECSC,   DONE);
    case '8': Action(esc, ESC_DECRC,   DONE);
    default:  Action(esc, ESC_UNKNOWN, DONE);
    }
DONE:
    return;
}

static inline void prepare_csi(VT_Parser *vtp, CSI_Payload *csi)
{
    memset(csi->param, csi->nparam = 0, sizeof(csi->param));

    Parser param_p = PARSER(vtp->seq, vtp->nseq - csi->ninterm);
    switch (csi->action = CSI_UNKNOWN, csi->final_byte) {
    case 'A': Action(csi, CSI_CUU, ENSURE_SINGLE_PARAM);
    case 'B': Action(csi, CSI_CUD, ENSURE_SINGLE_PARAM);
    case 'C': Action(csi, CSI_CUF, ENSURE_SINGLE_PARAM);
    case 'D': Action(csi, CSI_CUB, ENSURE_SINGLE_PARAM);
    case 'd': Action(csi, CSI_VPA, ENSURE_SINGLE_PARAM);
    case 'E': Action(csi, CSI_CNL, ENSURE_SINGLE_PARAM);
    case 'F': Action(csi, CSI_CPL, ENSURE_SINGLE_PARAM);
    case 'G': Action(csi, CSI_CHA, ENSURE_SINGLE_PARAM);
    case 'g': Action(csi, CSI_TBC, ENSURE_SINGLE_PARAM);
    case 'I': Action(csi, CSI_CHT, ENSURE_SINGLE_PARAM);
    case 'Z': Action(csi, CSI_CBT, ENSURE_SINGLE_PARAM);
    case 'J': Action(csi, CSI_ED,  ENSURE_SINGLE_PARAM);
    case 'K': Action(csi, CSI_EL,  ENSURE_SINGLE_PARAM);
    case 'S': Action(csi, CSI_SU,  ENSURE_SINGLE_PARAM);
    case 'T': Action(csi, CSI_SD,  ENSURE_SINGLE_PARAM);
    case 'L': Action(csi, CSI_IL,  ENSURE_SINGLE_PARAM);
    case 'M': Action(csi, CSI_DL,  ENSURE_SINGLE_PARAM);
    case '@': Action(csi, CSI_ICH, ENSURE_SINGLE_PARAM);
    case 'P': Action(csi, CSI_DCH, ENSURE_SINGLE_PARAM);
    case 'X': Action(csi, CSI_ECH, ENSURE_SINGLE_PARAM);
    // CSI Ps C (force single param, default: 0).
ENSURE_SINGLE_PARAM: {
        csi->param[csi->nparam++] = p_consume_number(&param_p);
    } break;

    case 'H': Action(csi, CSI_CUP,     ENSURE_DOUBLE_PARAM);
    case 'f': Action(csi, CSI_HVP,     ENSURE_DOUBLE_PARAM);
    case 'r': Action(csi, CSI_DECSTBM, ENSURE_DOUBLE_PARAM);
    // CSI Ps ; Ps C (force two delimited params, default: {0, 0}).
ENSURE_DOUBLE_PARAM: {
        csi->param[csi->nparam++] = p_consume_number(&param_p);
        (void)p_consume_param_delim(&param_p);
        csi->param[csi->nparam++] = p_consume_number(&param_p);
    } break;

    case 'h': Action(csi, CSI_DECSET, CHECK_PRIVATE_MODE);
    case 'l': Action(csi, CSI_DECRST, CHECK_PRIVATE_MODE);
    // CSI ? Pm C (check for private marker, e.g: '?').
CHECK_PRIVATE_MODE: {
        if (p_consume(&param_p, '?'))
            goto ENSURE_MULTIPLE_PARAM;
        else
            Action(csi, CSI_UNKNOWN, DONE);
    } break;

    case 'm': Action(csi, CSI_SGR, ENSURE_MULTIPLE_PARAM);
    // CSI Ps ; Pm C (delimited params).
ENSURE_MULTIPLE_PARAM: {
        do {
            csi->param[csi->nparam++] = p_consume_number(&param_p);
        } while (p_consume_param_delim(&param_p));
    } break;

    case 's': Action(csi, CSI_SC,      DONE);
    case 'u': Action(csi, CSI_RC,      DONE);
    default:  Action(csi, CSI_UNKNOWN, DONE);
    }
DONE:
    if (p_buflen(&param_p)) // extra unparsed seq.
        csi->action = CSI_UNKNOWN;
}

static inline void prepare_osc(VT_Parser *vtp, OSC_Payload *osc)
{
    (void)vtp;
    (void)osc;
    /* unhandled. */
}
