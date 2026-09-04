// vim:fdm=marker
#include "parser.h"
#include <cluterm/debug.h>
#include <cluterm/utf8.h>
#include <cluterm/util.h>
#include <stdbool.h>
#include <string.h>

#define s_consume_param_delim(p) (s_consume((p), ';') || s_consume((p), ':'))

static inline FSM_State execute(VT_Parser *, uchar, FSM_Effect);
static inline void collect(VT_Parser *, uchar);
static inline void transition(VT_Parser *, FSM_State);
static inline void dispatch(VT_Parser *, FSM_Event);

static inline void prepare_esc_payload(VT_Parser *, ESC_Payload *);
static inline void prepare_csi_payload(VT_Parser *, CSI_Payload *);
static inline void prepare_osc_payload(VT_Parser *, OSC_Payload *);
static inline void prepare_dcs_payload(VT_Parser *, DCS_Payload *);

void parser_init(VT_Parser *vtp) { transition(vtp, STATE_GROUND); }

void parser_feed(VT_Parser *vtp, const uchar *stream, size_t slen)
{
    vtp->scanner = SCANNER(stream, slen);
}

FSM_Event parser_run(VT_Parser *vtp)
{
    if (vtp->fsm.dispatching) {
        memset(&vtp->payload, 0, sizeof(vtp->payload));
        vtp->fsm.event = EVENT_NOOP;
    }
    vtp->fsm.dispatching = false;
    FSM_State next_state;
    for (Scanner *s = &vtp->scanner;
         !vtp->fsm.dispatching && s_peek(s) != NULL;) {
        uchar input = s_next(s);

        FSM_Effect effect = transition_table[vtp->fsm.state][input];
        if (!effect)
            effect = default_transition[vtp->fsm.state];

        if (effect) {
            next_state = execute(vtp, input, effect);
            transition(vtp, next_state);
        }
    }

    return vtp->fsm.event;
}

static inline FSM_State execute(VT_Parser *vtp, uchar input, FSM_Effect effect)
{
    switch (fsm_action(effect)) {
    case FSM_UTF8_DECODE: {
        if (vtp->utf8_decoder.rune == 0) {
            if (!utf8decoder_check(&vtp->utf8_decoder, input))
                transition(vtp, STATE_GROUND);
        } else {
            if (vtp->utf8_decoder.need_input)
                utf8decoder_feed(&vtp->utf8_decoder, input);
        }
        if (!vtp->utf8_decoder.need_input) {
            dispatch(vtp, EVENT_PRINT);
            return STATE_GROUND;
        }
    } break;

    case FSM_DISPATCH_CTRL: {
        vtp->payload.ctrl.action = input;
        dispatch(vtp, EVENT_CTRL);
    } break;

    case FSM_DISPATCH_ESC: {
        vtp->payload.esc.interm     = vtp->seq;
        vtp->payload.esc.ninterm    = vtp->nseq;
        vtp->payload.esc.final_byte = input;
        dispatch(vtp, EVENT_ESC);
    } break;

    case FSM_DISPATCH_CSI: {
        vtp->payload.csi.final_byte = input;
        if (vtp->fsm.state == STATE_CSI_INTERM)
            vtp->payload.csi.ninterm = vtp->nseq - vtp->payload.csi.ninterm;
        dispatch(vtp, EVENT_CSI);
    } break;

    case FSM_DISPATCH_DCS: dispatch(vtp, EVENT_DCS); break;
    case FSM_DISPATCH_OSC: dispatch(vtp, EVENT_OSC); break;

    case FSM_DISPATCH_ST: {
        switch (vtp->fsm.from) {
        case STATE_OSC_STRING: dispatch(vtp, EVENT_OSC); break;
        case STATE_DCS_PASSTHROUGH: dispatch(vtp, EVENT_DCS); break;
        default: break;
        }
    } break;

    case FSM_COLLECT: collect(vtp, input); break;
    case FSM_REPLAY: s_rollback(&vtp->scanner); break;
    case FSM_NONE: break;
    }

    return fsm_state(effect);
}

static inline void collect(VT_Parser *vtp, uchar input)
{
    switch (vtp->fsm.state) {
    case STATE_DCS_FINAL: {
        vtp->payload.dcs.final_byte = input;
    } break;
    default: {
        if (vtp->nseq < sizeof(vtp->seq))
            vtp->seq[vtp->nseq++] = input;
    } break;
    }
    debug_2("Collect { input: 0x%02x, current_seq: '%s'}\n", input, vtp->seq);
}

static inline void transition(VT_Parser *vtp, FSM_State next_state)
{
    if (next_state == vtp->fsm.state)
        return;

#if DEBUG_LVL >= 2 // {{{
    debug_2("Transition { ");
#define FROM_REPR(sym)                                                         \
    case sym: debug(#sym " -> "); break;
    switch (vtp->fsm.state) {
        FROM_REPR(STATE_GROUND);
        FROM_REPR(STATE_UTF8_DECODE);
        FROM_REPR(STATE_ESC);
        FROM_REPR(STATE_ESC_INTERM);
        FROM_REPR(STATE_CSI_PARAM);
        FROM_REPR(STATE_CSI_INTERM);
        FROM_REPR(STATE_CSI_IGNORE);
        FROM_REPR(STATE_OSC_STRING);
        FROM_REPR(STATE_DCS_PARAM);
        FROM_REPR(STATE_DCS_INTERM);
        FROM_REPR(STATE_DCS_FINAL);
        FROM_REPR(STATE_DCS_PASSTHROUGH);
        FROM_REPR(STATE_DCS_IGNORE);
        FROM_REPR(STATE_ST);
    }
#undef FROM_REPR
#define TO_REPR(sym)                                                           \
    case sym: debug(#sym); break;
    switch (next_state) {
        TO_REPR(STATE_GROUND);
        TO_REPR(STATE_UTF8_DECODE);
        TO_REPR(STATE_ESC);
        TO_REPR(STATE_ESC_INTERM);
        TO_REPR(STATE_CSI_PARAM);
        TO_REPR(STATE_CSI_INTERM);
        TO_REPR(STATE_CSI_IGNORE);
        TO_REPR(STATE_OSC_STRING);
        TO_REPR(STATE_DCS_PARAM);
        TO_REPR(STATE_DCS_INTERM);
        TO_REPR(STATE_DCS_FINAL);
        TO_REPR(STATE_DCS_PASSTHROUGH);
        TO_REPR(STATE_DCS_IGNORE);
        TO_REPR(STATE_ST);
    }
#undef TO_REPR
    debug(" }\n");
#endif // }}}

    switch (vtp->fsm.from = vtp->fsm.state) { // on Exit.
    default: break;
    }

    switch (vtp->fsm.state = next_state) { // on Enter.
    case STATE_UTF8_DECODE: {
        vtp->utf8_decoder.rune = 0;
    } break;
    case STATE_ESC: {
        memset(vtp->seq, vtp->nseq = 0, sizeof(vtp->seq));
    } break;
    case STATE_CSI_INTERM: {
        // @NOTE: marking the beginning of the interm sequence, would be used
        // later while dispatching csi in 'FSM_DISPATCH_CSI' action.
        vtp->payload.csi.interm  = vtp->seq + vtp->nseq;
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
    case EVENT_CTRL: break;
    case EVENT_ESC: {
        prepare_esc_payload(vtp, &vtp->payload.esc);
    } break;
    case EVENT_CSI: {
        prepare_csi_payload(vtp, &vtp->payload.csi);
    } break;
    case EVENT_OSC: {
        prepare_osc_payload(vtp, &vtp->payload.osc);
    } break;
    case EVENT_DCS: {
        prepare_dcs_payload(vtp, &vtp->payload.dcs);
    } break;
    }

#if DEBUG_LVL >= 2 // {{{
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
        if (esc->action == ESC_UNKNOWN) {
            debug(": 'ESC%s%c'", esc->interm, esc->final_byte);
        } else {
            debug(": '%s' %c", esc->interm, esc->final_byte);
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
            CASE_REPR(CSI_DECSCUSR);
            CASE_REPR(CSI_DECSTBM);
            CASE_REPR(CSI_DECSET);
            CASE_REPR(CSI_DECRST);
            CASE_REPR(CSI_DA1);
            CASE_REPR(CSI_UNKNOWN);
        }
        if (csi->action == CSI_UNKNOWN) {
            debug(": ESC[%s%c", vtp->seq, csi->final_byte);
        } else {
            if (csi->nparam)
                debug(": %d", csi->param[0]);
            for (int i = 1; i < csi->nparam; ++i)
                debug(" %d", csi->param[i]);
            if (csi->ninterm)
                debug(" ([%d]: %s)", csi->ninterm, csi->interm);
        }
    } break;
    case EVENT_OSC: {
        debug("[OSC]: '%s'", vtp->seq);
    } break;
    case EVENT_DCS: {
        debug("[DCS]: %c '%s'", vtp->payload.dcs.final_byte, vtp->seq);
    } break;
#undef CASE_REPR
    }
    debug(" }\n");
    fflush(stdout);
#endif // }}}

    vtp->fsm.dispatching = true;
}

static inline void prepare_esc_payload(VT_Parser *vtp, ESC_Payload *esc)
{
    switch (esc->action = esc->final_byte) {
    case ESC_IND: // fallthrough
    case ESC_RI:  // fallthrough
    case ESC_HTS: {
        if (vtp->nseq) // ESC C.
            goto unknown;
    } break;

    case ESC_CS_LINEGFX: // fallthrough
    case ESC_CS_USASCII: {
        // ESC [()*+] C (ensure index to designate the character set).
        if (!(vtp->nseq == 1 && BETWEEN(vtp->seq[0], '(', '+')))
            goto unknown;
    } break;

    case ESC_DECSC: break;
    case ESC_DECRC: break;

    default: {
    unknown:
        esc->action = ESC_UNKNOWN;
    } break;
    }
}

static inline void prepare_csi_payload(VT_Parser *vtp, CSI_Payload *csi)
{
    memset(csi->param, csi->nparam = 0, sizeof(csi->param));

    Scanner param_s  = SCANNER(vtp->seq, vtp->nseq - csi->ninterm);
    Scanner interm_s = SCANNER(csi->interm, csi->ninterm);

    switch (csi->action = csi->final_byte) {
    case CSI_DECSCUSR: {
        if (!s_consume(&interm_s, ' '))
            goto unknown;
    } // fallthrough
    case CSI_CUU: // fallthrough
    case CSI_CUD: // fallthrough
    case CSI_CUF: // fallthrough
    case CSI_CUB: // fallthrough
    case CSI_VPA: // fallthrough
    case CSI_CNL: // fallthrough
    case CSI_CPL: // fallthrough
    case CSI_CHA: // fallthrough
    case CSI_TBC: // fallthrough
    case CSI_CHT: // fallthrough
    case CSI_CBT: // fallthrough
    case CSI_ED:  // fallthrough
    case CSI_EL:  // fallthrough
    case CSI_SU:  // fallthrough
    case CSI_SD:  // fallthrough
    case CSI_IL:  // fallthrough
    case CSI_DL:  // fallthrough
    case CSI_ICH: // fallthrough
    case CSI_DCH: // fallthrough
    case CSI_ECH: // fallthrough
    case CSI_DA1: {
        // CSI Ps C (force single param, default: 0).
        csi->param[csi->nparam++] = s_consume_number(&param_s);
    } break;

    case CSI_CUP: // fallthrough
    case CSI_HVP: // fallthrough
    case CSI_DECSTBM: {
        // CSI Ps ; Ps C (force two delimited params, default: {0, 0}).
        csi->param[csi->nparam++] = s_consume_number(&param_s);
        (void)s_consume_param_delim(&param_s);
        csi->param[csi->nparam++] = s_consume_number(&param_s);
    } break;

    case CSI_DECSET: // fallthrough
    case CSI_DECRST: {
        // CSI ? Pm C (check for private marker, e.g: '?').
        if (s_consume(&param_s, '?'))
            goto ensure_multiple_param;
        else
            goto unknown;
    } break;

    case CSI_SGR: {
        // CSI Ps ; Pm C (delimited params).
    ensure_multiple_param:
        do {
            csi->param[csi->nparam++] = s_consume_number(&param_s);
        } while (s_consume_param_delim(&param_s));
    } break;

    case CSI_SC: break;
    case CSI_RC: break;

    default: {
    unknown:
        csi->action = CSI_UNKNOWN;
    } break;
    }

    if (s_buflen(&param_s) || s_buflen(&interm_s)) // extra unparsed seq.
        csi->action = CSI_UNKNOWN;
}

static inline void prepare_osc_payload(VT_Parser *vtp, OSC_Payload *osc)
{
    osc->action  = OSC_UNKNOWN;
    osc->scanner = SCANNER(vtp->seq, vtp->nseq);

    const uchar *ch = s_peek(&osc->scanner);
    if (!ch || !BETWEEN(*ch, '0', '9'))
        return;
    osc->action = s_consume_number(&osc->scanner);
}

static inline void prepare_dcs_payload(VT_Parser *vtp, DCS_Payload *dcs)
{
    dcs->seq  = vtp->seq;
    dcs->nseq = vtp->nseq;
    // @TODO: unimplemented.
}
