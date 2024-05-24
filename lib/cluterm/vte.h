#ifndef __CLUTERM__VTE_H__
#define __CLUTERM__VTE_H__

#define ENUM enum __attribute__((__packed__))

typedef ENUM CTRL_Action{
    C0_BEL = 7, // Bell.
    C0_BS,      // Backspace.
    C0_HT,      // Horizontal tab.
    C0_LF,      // Line feed.
    C0_VT,      // Vertical tab.
    C0_FF,      // Form feed.
    C0_CR,      // Carriage return.
    C0_SO,      // Shift out.
    C0_SI,      // Shift in.
} CTRL_Action;

typedef ENUM ESC_Action{
    ESC_IND,        // ESC D            (move cursor down, scroll if at bottom).
    ESC_RI,         // ESC M            (move cursor up, scroll if at top).
    ESC_HTS,        // ESC H            (Tab set).
    ESC_CS_LINEGFX, // ESC [(,),*,+] 0  (Designate charset with line drawing
                    // chars, VT100).
    ESC_CS_USASCII, // ESC [(,),*,+] B  (Designate charset with US ASCII chars,
                    // VT100).
    ESC_DECSC,      // ESC 7            (Save Cursor, VT100).
    ESC_DECRC,      // ESC 8            (Save Cursor, VT100).
    ESC_UNKNOWN,
} ESC_Action;

typedef ENUM CSI_Action{
    CSI_CUU,     // CSI Ps A           (Cursor up).
    CSI_CUD,     // CSI Ps B           (Cursor down).
    CSI_CUF,     // CSI Ps C           (Cursor forward).
    CSI_CUB,     // CSI Ps D           (Cursor back).
    CSI_VPA,     // CSI Ps d           (Line position absolute).
    CSI_CNL,     // CSI Ps E           (Cursor next line).
    CSI_CPL,     // CSI Ps F           (Cursor previous line).
    CSI_CHA,     // CSI Ps G           (Cursor horizontal Absolute).
    CSI_CUP,     // CSI Ps ; Ps H      (Cursor position).
    CSI_CHT,     // CSI Ps I           (Forward tabulation).
    CSI_CBT,     // CSI Ps Z           (Backward tabulation).
    CSI_TBC,     // CSI Ps g           (Tabulation clear).
    CSI_ED,      // CSI Ps J           (Erase in display).
    CSI_EL,      // CSI Ps K           (Erase in line).
    CSI_IL,      // CSI Ps L           (Insert lines).
    CSI_DL,      // CSI Ps M           (Delete lines).
    CSI_ICH,     // CSI Ps @           (Insert blank chars).
    CSI_DCH,     // CSI Ps P           (Delete chars on current line).
    CSI_ECH,     // CSI Ps X           (Erase chars on current line).
    CSI_SU,      // CSI Ps S           (Scroll up).
    CSI_SD,      // CSI Ps T           (Scroll down).
    CSI_HVP,     // CSI Ps ; Ps f      (Horizontal Vertical position).
    CSI_SGR,     // CSI Pm m           (Select Graphic Rendition).
    CSI_SC,      // CSI s              (Save current cursor position).
    CSI_RC,      // CSI u              (Restore saved cursor position).
    CSI_DECSTBM, // CSI Ps ; Ps r      (Set scrolling region).
    CSI_DECSET,  // CSI Pm h           (Private mode 'set', xterm).
    CSI_DECRST,  // CSI Pm l           (Private mode 'reset', xterm).
    CSI_UNKNOWN,
} CSI_Action;

#endif
