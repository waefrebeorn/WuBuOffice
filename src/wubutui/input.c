/* input.c -- pure byte-stream key decoder (no fd reads). */
#include "input.h"

/* CSI sequences: ESC '[' ... final. We recognize the common editing keys.
 * "\x1b[A" up, [B down, [C right, [D left, [H home, [F end,
 * "\x1b[1~" home, [2~ insert, [3~ delete, [4~ end, [5~ pgup, [6~ pgdn,
 * [7~ home, [8~ end. Also SS3 "\x1bO" A/B/C/D/H/F for application mode. */

static size_t decode_csi(const char *b, size_t len, TuiKey *o) {
    /* b[0]==ESC, b[1]=='[' guaranteed by caller; need at least a 3rd byte. */
    if (len < 3) { o->type = TUI_KEY_INCOMPLETE; return 0; }
    char c = b[2];
    switch (c) {
        case 'A': o->type = TUI_KEY_UP;    return 3;
        case 'B': o->type = TUI_KEY_DOWN;  return 3;
        case 'C': o->type = TUI_KEY_RIGHT; return 3;
        case 'D': o->type = TUI_KEY_LEFT;  return 3;
        case 'H': o->type = TUI_KEY_HOME;  return 3;
        case 'F': o->type = TUI_KEY_END;   return 3;
        default: break;
    }
    if (c >= '0' && c <= '9') {
        /* numeric parameter terminated by '~' */
        size_t i = 2;
        int val = 0;
        while (i < len && b[i] >= '0' && b[i] <= '9') { val = val * 10 + (b[i] - '0'); i++; }
        if (i >= len) { o->type = TUI_KEY_INCOMPLETE; return 0; }
        if (b[i] != '~') { o->type = TUI_KEY_ESC; return i + 1; } /* unknown, consume */
        switch (val) {
            case 1: case 7: o->type = TUI_KEY_HOME;      return i + 1;
            case 2:         o->type = TUI_KEY_INSERT;    return i + 1;
            case 3:         o->type = TUI_KEY_DELETE;    return i + 1;
            case 4: case 8: o->type = TUI_KEY_END;       return i + 1;
            case 5:         o->type = TUI_KEY_PAGE_UP;   return i + 1;
            case 6:         o->type = TUI_KEY_PAGE_DOWN; return i + 1;
            default:        o->type = TUI_KEY_ESC;       return i + 1;
        }
    }
    /* unknown but terminated CSI final byte (0x40..0x7e) */
    if ((unsigned char)c >= 0x40 && (unsigned char)c <= 0x7e) { o->type = TUI_KEY_ESC; return 3; }
    o->type = TUI_KEY_INCOMPLETE;
    return 0;
}

static size_t decode_ss3(const char *b, size_t len, TuiKey *o) {
    /* b[0]==ESC, b[1]=='O'; need a 3rd byte. */
    if (len < 3) { o->type = TUI_KEY_INCOMPLETE; return 0; }
    switch (b[2]) {
        case 'A': o->type = TUI_KEY_UP;    return 3;
        case 'B': o->type = TUI_KEY_DOWN;  return 3;
        case 'C': o->type = TUI_KEY_RIGHT; return 3;
        case 'D': o->type = TUI_KEY_LEFT;  return 3;
        case 'H': o->type = TUI_KEY_HOME;  return 3;
        case 'F': o->type = TUI_KEY_END;   return 3;
        default:  o->type = TUI_KEY_ESC;   return 3;
    }
}

size_t tui_key_decode(const char *buf, size_t len, TuiKey *out) {
    TuiKey scratch;
    TuiKey *o = out ? out : &scratch;
    o->ch = 0;
    if (len == 0) { o->type = TUI_KEY_NONE; return 0; }

    unsigned char c0 = (unsigned char)buf[0];

    if (c0 == 0x1b) {                     /* ESC */
        if (len == 1) { o->type = TUI_KEY_ESC; return 1; } /* lone ESC */
        if (buf[1] == '[') return decode_csi(buf, len, o);
        if (buf[1] == 'O') return decode_ss3(buf, len, o);
        /* ESC followed by something else: treat ESC as its own key */
        o->type = TUI_KEY_ESC;
        return 1;
    }

    switch (c0) {
        case '\r': case '\n': o->type = TUI_KEY_ENTER;     return 1;
        case '\t':            o->type = TUI_KEY_TAB;       return 1;
        case 0x7f: case 0x08: o->type = TUI_KEY_BACKSPACE; return 1;
        default: break;
    }

    o->type = TUI_KEY_CHAR;
    o->ch = buf[0];
    return 1;
}

const char *tui_key_name(TuiKeyType t) {
    switch (t) {
        case TUI_KEY_NONE:       return "NONE";
        case TUI_KEY_INCOMPLETE: return "INCOMPLETE";
        case TUI_KEY_CHAR:       return "CHAR";
        case TUI_KEY_ENTER:      return "ENTER";
        case TUI_KEY_ESC:        return "ESC";
        case TUI_KEY_TAB:        return "TAB";
        case TUI_KEY_BACKSPACE:  return "BACKSPACE";
        case TUI_KEY_UP:         return "UP";
        case TUI_KEY_DOWN:       return "DOWN";
        case TUI_KEY_LEFT:       return "LEFT";
        case TUI_KEY_RIGHT:      return "RIGHT";
        case TUI_KEY_PAGE_UP:    return "PAGE_UP";
        case TUI_KEY_PAGE_DOWN:  return "PAGE_DOWN";
        case TUI_KEY_HOME:       return "HOME";
        case TUI_KEY_END:        return "END";
        case TUI_KEY_INSERT:     return "INSERT";
        case TUI_KEY_DELETE:     return "DELETE";
    }
    return "?";
}
