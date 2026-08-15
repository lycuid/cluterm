// vim:fdm=marker
#include "parser.h"
#include <cluterm/debug.h>
#include <cluterm/utf8.h>
#include <cluterm/utils.h>
#include <stdbool.h>
#include <string.h>
// clang-format off

#define IS_C0(ch)   ((ch) <= 0x1f || (ch) == 0x7f)
#define IS_C1(ch)   BETWEEN(ch, 0x80, 0x9f)
#define IS_CTRL(ch) (IS_C0(ch) || IS_C1(ch))

#define IS_CSI_PARAM(ch) BETWEEN(ch, 0x30, 0x3f)
#define IS_INTERM(ch)    BETWEEN(ch, 0x20, 0x2f)
#define IS_ESC_FINAL(ch) BETWEEN(ch, 0x30, 0x7e)
#define IS_CSI_FINAL(ch) BETWEEN(ch, 0x40, 0x7e)
#define IS_PRINTABLE(ch) BETWEEN(ch, 0x20, 0x7f)

#define s_consume_param_delim(p) (s_consume((p), ';') || s_consume((p), ':'))

static inline void collect(VT_Parser *, uchar);
static inline void replay(VT_Parser *, FSM_State);
static inline void transition(VT_Parser *, FSM_State);
static inline void dispatch(VT_Parser *, FSM_Event);

static inline void prepare_ctrl_payload(VT_Parser *, CTRL_Payload *);
static inline void prepare_esc_payload(VT_Parser *, ESC_Payload *);
static inline void prepare_csi_payload(VT_Parser *, CSI_Payload *);
static inline void prepare_osc_payload(VT_Parser *, OSC_Payload *);

void parser_init(VT_Parser *vtp) { transition(vtp, STATE_GROUND); }

void parser_feed(VT_Parser *vtp, const uchar *stream, uint32_t slen)
{
    vtp->scanner = SCANNER(stream, slen);
}

FSM_Event parser_run(VT_Parser *vtp)
{
    if (vtp->fsm.dispatching)
        transition(vtp, STATE_GROUND);
    vtp->fsm.dispatching = false;

    for (Scanner *s = &vtp->scanner;
         !vtp->fsm.dispatching && s_peek(s) != NULL;) {
        uchar input = s_next(s);

        switch (vtp->fsm.state) {
        case STATE_GROUND: {
            switch (input) {
            case 0x1b: transition(vtp, STATE_ESC);        break;
            case 0x9b: transition(vtp, STATE_CSI_PARAM);  break;
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
            case '[': transition(vtp, STATE_CSI_PARAM);  break;
            case ']': transition(vtp, STATE_OSC_STRING); break;
            default:  replay(vtp, STATE_ESC_INTERM);     break;
            }
        } break;
        case STATE_ESC_INTERM: {
            if (IS_INTERM(input))
                collect(vtp, input);
            else
                replay(vtp, STATE_ESC_FINAL);
        } break;
        case STATE_ESC_FINAL: {
            if (IS_ESC_FINAL(input))
                collect(vtp, input);
            else
                replay(vtp, STATE_GROUND);
        } break;
        case STATE_CSI_PARAM: {
            if (IS_CSI_PARAM(input))
                collect(vtp, input);
            else
                replay(vtp, STATE_CSI_INTERM);
        } break;
        case STATE_CSI_INTERM: {
            if (IS_INTERM(input))
                collect(vtp, input);
            else
                replay(vtp, STATE_CSI_FINAL);
        } break;
        case STATE_CSI_FINAL: {
            if (IS_CSI_FINAL(input))
                collect(vtp, input);
            else
                replay(vtp, STATE_CSI_IGNORE);
        } break;
        case STATE_CSI_IGNORE: {
            if (IS_CTRL(input))
                replay(vtp, STATE_GROUND);
            else if (IS_CSI_FINAL(input))
                transition(vtp, STATE_GROUND);
        } break;
        case STATE_OSC_STRING: {
            switch (input) {
            case C0_BEL: // fallthrough.
            case 0x9c:   dispatch(vtp, EVENT_OSC);      break;
            case 0x1b:   transition(vtp, STATE_OSC_ST); break;
            default: {
                if (IS_PRINTABLE(input))
                    collect(vtp, input);
                else
                    replay(vtp, STATE_GROUND);
            } break;
            }
        } break;
        case STATE_OSC_ST: {
            switch (input) {
            case '\\': dispatch(vtp, EVENT_OSC); break;
            default:   replay(vtp, STATE_ESC);   break;
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
        /* if (vtp->nseq < sizeof(vtp->seq)) */
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

static inline void replay(VT_Parser *vtp, FSM_State state)
{
    s_rollback(&vtp->scanner);
    transition(vtp, state);
}

static inline void transition(VT_Parser *vtp, FSM_State next_state)
{
#if DEBUG_LVL >= 6
    // {{{
    debug_2("Transition { ");
#define FROM_REPR(sym)                                                         \
    case sym: debug(#sym " -> "); break;
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
    case sym: debug(#sym); break;
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
    debug(" }\n");
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

#if DEBUG_LVL >= 2
    // {{{
    debug_2("Dispatch { ");
    switch (vtp->fsm.event) {
#define CASE_REPR(sym)                                                         \
    case sym: debug("[" #sym "]"); break
    case EVENT_NOOP: {
        debug("[NOOP]");
        if (vtp->nseq) {
            debug(": '%s' (%ld)", vtp->seq, vtp->nseq);
        }
    } break;
    case EVENT_PRINT: {
        debug("[PRINT]");
        debug(BETWEEN(vtp->payload.value, 32, 126) ? ": '%c'" : ": %d",
              vtp->payload.value);
    } break;
    case EVENT_CTRL: {
        CTRL_Payload *ctrl = &vtp->payload.ctrl;
        switch (ctrl->action) {
            CASE_REPR(C0_NOOP);
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
        debug(": '%s'", esc->interm);
        if (esc->action == ESC_UNKNOWN) {
            fprintf(stderr, "ESC%s%c\n", esc->interm, esc->final_byte);
            debug(" ESC%s%c.\n", esc->interm, esc->final_byte);
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
            debug(": %d", csi->param[0]);
        for (int i = 1; i < csi->nparam; ++i)
            debug(" %d", csi->param[i]);
        if (csi->ninterm)
            debug(" ([%d]: %s)", csi->ninterm, csi->interm);
        if (csi->action == CSI_UNKNOWN) {
            fprintf(stderr, "ESC[%s%c\n", vtp->seq, csi->final_byte);
            debug(" ESC[%s%c", vtp->seq, csi->final_byte);
        }
    } break;
    case EVENT_OSC: {
        debug("[OSC]: '%s'", vtp->seq);
    } break;
#undef CASE_REPR
    }
    debug(" }\n");
    fflush(stdout);
    // }}}
#endif

    vtp->fsm.dispatching = true;
}

static inline void prepare_ctrl_payload(VT_Parser *vtp, CTRL_Payload *ctrl)
{
    Scanner *s = &vtp->scanner;
    switch (s_rollback(s), ctrl->action = s_next(s)) {
    case C0_BEL: // fallthrough
    case C0_BS:  // fallthrough
    case C0_HT:  // fallthrough
    case C0_LF:  // fallthrough
    case C0_VT:  // fallthrough
    case C0_FF:  // fallthrough
    case C0_CR:  // fallthrough
    case C0_SO:  // fallthrough
    case C0_SI:  break;
    default:     { ctrl->action = C0_NOOP; } break;
    }
}

static inline void prepare_esc_payload(VT_Parser *vtp, ESC_Payload *esc)
{
    switch (esc->action = ESC_UNKNOWN, esc->final_byte) {
    case 'D': { esc->action = ESC_IND; goto ensure_empty_interm; }
    case 'M': { esc->action = ESC_RI;  goto ensure_empty_interm; }
    case 'H': { esc->action = ESC_HTS; goto ensure_empty_interm; }
    // ESC C.
ensure_empty_interm: {
        if (vtp->nseq) {
            esc->action = ESC_UNKNOWN; goto done;
        }
    } break;

    case '0': { esc->action = ESC_CS_LINEGFX; goto ensure_charset_index; }
    case 'B': { esc->action = ESC_CS_USASCII; goto ensure_charset_index; }
    // ESC [()*+] C (ensure index to designate the character set).
ensure_charset_index: {
        if (!(vtp->nseq == 1 && BETWEEN(vtp->seq[0], '(', '+'))) {
            esc->action = ESC_UNKNOWN; goto done;
        }
    } break;

    case '7': { esc->action = ESC_DECSC;   goto done; }
    case '8': { esc->action = ESC_DECRC;   goto done; }
    default:  { esc->action = ESC_UNKNOWN; goto done; }
    }
done:;
}

static inline void prepare_csi_payload(VT_Parser *vtp, CSI_Payload *csi)
{
    memset(csi->param, csi->nparam = 0, sizeof(csi->param));

    Scanner param_s = SCANNER(vtp->seq, vtp->nseq - csi->ninterm);
    switch (csi->action = CSI_UNKNOWN, csi->final_byte) {
    case 'A': { csi->action = CSI_CUU; goto ensure_single_param; }
    case 'B': { csi->action = CSI_CUD; goto ensure_single_param; }
    case 'C': { csi->action = CSI_CUF; goto ensure_single_param; }
    case 'D': { csi->action = CSI_CUB; goto ensure_single_param; }
    case 'd': { csi->action = CSI_VPA; goto ensure_single_param; }
    case 'E': { csi->action = CSI_CNL; goto ensure_single_param; }
    case 'F': { csi->action = CSI_CPL; goto ensure_single_param; }
    case 'G': { csi->action = CSI_CHA; goto ensure_single_param; }
    case 'g': { csi->action = CSI_TBC; goto ensure_single_param; }
    case 'I': { csi->action = CSI_CHT; goto ensure_single_param; }
    case 'Z': { csi->action = CSI_CBT; goto ensure_single_param; }
    case 'J': { csi->action = CSI_ED;  goto ensure_single_param; }
    case 'K': { csi->action = CSI_EL;  goto ensure_single_param; }
    case 'S': { csi->action = CSI_SU;  goto ensure_single_param; }
    case 'T': { csi->action = CSI_SD;  goto ensure_single_param; }
    case 'L': { csi->action = CSI_IL;  goto ensure_single_param; }
    case 'M': { csi->action = CSI_DL;  goto ensure_single_param; }
    case '@': { csi->action = CSI_ICH; goto ensure_single_param; }
    case 'P': { csi->action = CSI_DCH; goto ensure_single_param; }
    case 'X': { csi->action = CSI_ECH; goto ensure_single_param; }
    // CSI Ps C (force single param, default: 0).
ensure_single_param: {
        csi->param[csi->nparam++] = s_consume_number(&param_s);
    } break;

    case 'H': { csi->action = CSI_CUP;     goto ensure_double_param; }
    case 'f': { csi->action = CSI_HVP;     goto ensure_double_param; }
    case 'r': { csi->action = CSI_DECSTBM; goto ensure_double_param; }
    // CSI Ps ; Ps C (force two delimited params, default: {0, 0}).
ensure_double_param: {
        csi->param[csi->nparam++] = s_consume_number(&param_s);
        (void)s_consume_param_delim(&param_s);
        csi->param[csi->nparam++] = s_consume_number(&param_s);
    } break;

    case 'h': { csi->action = CSI_DECSET; goto check_private_mode; }
    case 'l': { csi->action = CSI_DECRST; goto check_private_mode; }
    // CSI ? Pm C (check for private marker, e.g: '?').
check_private_mode: {
        if (s_consume(&param_s, '?')) {
            goto ensure_multiple_param;
        } else {
            csi->action = CSI_UNKNOWN; goto done;
        }
    } break;

    case 'm': { csi->action = CSI_SGR; goto ensure_multiple_param; }
    // CSI Ps ; Pm C (delimited params).
    ensure_multiple_param: {
        do {
            csi->param[csi->nparam++] = s_consume_number(&param_s);
        } while (s_consume_param_delim(&param_s));
    } break;

    case 's': { csi->action = CSI_SC;      goto done; }
    case 'u': { csi->action = CSI_RC;      goto done; }
    default:  { csi->action = CSI_UNKNOWN; goto done; }
    }

done:
    if (s_buflen(&param_s)) // extra unparsed seq.
        csi->action = CSI_UNKNOWN;
}

static inline void prepare_osc_payload(VT_Parser *vtp, OSC_Payload *osc)
{
    osc->action  = OSC_UNKNOWN;
    osc->scanner = SCANNER(vtp->seq, vtp->nseq);
    const uchar *ch = s_peek(&osc->scanner);
    if (!ch || !BETWEEN(*ch, '0', '9'))
        return;

    int action = s_consume_number(&osc->scanner);
    switch (action) {
    case OSC_0:  // fallthrough
    case OSC_2:  // fallthrough
    case OSC_7:  // fallthrough
    case OSC_10: // fallthrough
    case OSC_11: // fallthrough
    case OSC_12: {
        if (!s_consume(&osc->scanner, ';'))
            osc->action = OSC_UNKNOWN;
    } break;
    default: break;
    }
}
