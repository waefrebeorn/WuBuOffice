/* input.c -- pure byte-stream key decoder (no fd reads). */
#include "input.h"

/* CSI sequences: ESC '[' ... final. We recognize the common editing keys.
 * "\x1b[A" up, [B down, [C right, [D left, [H home, [F end,
 * "\x1b[1~" home, [2~ insert, [3~ delete, [4~ end, [5~ pgup, [6~ pgdn,
 * [7~ home, [8~ end. Also SS3 "\x1bO" A/B/C/D/H/F for application mode. */

/* Fill the shared button/action/modifier decode from an xterm button code.
 * The low 2 bits select button (0=left,1=middle,2=right,3=release-in-X10);
 * bit 2 = shift, bit 3 = alt/meta, bit 4 = ctrl, bit 5 = motion(drag/move),
 * bit 6 = wheel (then low bits: 0=up,1=down). This encoding is shared by the
 * legacy X10 and the modern SGR 1006 reports. `sgr_release` is 1 when an SGR
 * report ended with 'm' (button-up); X10 has no per-button release. */
static void fill_mouse(TuiKey *o, int cb, size_t x0, size_t y0, int sgr_release) {
    o->type = TUI_KEY_MOUSE;
    o->mouse_x = x0;
    o->mouse_y = y0;
    o->mouse_shift = (cb & 0x04) ? 1 : 0;
    o->mouse_alt   = (cb & 0x08) ? 1 : 0;
    o->mouse_ctrl  = (cb & 0x10) ? 1 : 0;
    o->mouse_button = TUI_MBTN_NONE;

    if (cb & 0x40) {                       /* wheel */
        o->mouse_action = (cb & 0x01) ? TUI_MOUSE_WHEEL_DOWN : TUI_MOUSE_WHEEL_UP;
        return;
    }
    int low = cb & 0x03;
    int motion = (cb & 0x20) ? 1 : 0;
    if (motion) {
        o->mouse_action = (low == 3) ? TUI_MOUSE_MOVE : TUI_MOUSE_DRAG;
    } else if (sgr_release) {
        o->mouse_action = TUI_MOUSE_RELEASE;
    } else if (low == 3) {
        o->mouse_action = TUI_MOUSE_RELEASE;   /* X10 release: button unknown */
    } else {
        o->mouse_action = TUI_MOUSE_PRESS;
    }
    switch (low) {
        case 0: o->mouse_button = TUI_MBTN_LEFT;   break;
        case 1: o->mouse_button = TUI_MBTN_MIDDLE; break;
        case 2: o->mouse_button = TUI_MBTN_RIGHT;  break;
        default: o->mouse_button = TUI_MBTN_NONE;  break; /* release/motion */
    }
}

/* SGR 1006 mouse: ESC [ < Cb ; Cx ; Cy (M|m). Coords are 1-based; 'M'=press/
 * motion/wheel, 'm'=release. b[0..2] == ESC '[' '<' guaranteed by caller. */
static size_t decode_sgr_mouse(const char *b, size_t len, TuiKey *o) {
    size_t i = 3;
    int cb = 0, cx = 0, cy = 0, field = 0, digits = 0;
    for (; i < len; i++) {
        char c = b[i];
        if (c >= '0' && c <= '9') {
            int d = c - '0';
            if (field == 0) cb = cb * 10 + d;
            else if (field == 1) cx = cx * 10 + d;
            else cy = cy * 10 + d;
            digits++;
        } else if (c == ';') {
            if (field < 2) field++;
            digits = 0;
        } else if (c == 'M' || c == 'm') {
            if (field != 2 || digits == 0) { o->type = TUI_KEY_ESC; return i + 1; }
            size_t x0 = cx > 0 ? (size_t)(cx - 1) : 0;
            size_t y0 = cy > 0 ? (size_t)(cy - 1) : 0;
            fill_mouse(o, cb, x0, y0, c == 'm');
            return i + 1;
        } else {
            o->type = TUI_KEY_ESC;   /* malformed but terminated */
            return i + 1;
        }
    }
    o->type = TUI_KEY_INCOMPLETE;    /* need more bytes */
    return 0;
}

/* Legacy X10 mouse: ESC [ M Cb Cx Cy, each a single byte biased by 32. Coords
 * are 1-based after removing the bias. b[0..2] == ESC '[' 'M' guaranteed. */
static size_t decode_x10_mouse(const char *b, size_t len, TuiKey *o) {
    if (len < 6) { o->type = TUI_KEY_INCOMPLETE; return 0; }
    int cb = (unsigned char)b[3] - 32;
    int cx = (unsigned char)b[4] - 32;
    int cy = (unsigned char)b[5] - 32;
    size_t x0 = cx > 0 ? (size_t)(cx - 1) : 0;
    size_t y0 = cy > 0 ? (size_t)(cy - 1) : 0;
    fill_mouse(o, cb, x0, y0, 0);
    return 6;
}

static size_t decode_csi(const char *b, size_t len, TuiKey *o) {
    /* b[0]==ESC, b[1]=='[' guaranteed by caller; need at least a 3rd byte. */
    if (len < 3) { o->type = TUI_KEY_INCOMPLETE; return 0; }
    char c = b[2];
    if (c == '<') return decode_sgr_mouse(b, len, o);   /* SGR 1006 */
    if (c == 'M') return decode_x10_mouse(b, len, o);   /* legacy X10 */
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
    o->mouse_action = TUI_MOUSE_PRESS;
    o->mouse_button = TUI_MBTN_NONE;
    o->mouse_x = o->mouse_y = 0;
    o->mouse_shift = o->mouse_alt = o->mouse_ctrl = 0;
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
        case TUI_KEY_MOUSE:      return "MOUSE";
    }
    return "?";
}
