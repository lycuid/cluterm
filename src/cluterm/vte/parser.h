#ifndef __VTE__STATE_MACHINE_H__
#define __VTE__STATE_MACHINE_H__

#include <cluterm/utf8.h>
#include <cluterm/vte.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Parser {
    const char *buffer;
    uint32_t cursor, size;
} Parser;

// clang-format off
#define PARSER(str, len) (Parser){.buffer = str, .cursor = 0, .size = len}
// clang-format on

typedef enum FSM_State {
    STATE_DISPATCH = 0,
    STATE_GROUND,
    STATE_ESC,

    STATE_ESC_INTERM,
    STATE_ESC_FINAL,

    STATE_CSI_PARAM,
    STATE_CSI_INTERM,
    STATE_CSI_FINAL,
    STATE_CSI_IGNORE,

    STATE_OSC_STRING,
    STATE_OSC_ST,
} FSM_State;

typedef ENUM FSM_Event{EVENT_NOOP = 0, EVENT_PRINT, EVENT_CTRL,
                       EVENT_ESC,      EVENT_CSI,   EVENT_OSC} FSM_Event;

typedef struct CTRL_Payload {
    CTRL_Action action;
    Rune value;
} CTRL_Payload;

typedef struct ESC_Payload {
    ESC_Action action;
    char *interm, final_byte;
    int ninterm;
} ESC_Payload;

typedef struct CSI_Payload {
    CSI_Action action;
    int param[1 << 4], nparam, ninterm;
    char *interm, final_byte;
} CSI_Payload;

typedef struct OSC_Payload {
    int _;
} OSC_Payload;

typedef union VT_Payload {
    Rune value;
    CTRL_Payload ctrl;
    ESC_Payload esc;
    CSI_Payload csi;
    OSC_Payload osc;
} VT_Payload;

typedef struct VT_Parser {
    Parser inner;
    char seq[1 << 6];
    int nseq;
    VT_Payload payload;
    struct {
        FSM_State state;
        FSM_Event event;
    } fsm;
} VT_Parser;

void parser_init(VT_Parser *);
void parser_feed(VT_Parser *, const char *, uint32_t);
FSM_Event parser_run(VT_Parser *);
#define parser_consumed(vtp) ((vtp)->inner.cursor)

#endif
// vim:fdm=marker
