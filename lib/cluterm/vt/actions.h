#ifndef __CLUTERM__VT__ACTIONS_H__
#define __CLUTERM__VT__ACTIONS_H__

typedef enum CTRL_Action {
    C0_NOOP = -1, // NOOP.
    C0_BEL  = 7,  // Bell.
    C0_BS,        // Backspace.
    C0_HT,        // Horizontal tab.
    C0_LF,        // Line feed.
    C0_VT,        // Vertical tab.
    C0_FF,        // Form feed.
    C0_CR,        // Carriage return.
    C0_SO,        // Shift out.
    C0_SI,        // Shift in.
} CTRL_Action;

typedef enum ESC_Action {
    ESC_UNKNOWN    = -1,
    ESC_IND        = 'D', // ESC D  (move cursor down, scroll if at bottom).
    ESC_RI         = 'M', // ESC M  (move cursor up, scroll if at top).
    ESC_HTS        = 'H', // ESC H  (Tab set).
    ESC_CS_LINEGFX = '0', // ESC [(,),*,+] 0  (Designate charset with line
                          // drawing chars, VT100).
    ESC_CS_USASCII = 'B', // ESC [(,),*,+] B  (Designate charset with US ASCII
                          // chars, VT100).
    ESC_DECSC = '7',      // ESC 7  (Save Cursor, VT100).
    ESC_DECRC = '8',      // ESC 8  (Save Cursor, VT100).
} ESC_Action;

typedef enum CSI_Action {
    CSI_UNKNOWN  = -1,
    CSI_CUU      = 'A', // CSI Ps A           (Cursor up).
    CSI_CUD      = 'B', // CSI Ps B           (Cursor down).
    CSI_CUF      = 'C', // CSI Ps C           (Cursor forward).
    CSI_CUB      = 'D', // CSI Ps D           (Cursor back).
    CSI_VPA      = 'd', // CSI Ps d           (Line position absolute).
    CSI_CNL      = 'E', // CSI Ps E           (Cursor next line).
    CSI_CPL      = 'F', // CSI Ps F           (Cursor previous line).
    CSI_CHA      = 'G', // CSI Ps G           (Cursor horizontal Absolute).
    CSI_CUP      = 'H', // CSI Ps ; Ps H      (Cursor position).
    CSI_CHT      = 'I', // CSI Ps I           (Forward tabulation).
    CSI_CBT      = 'Z', // CSI Ps Z           (Backward tabulation).
    CSI_TBC      = 'g', // CSI Ps g           (Tabulation clear).
    CSI_ED       = 'J', // CSI Ps J           (Erase in display).
    CSI_EL       = 'K', // CSI Ps K           (Erase in line).
    CSI_IL       = 'L', // CSI Ps L           (Insert lines).
    CSI_DL       = 'M', // CSI Ps M           (Delete lines).
    CSI_ICH      = '@', // CSI Ps @           (Insert blank chars).
    CSI_DCH      = 'P', // CSI Ps P           (Delete chars on current line).
    CSI_ECH      = 'X', // CSI Ps X           (Erase chars on current line).
    CSI_SU       = 'S', // CSI Ps S           (Scroll up).
    CSI_SD       = 'T', // CSI Ps T           (Scroll down).
    CSI_DSR      = 'n', // CSI Ps n           (Device Status Report).
    CSI_HVP      = 'f', // CSI Ps ; Ps f      (Horizontal Vertical position).
    CSI_SGR      = 'm', // CSI Pm m           (Select Graphic Rendition).
    CSI_SC       = 's', // CSI s              (Save current cursor position).
    CSI_RC       = 'u', // CSI u              (Restore saved cursor position).
    CSI_DECSCUSR = 'q', // CSI Ps SP q        (Set Cursor Style).
    CSI_DECSTBM  = 'r', // CSI Ps ; Ps r      (Set scrolling region).
    CSI_DECSET   = 'h', // CSI Pm h           (Private mode 'set', xterm).
    CSI_DECRST   = 'l', // CSI Pm l           (Private mode 'reset', xterm).
    CSI_DA1      = 'c', // CSI c              (Private mode 'reset', xterm).
} CSI_Action;

typedef enum OSC_Action {
    OSC_UNKNOWN = -1,
    OSC_0,         // OSC 0      (set icon name and window title).
    OSC_2  = 2,    // OSC 2      (Set window title).
    OSC_4  = 4,    // OSC 4      (change palette color).
    OSC_7  = 7,    // OSC 7      (set current working directory).
    OSC_10 = 10,   // OSC 10     (set foreground color).
    OSC_11,        // OSC 11     (set background color).
    OSC_12,        // OSC 12     (set cursor color).
    OSC_104 = 104, // OSC 104    (reset palette color).
    OSC_110 = 110, // OSC 110    (reset foreground color).
    OSC_111,       // OSC 111    (reset background color).
    OSC_112,       // OSC 112    (reset cursor color).
} OSC_Action;

#endif
