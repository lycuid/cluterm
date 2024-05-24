#ifndef __CLUTERM__VTE__PARSER_H__
#define __CLUTERM__VTE__PARSER_H__

#include <cluterm/utf8.h>
#include <cluterm/vte.h>

typedef struct Reader {
    const char *buffer;
    uint32_t cursor, size;
} Reader;

#define READER(str, len)                                                       \
    (Reader) { .buffer = str, .cursor = 0, .size = len }

typedef enum FSM_State {
    STATE_GROUND = 0,
    STATE_UTF8_DECODE,
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
    Reader reader;
    UTF8_Decoder utf8_decoder;
    char seq[1 << 6];
    int nseq;
    VT_Payload payload;
    struct {
        FSM_State state;
        FSM_Event event;
        bool dispatching : 1;
    } fsm;
} VT_Parser;

void parser_init(VT_Parser *);
void parser_feed(VT_Parser *, const char *, uint32_t);
FSM_Event parser_run(VT_Parser *);

#endif
// vim:fdm=marker
