/**********************************************************************************************
 *
 * @file console_vt_ui.h
 *   (c) 2025-2026 Jacques Morel
 * @brief VT-native box drawing and pulldown menu helpers for the KaOS console
 *
 * Uses DEC Special Graphics (CSI ESC ( 0 ), SGR color attributes, CUP/EL/ECH,
 * DECSC/DECRC, and SGR mouse reports — not raw screen-buffer pokes.
 *
 **********************************************************************************************/
#ifndef CONSOLE_VT_UI_H
#define CONSOLE_VT_UI_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef enum {
    CONSOLE_VT_BORDER_DEC_SINGLE,
    CONSOLE_VT_BORDER_DEC_DOUBLE,
    CONSOLE_VT_BORDER_UNICODE_LIGHT,
    CONSOLE_VT_BORDER_UNICODE_HEAVY,
    CONSOLE_VT_BORDER_ASCII
} ConsoleVtBorderStyle;

typedef struct {
    int fg_ansi;
    int bg_ansi;
    int fg_256;
    int bg_256;
    int fg_rgb[3];
    int bg_rgb[3];
    bool bold;
    bool dim;
    bool italic;
    bool underline;
    bool inverse;
    bool blink;
    bool fg_truecolor;
    bool bg_truecolor;
    bool fg_256_mode;
    bool bg_256_mode;
} ConsoleVtStyle;

typedef struct {
    int row;
    int col;
    int width;
    int height;
    ConsoleVtBorderStyle border_style;
    ConsoleVtStyle border;
    ConsoleVtStyle fill;
    ConsoleVtStyle title_style;
    const char* title_text;
    bool fill_interior;
} ConsoleVtBoxParams;

typedef enum {
    CONSOLE_VT_WIDGET_MENU,
    CONSOLE_VT_WIDGET_COMBO,
    CONSOLE_VT_WIDGET_DIALOG
} ConsoleVtWidgetKind;

typedef struct {
    ConsoleVtWidgetKind kind;
    bool active;
    bool expanded;
    int origin_row;
    int origin_col;
    int width;
    int height;
    int highlight;
    int selected_index;
    int item_count;
    char items[32][64];
    ConsoleVtStyle item_style;
    ConsoleVtStyle highlight_style;
    ConsoleVtStyle border_style;
    ConsoleVtBorderStyle border;
    char message[160];
    int dialog_focus;
    bool scroll_saved;
    int saved_scroll_top;
    int saved_scroll_bottom;
} ConsoleVtWidgetState;

static ConsoleVtWidgetState vt_widget = {0};

static ConsoleVtStyle ConsoleVtStyleDefault(void) {
    ConsoleVtStyle s = {0};
    s.fg_ansi = -1;
    s.bg_ansi = -1;
    s.fg_256 = -1;
    s.bg_256 = -1;
    return s;
}

static bool ConsoleVtWidgetIsActive(void) {
    return vt_widget.active;
}

static bool ConsoleVtMenuIsActive(void) {
    return ConsoleVtWidgetIsActive();
}

static void ConsoleVtCup(KTerm* t, int row, int col) {
    KTerm_WriteFormat(t, "\x1B[%d;%dH", row, col);
}

static void ConsoleVtSetRowProtection(KTerm* t, int row, bool protect) {
    ConsoleVtCup(t, row, 1);
    KTerm_WriteString(t, protect ? "\x1B[1 q" : "\x1B[0 q");
}

static void ConsoleVtOverlaySaveScroll(KTerm* t) {
    if (vt_widget.scroll_saved || !t) return;
    KTermSession* session = GET_SESSION(t);
    vt_widget.saved_scroll_top = session->scroll_top + 1;
    vt_widget.saved_scroll_bottom = session->scroll_bottom + 1;
    vt_widget.scroll_saved = true;
}

static void ConsoleVtOverlayRestoreScroll(KTerm* t) {
    if (!vt_widget.scroll_saved || !t) return;
    KTerm_WriteFormat(t, "\x1B[%d;%dr", vt_widget.saved_scroll_top, vt_widget.saved_scroll_bottom);
    vt_widget.scroll_saved = false;
}

static void ConsoleVtProtectBoxRows(KTerm* t, int top_row, int bottom_row) {
    ConsoleVtSetRowProtection(t, top_row, true);
    ConsoleVtSetRowProtection(t, bottom_row, true);
}

static void ConsoleVtUnprotectScreen(KTerm* t) {
    ConsoleVtSetRowProtection(t, 1, false);
}

static void ConsoleVtHideCursor(KTerm* t) {
    KTerm_WriteString(t, "\x1B[?25l");
}

static void ConsoleVtShowCursor(KTerm* t) {
    KTerm_WriteString(t, "\x1B[?25h");
}

static void ConsoleVtSaveCursor(KTerm* t) {
    KTerm_WriteString(t, "\x1B[s");
}

static void ConsoleVtRestoreCursor(KTerm* t) {
    KTerm_WriteString(t, "\x1B[u");
}

static void ConsoleVtResetAttributes(KTerm* t) {
    KTerm_WriteString(t, "\x1B[0m");
}

static void ConsoleVtBeginDecLineDraw(KTerm* t) {
    KTerm_WriteString(t, "\x1B(0");
}

static void ConsoleVtEndDecLineDraw(KTerm* t) {
    KTerm_WriteString(t, "\x1B(B");
}

static void ConsoleVtAppendSgrCode(char* buf, size_t buf_size, int* len, int code, bool* first) {
    if (*len + 16 >= (int)buf_size) return;
    if (!(*first)) {
        buf[(*len)++] = ';';
    }
    *first = false;
    *len += snprintf(buf + *len, buf_size - (size_t)*len, "%d", code);
}

static void ConsoleVtEmitStyle(KTerm* t, const ConsoleVtStyle* style) {
    if (!style) {
        ConsoleVtResetAttributes(t);
        return;
    }

    char buf[160];
    int len = 0;
    bool first = true;
    buf[len++] = '\x1B';
    buf[len++] = '[';

    if (style->bold) ConsoleVtAppendSgrCode(buf, sizeof(buf), &len, 1, &first);
    if (style->dim) ConsoleVtAppendSgrCode(buf, sizeof(buf), &len, 2, &first);
    if (style->italic) ConsoleVtAppendSgrCode(buf, sizeof(buf), &len, 3, &first);
    if (style->underline) ConsoleVtAppendSgrCode(buf, sizeof(buf), &len, 4, &first);
    if (style->blink) ConsoleVtAppendSgrCode(buf, sizeof(buf), &len, 5, &first);
    if (style->inverse) ConsoleVtAppendSgrCode(buf, sizeof(buf), &len, 7, &first);

    if (style->fg_truecolor) {
        ConsoleVtAppendSgrCode(buf, sizeof(buf), &len, 38, &first);
        ConsoleVtAppendSgrCode(buf, sizeof(buf), &len, 2, &first);
        ConsoleVtAppendSgrCode(buf, sizeof(buf), &len, style->fg_rgb[0], &first);
        ConsoleVtAppendSgrCode(buf, sizeof(buf), &len, style->fg_rgb[1], &first);
        ConsoleVtAppendSgrCode(buf, sizeof(buf), &len, style->fg_rgb[2], &first);
    } else if (style->fg_256_mode && style->fg_256 >= 0) {
        ConsoleVtAppendSgrCode(buf, sizeof(buf), &len, 38, &first);
        ConsoleVtAppendSgrCode(buf, sizeof(buf), &len, 5, &first);
        ConsoleVtAppendSgrCode(buf, sizeof(buf), &len, style->fg_256, &first);
    } else if (style->fg_ansi >= 0) {
        int code = (style->fg_ansi < 8) ? (30 + style->fg_ansi) : (90 + (style->fg_ansi - 8));
        ConsoleVtAppendSgrCode(buf, sizeof(buf), &len, code, &first);
    }

    if (style->bg_truecolor) {
        ConsoleVtAppendSgrCode(buf, sizeof(buf), &len, 48, &first);
        ConsoleVtAppendSgrCode(buf, sizeof(buf), &len, 2, &first);
        ConsoleVtAppendSgrCode(buf, sizeof(buf), &len, style->bg_rgb[0], &first);
        ConsoleVtAppendSgrCode(buf, sizeof(buf), &len, style->bg_rgb[1], &first);
        ConsoleVtAppendSgrCode(buf, sizeof(buf), &len, style->bg_rgb[2], &first);
    } else if (style->bg_256_mode && style->bg_256 >= 0) {
        ConsoleVtAppendSgrCode(buf, sizeof(buf), &len, 48, &first);
        ConsoleVtAppendSgrCode(buf, sizeof(buf), &len, 5, &first);
        ConsoleVtAppendSgrCode(buf, sizeof(buf), &len, style->bg_256, &first);
    } else if (style->bg_ansi >= 0) {
        int code = (style->bg_ansi < 8) ? (40 + style->bg_ansi) : (100 + (style->bg_ansi - 8));
        ConsoleVtAppendSgrCode(buf, sizeof(buf), &len, code, &first);
    }

    if (first) {
        ConsoleVtAppendSgrCode(buf, sizeof(buf), &len, 0, &first);
    }

    buf[len++] = 'm';
    buf[len] = '\0';
    KTerm_WriteString(t, buf);
}

static bool ConsoleVtIsBorderStyleName(const char* token) {
    if (!token) return false;
    return strcmp(token, "dec") == 0 || strcmp(token, "single") == 0 ||
           strcmp(token, "double") == 0 || strcmp(token, "dec_double") == 0 ||
           strcmp(token, "unicode") == 0 || strcmp(token, "light") == 0 ||
           strcmp(token, "heavy") == 0 || strcmp(token, "unicode_heavy") == 0 ||
           strcmp(token, "ascii") == 0;
}

static bool ConsoleVtParseStyleToken(const char* token, ConsoleVtStyle* style) {
    if (!token || !style) return false;

    if (strcmp(token, "bold") == 0) {
        style->bold = true;
        return true;
    }
    if (strcmp(token, "dim") == 0) {
        style->dim = true;
        return true;
    }
    if (strcmp(token, "italic") == 0) {
        style->italic = true;
        return true;
    }
    if (strcmp(token, "underline") == 0) {
        style->underline = true;
        return true;
    }
    if (strcmp(token, "inverse") == 0) {
        style->inverse = true;
        return true;
    }
    if (strcmp(token, "blink") == 0) {
        style->blink = true;
        return true;
    }

    static const struct { const char* name; int ansi; } named[] = {
        {"black", 0}, {"red", 1}, {"green", 2}, {"yellow", 3},
        {"blue", 4}, {"magenta", 5}, {"cyan", 6}, {"white", 7},
        {"bright_black", 8}, {"bright_red", 9}, {"bright_green", 10}, {"bright_yellow", 11},
        {"bright_blue", 12}, {"bright_magenta", 13}, {"bright_cyan", 14}, {"bright_white", 15}
    };
    for (size_t i = 0; i < sizeof(named) / sizeof(named[0]); i++) {
        if (strcmp(token, named[i].name) == 0) {
            style->fg_ansi = named[i].ansi;
            return true;
        }
    }

    if (strncmp(token, "256:", 4) == 0) {
        style->fg_256_mode = true;
        style->fg_256 = atoi(token + 4);
        return true;
    }
    if (strncmp(token, "bg256:", 6) == 0) {
        style->bg_256_mode = true;
        style->bg_256 = atoi(token + 6);
        return true;
    }
    if (strncmp(token, "rgb:", 4) == 0) {
        int r = 0, g = 0, b = 0;
        if (sscanf(token + 4, "%d,%d,%d", &r, &g, &b) == 3 ||
            sscanf(token + 4, "%d-%d-%d", &r, &g, &b) == 3) {
            style->fg_truecolor = true;
            style->fg_rgb[0] = r;
            style->fg_rgb[1] = g;
            style->fg_rgb[2] = b;
            return true;
        }
    }
    if (strncmp(token, "bgrgb:", 6) == 0) {
        int r = 0, g = 0, b = 0;
        if (sscanf(token + 6, "%d,%d,%d", &r, &g, &b) == 3 ||
            sscanf(token + 6, "%d-%d-%d", &r, &g, &b) == 3) {
            style->bg_truecolor = true;
            style->bg_rgb[0] = r;
            style->bg_rgb[1] = g;
            style->bg_rgb[2] = b;
            return true;
        }
    }
    return false;
}

static ConsoleVtBorderStyle ConsoleVtParseBorderStyle(const char* name) {
    if (!name) return CONSOLE_VT_BORDER_DEC_SINGLE;
    if (strcmp(name, "dec") == 0 || strcmp(name, "single") == 0) return CONSOLE_VT_BORDER_DEC_SINGLE;
    if (strcmp(name, "double") == 0 || strcmp(name, "dec_double") == 0) return CONSOLE_VT_BORDER_DEC_DOUBLE;
    if (strcmp(name, "unicode") == 0 || strcmp(name, "light") == 0) return CONSOLE_VT_BORDER_UNICODE_LIGHT;
    if (strcmp(name, "heavy") == 0 || strcmp(name, "unicode_heavy") == 0) return CONSOLE_VT_BORDER_UNICODE_HEAVY;
    if (strcmp(name, "ascii") == 0) return CONSOLE_VT_BORDER_ASCII;
    return CONSOLE_VT_BORDER_DEC_SINGLE;
}

static void ConsoleVtGetBorderChars(ConsoleVtBorderStyle style, char* ul, char* ur, char* ll, char* lr,
                                    char* h, char* v, bool dec_mode) {
    if (dec_mode && style == CONSOLE_VT_BORDER_DEC_SINGLE) {
        *ul = 'l'; *ur = 'k'; *ll = 'm'; *lr = 'j'; *h = 'q'; *v = 'x';
        return;
    }

    switch (style) {
        case CONSOLE_VT_BORDER_DEC_DOUBLE:
            *ul = 0xC9; *ur = 0xBB; *ll = 0xC8; *lr = 0xBC; *h = 0xCD; *v = 0xBA;
            break;
        case CONSOLE_VT_BORDER_UNICODE_HEAVY:
            *ul = '+'; *ur = '+'; *ll = '+'; *lr = '+'; *h = '-'; *v = '|';
            break;
        case CONSOLE_VT_BORDER_ASCII:
            *ul = '+'; *ur = '+'; *ll = '+'; *lr = '+'; *h = '-'; *v = '|';
            break;
        case CONSOLE_VT_BORDER_UNICODE_LIGHT:
        default:
            *ul = '+'; *ur = '+'; *ll = '+'; *lr = '+'; *h = '-'; *v = '|';
            break;
    }
}

static void ConsoleVtWriteBorderChar(KTerm* t, ConsoleVtBorderStyle style, char ch, bool dec_mode) {
    if (dec_mode && style == CONSOLE_VT_BORDER_DEC_SINGLE) {
        KTerm_WriteChar(t, (unsigned char)ch);
        return;
    }

    switch (style) {
        case CONSOLE_VT_BORDER_UNICODE_LIGHT:
            if (ch == '+') KTerm_WriteString(t, "\xE2\x94\x8C");
            else if (ch == '-') KTerm_WriteString(t, "\xE2\x94\x80");
            else if (ch == '|') KTerm_WriteString(t, "\xE2\x94\x82");
            else KTerm_WriteChar(t, (unsigned char)ch);
            break;
        case CONSOLE_VT_BORDER_UNICODE_HEAVY:
            if (ch == '-') KTerm_WriteString(t, "\xE2\x95\x90");
            else if (ch == '|') KTerm_WriteString(t, "\xE2\x95\x91");
            else KTerm_WriteChar(t, (unsigned char)ch);
            break;
        case CONSOLE_VT_BORDER_DEC_DOUBLE:
            KTerm_WriteChar(t, (unsigned char)ch);
            break;
        default:
            KTerm_WriteChar(t, (unsigned char)ch);
            break;
    }
}

static void ConsoleVtWriteUnicodeCorner(KTerm* t, ConsoleVtBorderStyle style, int corner) {
    if (style == CONSOLE_VT_BORDER_UNICODE_LIGHT) {
        const char* corners[4] = {
            "\xE2\x94\x8C", "\xE2\x94\x90", "\xE2\x94\x94", "\xE2\x94\x98"
        };
        KTerm_WriteString(t, corners[corner & 3]);
    } else if (style == CONSOLE_VT_BORDER_UNICODE_HEAVY) {
        if (corner == 0) KTerm_WriteString(t, "\xE2\x95\x94");
        else if (corner == 1) KTerm_WriteString(t, "\xE2\x95\x97");
        else if (corner == 2) KTerm_WriteString(t, "\xE2\x95\x9A");
        else KTerm_WriteString(t, "\xE2\x95\x9D");
    }
}

static void ConsoleVtDrawBox(KTerm* t, const ConsoleVtBoxParams* params) {
    if (!t || !params || params->width < 1 || params->height < 1) return;

    bool dec_mode = (params->border_style == CONSOLE_VT_BORDER_DEC_SINGLE);
    char ul, ur, ll, lr, h, v;
    ConsoleVtGetBorderChars(params->border_style, &ul, &ur, &ll, &lr, &h, &v, dec_mode);

    if (dec_mode) ConsoleVtBeginDecLineDraw(t);

    if (params->fill_interior) {
        ConsoleVtStyle fill = params->fill;
        for (int r = 0; r < params->height + 2; r++) {
            ConsoleVtCup(t, params->row + r, params->col);
            ConsoleVtEmitStyle(t, &fill);
            for (int c = 0; c < params->width + 2; c++) {
                KTerm_WriteChar(t, ' ');
            }
            ConsoleVtResetAttributes(t);
        }
    }

    ConsoleVtCup(t, params->row, params->col);
    ConsoleVtEmitStyle(t, &params->border);
    if (params->border_style == CONSOLE_VT_BORDER_UNICODE_LIGHT ||
        params->border_style == CONSOLE_VT_BORDER_UNICODE_HEAVY) {
        ConsoleVtWriteUnicodeCorner(t, params->border_style, 0);
        for (int i = 0; i < params->width; i++) ConsoleVtWriteBorderChar(t, params->border_style, h, dec_mode);
        ConsoleVtWriteUnicodeCorner(t, params->border_style, 1);
    } else {
        ConsoleVtWriteBorderChar(t, params->border_style, ul, dec_mode);
        for (int i = 0; i < params->width; i++) ConsoleVtWriteBorderChar(t, params->border_style, h, dec_mode);
        ConsoleVtWriteBorderChar(t, params->border_style, ur, dec_mode);
    }
    ConsoleVtResetAttributes(t);

    for (int r = 1; r <= params->height; r++) {
        ConsoleVtCup(t, params->row + r, params->col);
        ConsoleVtEmitStyle(t, &params->border);
        ConsoleVtWriteBorderChar(t, params->border_style, v, dec_mode);
        ConsoleVtResetAttributes(t);

        ConsoleVtCup(t, params->row + r, params->col + params->width + 1);
        ConsoleVtEmitStyle(t, &params->border);
        ConsoleVtWriteBorderChar(t, params->border_style, v, dec_mode);
        ConsoleVtResetAttributes(t);
    }

    ConsoleVtCup(t, params->row + params->height + 1, params->col);
    ConsoleVtEmitStyle(t, &params->border);
    if (params->border_style == CONSOLE_VT_BORDER_UNICODE_LIGHT ||
        params->border_style == CONSOLE_VT_BORDER_UNICODE_HEAVY) {
        ConsoleVtWriteUnicodeCorner(t, params->border_style, 2);
        for (int i = 0; i < params->width; i++) ConsoleVtWriteBorderChar(t, params->border_style, h, dec_mode);
        ConsoleVtWriteUnicodeCorner(t, params->border_style, 3);
    } else {
        ConsoleVtWriteBorderChar(t, params->border_style, ll, dec_mode);
        for (int i = 0; i < params->width; i++) ConsoleVtWriteBorderChar(t, params->border_style, h, dec_mode);
        ConsoleVtWriteBorderChar(t, params->border_style, lr, dec_mode);
    }
    ConsoleVtResetAttributes(t);

    if (params->title_text && params->title_text[0]) {
        ConsoleVtCup(t, params->row, params->col + 2);
        ConsoleVtEmitStyle(t, &params->title_style);
        KTerm_WriteChar(t, ' ');
        KTerm_WriteString(t, params->title_text);
        KTerm_WriteChar(t, ' ');
        ConsoleVtResetAttributes(t);
    }

    if (dec_mode) ConsoleVtEndDecLineDraw(t);
}

static void ConsoleVtClearRegion(KTerm* t, int row, int col, int width, int height) {
    for (int r = 0; r < height; r++) {
        ConsoleVtCup(t, row + r, col);
        KTerm_WriteString(t, "\x1B[K");
        for (int c = 1; c < width; c++) {
            KTerm_WriteChar(t, ' ');
        }
    }
}

static int ConsoleVtMeasureItemsWidth(const char* const* items, int count) {
    int max_w = 8;
    for (int i = 0; i < count; i++) {
        int w = (int)strlen(items[i]);
        if (w > max_w) max_w = w;
    }
    return max_w;
}

static void ConsoleVtWidgetEnterModal(void) {
    console.input_enabled = false;
    console.prompt_pending = false;
    console.in_command = false;
    ConsoleVtOverlaySaveScroll(term);
    ConsoleVtHideCursor(term);
}

static void ConsoleVtWidgetLeaveModal(bool notify_cancel) {
    if (!vt_widget.active) return;

    int clear_row = vt_widget.origin_row;
    int clear_h = vt_widget.height;
    if (vt_widget.kind == CONSOLE_VT_WIDGET_COMBO && vt_widget.expanded) {
        clear_h = 3 + vt_widget.item_count + 2;
    }
    ConsoleVtClearRegion(term, clear_row, vt_widget.origin_col, vt_widget.width + 2, clear_h);
    ConsoleVtUnprotectScreen(term);
    ConsoleVtOverlayRestoreScroll(term);
    vt_widget.active = false;
    vt_widget.expanded = false;
    vt_widget.item_count = 0;
    vt_widget.highlight = 0;
    ConsoleVtShowCursor(term);
    if (notify_cancel) {
        KTerm_WriteString(term, "\x1B[90m(widget cancelled)\x1B[0m\n");
    }
    console.prompt_pending = true;
    console.input_enabled = false;
    console.in_command = false;
}

static void ConsoleVtWidgetApplyDefaultStyles(void) {
    if (vt_widget.item_style.fg_ansi < 0 && !vt_widget.item_style.fg_256_mode && !vt_widget.item_style.fg_truecolor) {
        vt_widget.item_style.fg_ansi = 7;
    }
    if (vt_widget.highlight_style.bg_ansi < 0 && !vt_widget.highlight_style.bg_256_mode &&
        !vt_widget.highlight_style.bg_truecolor && !vt_widget.highlight_style.inverse) {
        vt_widget.highlight_style.bg_ansi = 4;
        vt_widget.highlight_style.bold = true;
    }
    if (vt_widget.border_style.fg_ansi < 0 && !vt_widget.border_style.fg_256_mode &&
        !vt_widget.border_style.fg_truecolor) {
        vt_widget.border_style.fg_ansi = 6;
        vt_widget.border_style.bold = true;
    }
}

static void ConsoleVtWidgetPaintList(int list_row, int list_col, int width, int count, int highlight) {
    for (int i = 0; i < count; i++) {
        ConsoleVtCup(term, list_row + i, list_col + 2);
        if (i == highlight) {
            ConsoleVtEmitStyle(term, &vt_widget.highlight_style);
        } else {
            ConsoleVtEmitStyle(term, &vt_widget.item_style);
        }
        KTerm_WriteString(term, vt_widget.items[i]);
        int pad = width - (int)strlen(vt_widget.items[i]);
        for (int p = 0; p < pad; p++) KTerm_WriteChar(term, ' ');
        ConsoleVtResetAttributes(term);
    }
}

static void ConsoleVtWidgetRepaintMenu(void) {
    ConsoleVtBoxParams box = {0};
    box.row = vt_widget.origin_row;
    box.col = vt_widget.origin_col;
    box.width = vt_widget.width;
    box.height = vt_widget.item_count;
    box.border_style = vt_widget.border;
    box.border = vt_widget.border_style;
    box.fill_interior = true;
    box.fill = ConsoleVtStyleDefault();
    box.fill.bg_ansi = 0;
    ConsoleVtDrawBox(term, &box);
    ConsoleVtWidgetPaintList(vt_widget.origin_row + 1, vt_widget.origin_col, vt_widget.width,
                             vt_widget.item_count, vt_widget.highlight);
    ConsoleVtProtectBoxRows(term, vt_widget.origin_row, vt_widget.origin_row + vt_widget.height - 1);
}

static void ConsoleVtWidgetRepaintComboCollapsed(void) {
    ConsoleVtBoxParams box = {0};
    box.row = vt_widget.origin_row;
    box.col = vt_widget.origin_col;
    box.width = vt_widget.width;
    box.height = 1;
    box.border_style = vt_widget.border;
    box.border = vt_widget.border_style;
    box.fill_interior = true;
    box.fill = ConsoleVtStyleDefault();
    box.fill.bg_ansi = 0;
    ConsoleVtDrawBox(term, &box);

    ConsoleVtCup(term, vt_widget.origin_row + 1, vt_widget.origin_col + 2);
    ConsoleVtEmitStyle(term, &vt_widget.item_style);
    if (vt_widget.selected_index >= 0 && vt_widget.selected_index < vt_widget.item_count) {
        KTerm_WriteString(term, vt_widget.items[vt_widget.selected_index]);
    }
    int label_len = (vt_widget.selected_index >= 0 && vt_widget.selected_index < vt_widget.item_count)
        ? (int)strlen(vt_widget.items[vt_widget.selected_index]) : 0;
    int pad = vt_widget.width - label_len - 2;
    for (int p = 0; p < pad; p++) KTerm_WriteChar(term, ' ');
    KTerm_WriteString(term, "\xE2\x96\xBC");
    ConsoleVtResetAttributes(term);
    ConsoleVtProtectBoxRows(term, vt_widget.origin_row, vt_widget.origin_row + 2);
}

static void ConsoleVtWidgetRepaintCombo(void) {
    ConsoleVtWidgetRepaintComboCollapsed();
    if (!vt_widget.expanded) return;

    int list_row = vt_widget.origin_row + 3;
    ConsoleVtBoxParams box = {0};
    box.row = list_row;
    box.col = vt_widget.origin_col;
    box.width = vt_widget.width;
    box.height = vt_widget.item_count;
    box.border_style = vt_widget.border;
    box.border = vt_widget.border_style;
    box.fill_interior = true;
    box.fill = ConsoleVtStyleDefault();
    box.fill.bg_ansi = 0;
    ConsoleVtDrawBox(term, &box);
    ConsoleVtWidgetPaintList(list_row + 1, vt_widget.origin_col, vt_widget.width,
                             vt_widget.item_count, vt_widget.highlight);
    ConsoleVtProtectBoxRows(term, list_row, list_row + vt_widget.item_count + 1);
}

static void ConsoleVtWidgetRepaintDialog(void) {
    ConsoleVtBoxParams box = {0};
    box.row = vt_widget.origin_row;
    box.col = vt_widget.origin_col;
    box.width = vt_widget.width;
    box.height = 3;
    box.border_style = vt_widget.border;
    box.border = vt_widget.border_style;
    box.fill_interior = true;
    box.fill = ConsoleVtStyleDefault();
    box.fill.bg_ansi = 0;
    box.title_text = vt_widget.message;
    box.title_style = vt_widget.border_style;
    ConsoleVtDrawBox(term, &box);

    ConsoleVtCup(term, vt_widget.origin_row + 2, vt_widget.origin_col + 2);
    for (int b = 0; b < 2; b++) {
        if (b > 0) KTerm_WriteString(term, "  ");
        if (b == vt_widget.dialog_focus) {
            ConsoleVtEmitStyle(term, &vt_widget.highlight_style);
        } else {
            ConsoleVtEmitStyle(term, &vt_widget.item_style);
        }
        KTerm_WriteString(term, b == 0 ? "[ OK ]" : "[ Cancel ]");
        ConsoleVtResetAttributes(term);
    }
    ConsoleVtProtectBoxRows(term, vt_widget.origin_row, vt_widget.origin_row + vt_widget.height - 1);
}

static void ConsoleVtWidgetRepaint(void) {
    if (!vt_widget.active || !term) return;
    switch (vt_widget.kind) {
        case CONSOLE_VT_WIDGET_MENU: ConsoleVtWidgetRepaintMenu(); break;
        case CONSOLE_VT_WIDGET_COMBO: ConsoleVtWidgetRepaintCombo(); break;
        case CONSOLE_VT_WIDGET_DIALOG: ConsoleVtWidgetRepaintDialog(); break;
    }
    ConsoleVtHideCursor(term);
}

static void ConsoleVtWidgetConfirmSelection(void) {
    if (!vt_widget.active) return;
    if (vt_widget.kind == CONSOLE_VT_WIDGET_DIALOG) {
        bool ok = (vt_widget.dialog_focus == 0);
        ConsoleVtWidgetLeaveModal(false);
        KTerm_WriteFormat(term, "\x1B[32mDialog:\x1B[0m %s\n", ok ? "OK" : "Cancel");
        return;
    }

    int index = vt_widget.highlight;
    if (vt_widget.kind == CONSOLE_VT_WIDGET_COMBO) {
        vt_widget.selected_index = index;
    }
    const char* label = vt_widget.items[index];
    ConsoleVtWidgetKind kind = vt_widget.kind;
    ConsoleVtWidgetLeaveModal(false);
    if (kind == CONSOLE_VT_WIDGET_COMBO) {
        KTerm_WriteFormat(term, "\x1B[32mCombo:\x1B[0m %s \x1B[90m(index %d)\x1B[0m\n", label, index);
    } else {
        KTerm_WriteFormat(term, "\x1B[32mSelected:\x1B[0m %s \x1B[90m(index %d)\x1B[0m\n", label, index);
    }
}

static void ConsoleVtWidgetShowList(int row, int col, ConsoleVtWidgetKind kind,
                                    const char* const* items, int count,
                                    ConsoleVtBorderStyle border,
                                    const ConsoleVtStyle* item_style,
                                    const ConsoleVtStyle* highlight_style,
                                    const ConsoleVtStyle* border_style) {
    if (!term || !items || count <= 0 || count > 32) return;

    vt_widget.kind = kind;
    vt_widget.active = true;
    vt_widget.expanded = (kind != CONSOLE_VT_WIDGET_COMBO);
    vt_widget.origin_row = row;
    vt_widget.origin_col = col;
    vt_widget.item_count = count;
    vt_widget.highlight = 0;
    vt_widget.selected_index = 0;
    vt_widget.border = border;
    vt_widget.item_style = item_style ? *item_style : ConsoleVtStyleDefault();
    vt_widget.highlight_style = highlight_style ? *highlight_style : ConsoleVtStyleDefault();
    vt_widget.border_style = border_style ? *border_style : ConsoleVtStyleDefault();
    ConsoleVtWidgetApplyDefaultStyles();

    vt_widget.width = ConsoleVtMeasureItemsWidth(items, count);
    if (kind == CONSOLE_VT_WIDGET_COMBO) {
        if (vt_widget.width < 12) vt_widget.width = 12;
        vt_widget.height = 3;
    } else {
        vt_widget.height = count + 2;
    }

    for (int i = 0; i < count; i++) {
        ConsoleCopyString(vt_widget.items[i], sizeof(vt_widget.items[i]), items[i]);
    }

    ConsoleVtWidgetEnterModal();
    ConsoleVtWidgetRepaint();
}

static void ConsoleVtMenuShow(int row, int col, const char* const* items, int count,
                              ConsoleVtBorderStyle border, const ConsoleVtStyle* item_style,
                              const ConsoleVtStyle* highlight_style, const ConsoleVtStyle* border_style) {
    ConsoleVtWidgetShowList(row, col, CONSOLE_VT_WIDGET_MENU, items, count,
                            border, item_style, highlight_style, border_style);
}

static void ConsoleVtWidgetShowDialog(int row, int col, int width, const char* message,
                                      ConsoleVtBorderStyle border,
                                      const ConsoleVtStyle* item_style,
                                      const ConsoleVtStyle* highlight_style,
                                      const ConsoleVtStyle* border_style) {
    if (!term || !message) return;

    vt_widget.kind = CONSOLE_VT_WIDGET_DIALOG;
    vt_widget.active = true;
    vt_widget.expanded = true;
    vt_widget.origin_row = row;
    vt_widget.origin_col = col;
    vt_widget.width = width > 0 ? width : (int)strlen(message) + 8;
    vt_widget.height = 5;
    vt_widget.item_count = 0;
    vt_widget.highlight = 0;
    vt_widget.dialog_focus = 0;
    vt_widget.border = border;
    vt_widget.item_style = item_style ? *item_style : ConsoleVtStyleDefault();
    vt_widget.highlight_style = highlight_style ? *highlight_style : ConsoleVtStyleDefault();
    vt_widget.border_style = border_style ? *border_style : ConsoleVtStyleDefault();
    ConsoleCopyString(vt_widget.message, sizeof(vt_widget.message), message);
    ConsoleVtWidgetApplyDefaultStyles();

    ConsoleVtWidgetEnterModal();
    ConsoleVtWidgetRepaint();
}

static bool ConsoleVtParseSgrMouse(const char* data, size_t len, int* row, int* col, bool* pressed) {
    if (!data || len < 6 || data[0] != '\x1B' || data[1] != '[' || data[2] != '<') return false;

    int button = 0, x = 0, y = 0;
    const char* p = data + 3;
    if (sscanf(p, "%d;%d;%d", &button, &x, &y) < 3) return false;

    char suffix = data[len - 1];
    if (suffix != 'M' && suffix != 'm') return false;

    if (row) *row = y;
    if (col) *col = x;
    if (pressed) *pressed = (suffix == 'M');
    (void)button;
    return true;
}

static bool ConsoleVtWidgetHitTestList(int row, int col, int list_row, int list_count, int* out_index) {
    if (row < list_row + 1 || row > list_row + list_count ||
        col < vt_widget.origin_col + 1 || col > vt_widget.origin_col + vt_widget.width + 1) {
        return false;
    }
    if (out_index) *out_index = row - (list_row + 1);
    return true;
}

static bool ConsoleVtWidgetHandlePointer(int row, int col, bool pressed) {
    if (!vt_widget.active || !pressed) return true;

    if (vt_widget.kind == CONSOLE_VT_WIDGET_DIALOG) {
        int btn_row = vt_widget.origin_row + 2;
        if (row != btn_row) {
            ConsoleVtWidgetLeaveModal(true);
            return true;
        }
        int ok_col = vt_widget.origin_col + 2;
        int cancel_col = ok_col + 8;
        if (col >= cancel_col) vt_widget.dialog_focus = 1;
        else if (col >= ok_col) vt_widget.dialog_focus = 0;
        ConsoleVtWidgetConfirmSelection();
        return true;
    }

    if (vt_widget.kind == CONSOLE_VT_WIDGET_COMBO) {
        if (ConsoleVtWidgetHitTestList(row, col, vt_widget.origin_row, 1, NULL)) {
            if (!vt_widget.expanded) {
                vt_widget.expanded = true;
                vt_widget.highlight = vt_widget.selected_index;
                ConsoleVtWidgetRepaint();
                return true;
            }
        }
        if (vt_widget.expanded) {
            int list_row = vt_widget.origin_row + 3;
            int index = 0;
            if (ConsoleVtWidgetHitTestList(row, col, list_row, vt_widget.item_count, &index)) {
                vt_widget.highlight = index;
                ConsoleVtWidgetConfirmSelection();
                return true;
            }
            ConsoleVtWidgetLeaveModal(true);
            return true;
        }
    }

    if (vt_widget.kind == CONSOLE_VT_WIDGET_MENU) {
        int index = 0;
        if (ConsoleVtWidgetHitTestList(row, col, vt_widget.origin_row, vt_widget.item_count, &index)) {
            vt_widget.highlight = index;
            ConsoleVtWidgetConfirmSelection();
            return true;
        }
        ConsoleVtWidgetLeaveModal(true);
        return true;
    }

    ConsoleVtWidgetLeaveModal(true);
    return true;
}

static bool ConsoleVtWidgetHandleResponse(const char* response_data, size_t length) {
    if (!vt_widget.active || !response_data || length == 0) return false;

    int row = 0, col = 0;
    bool pressed = false;
    if (ConsoleVtParseSgrMouse(response_data, length, &row, &col, &pressed)) {
        return ConsoleVtWidgetHandlePointer(row, col, pressed);
    }

    if (length >= 3 && response_data[0] == '\x1B' && response_data[1] == '[') {
        if (response_data[2] == 'A' || response_data[2] == 'B' ||
            (length >= 4 && response_data[2] == '1' && (response_data[3] == 'A' || response_data[3] == 'B'))) {
            bool up = (response_data[length - 1] == 'A');
            if (vt_widget.kind == CONSOLE_VT_WIDGET_DIALOG) {
                (void)up;
                vt_widget.dialog_focus = up ? 0 : 1;
            } else if (vt_widget.kind == CONSOLE_VT_WIDGET_COMBO && !vt_widget.expanded) {
                if (!up) {
                    vt_widget.expanded = true;
                    vt_widget.highlight = vt_widget.selected_index;
                }
            } else {
                if (up) {
                    if (vt_widget.highlight > 0) vt_widget.highlight--;
                } else if (vt_widget.highlight < vt_widget.item_count - 1) {
                    vt_widget.highlight++;
                }
            }
            ConsoleVtWidgetRepaint();
            return true;
        }
        if (response_data[2] == 'C' || response_data[2] == 'D' ||
            (length >= 4 && response_data[2] == '1' && (response_data[3] == 'C' || response_data[3] == 'D'))) {
            if (vt_widget.kind == CONSOLE_VT_WIDGET_DIALOG) {
                bool left = (response_data[length - 1] == 'D');
                vt_widget.dialog_focus = left ? 0 : 1;
                ConsoleVtWidgetRepaint();
            }
            return true;
        }
    }

    if (length == 1 && response_data[0] == '\x1B') {
        if (vt_widget.kind == CONSOLE_VT_WIDGET_COMBO && vt_widget.expanded) {
            int list_row = vt_widget.origin_row + 3;
            ConsoleVtClearRegion(term, list_row, vt_widget.origin_col, vt_widget.width + 2, vt_widget.item_count + 2);
            vt_widget.expanded = false;
            ConsoleVtWidgetRepaintComboCollapsed();
        } else {
            ConsoleVtWidgetLeaveModal(true);
        }
        return true;
    }

    if (length == 1 && (response_data[0] == '\r' || response_data[0] == '\n')) {
        if (vt_widget.kind == CONSOLE_VT_WIDGET_COMBO && !vt_widget.expanded) {
            vt_widget.expanded = true;
            vt_widget.highlight = vt_widget.selected_index;
            ConsoleVtWidgetRepaint();
        } else {
            ConsoleVtWidgetConfirmSelection();
        }
        return true;
    }

    if (length == 1 && response_data[0] == ' ') {
        if (vt_widget.kind == CONSOLE_VT_WIDGET_COMBO && !vt_widget.expanded) {
            vt_widget.expanded = true;
            vt_widget.highlight = vt_widget.selected_index;
            ConsoleVtWidgetRepaint();
            return true;
        }
        if (vt_widget.kind == CONSOLE_VT_WIDGET_DIALOG) {
            ConsoleVtWidgetConfirmSelection();
            return true;
        }
    }

    if (length == 1 && response_data[0] == '\t') {
        if (vt_widget.kind == CONSOLE_VT_WIDGET_DIALOG) {
            vt_widget.dialog_focus = 1 - vt_widget.dialog_focus;
            ConsoleVtWidgetRepaint();
            return true;
        }
    }

    if (length == 1 && response_data[0] == 0x03) {
        ConsoleVtWidgetLeaveModal(true);
        return true;
    }

    return true;
}

static bool ConsoleVtMenuHandleResponse(const char* response_data, size_t length) {
    return ConsoleVtWidgetHandleResponse(response_data, length);
}

static void ConsoleVtDemoStyles(KTerm* t) {
    KTerm_WriteString(t, "\x1B[1;36mVT UI style samples\x1B[0m\n\n");

    ConsoleVtStyle border = ConsoleVtStyleDefault();
    border.fg_ansi = 6;
    border.bold = true;

    ConsoleVtBoxParams box = {0};
    box.row = 3;
    box.col = 4;
    box.width = 18;
    box.height = 3;
    box.border_style = CONSOLE_VT_BORDER_DEC_SINGLE;
    box.border = border;
    box.title_text = "DEC single";
    box.title_style = border;
    ConsoleVtDrawBox(t, &box);

    box.row = 3;
    box.col = 28;
    box.border_style = CONSOLE_VT_BORDER_UNICODE_LIGHT;
    box.title_text = "Unicode light";
    ConsoleVtDrawBox(t, &box);

    box.row = 3;
    box.col = 52;
    box.width = 16;
    box.border_style = CONSOLE_VT_BORDER_DEC_DOUBLE;
    box.title_text = "CP437 double";
    ConsoleVtDrawBox(t, &box);

    ConsoleVtStyle fill = ConsoleVtStyleDefault();
    fill.bg_256_mode = true;
    fill.bg_256 = 17;
    fill.fg_ansi = 15;
    box.row = 9;
    box.col = 4;
    box.width = 22;
    box.height = 2;
    box.border_style = CONSOLE_VT_BORDER_UNICODE_HEAVY;
    box.border.fg_ansi = 11;
    box.border.bold = true;
    box.fill_interior = true;
    box.fill = fill;
    box.title_text = "256-color fill";
    box.title_style = box.border;
    ConsoleVtDrawBox(t, &box);

    ConsoleVtStyle tc = ConsoleVtStyleDefault();
    tc.fg_truecolor = true;
    tc.fg_rgb[0] = 255;
    tc.fg_rgb[1] = 128;
    tc.fg_rgb[2] = 64;
    tc.bg_truecolor = true;
    tc.bg_rgb[0] = 32;
    tc.bg_rgb[1] = 16;
    tc.bg_rgb[2] = 48;
    box.row = 9;
    box.col = 32;
    box.width = 20;
    box.border_style = CONSOLE_VT_BORDER_ASCII;
    box.border = tc;
    box.fill = tc;
    box.title_text = "truecolor";
    box.title_style = tc;
    ConsoleVtDrawBox(t, &box);

    KTerm_WriteString(t, "\n\x1B[90mUse vt_box, vt_menu, vt_combo, vt_dialog for interactive widgets.\x1B[0m\n");
}

static void ConsoleVtRunBoxCommand(char* tokens[], int token_count) {
    int row = 5, col = 10, width = 30, height = 5;
    ConsoleVtBorderStyle border = CONSOLE_VT_BORDER_DEC_SINGLE;
    ConsoleVtStyle border_style = ConsoleVtStyleDefault();
    border_style.fg_ansi = 6;
    border_style.bold = true;
    ConsoleVtStyle fill = ConsoleVtStyleDefault();
    bool use_fill = false;
    const char* title = NULL;

    int argi = 1;
    if (token_count >= 5) {
        row = atoi(tokens[argi++]);
        col = atoi(tokens[argi++]);
        width = atoi(tokens[argi++]);
        height = atoi(tokens[argi++]);
    }

    for (; argi < token_count; argi++) {
        if (ConsoleVtIsBorderStyleName(tokens[argi])) {
            border = ConsoleVtParseBorderStyle(tokens[argi]);
            continue;
        }
        if (strcmp(tokens[argi], "fill") == 0) {
            use_fill = true;
            continue;
        }
        if (strncmp(tokens[argi], "title:", 6) == 0) {
            title = tokens[argi] + 6;
            continue;
        }
        if (!ConsoleVtParseStyleToken(tokens[argi], &border_style)) {
            if (!ConsoleVtParseStyleToken(tokens[argi], &fill)) {
                KTerm_WriteFormat(term, "\x1B[33mWarning: unknown style token '%s'\x1B[0m\n", tokens[argi]);
            } else {
                use_fill = true;
            }
        }
    }

    ConsoleVtBoxParams params = {0};
    params.row = row;
    params.col = col;
    params.width = width;
    params.height = height;
    params.border_style = border;
    params.border = border_style;
    params.title_text = title;
    params.title_style = border_style;
    params.fill_interior = use_fill;
    params.fill = fill;
    ConsoleVtDrawBox(term, &params);
    KTerm_WriteString(term, "\n");
}

static void ConsoleVtRunMenuCommand(char* tokens[], int token_count) {
    if (token_count < 4) {
        KTerm_WriteString(term,
            "\x1B[31mUsage: vt_menu <row> <col> <item> [item...]\x1B[0m\n"
            "\x1B[90mOptional trailing style tokens: dec unicode heavy ascii cyan bold inverse 256:N rgb:R,G,B\x1B[0m\n");
        return;
    }

    int row = atoi(tokens[1]);
    int col = atoi(tokens[2]);
    const char* items[32];
    int item_count = 0;
    ConsoleVtBorderStyle border = CONSOLE_VT_BORDER_DEC_SINGLE;
    ConsoleVtStyle item_style = ConsoleVtStyleDefault();
    ConsoleVtStyle highlight_style = ConsoleVtStyleDefault();
    ConsoleVtStyle border_style = ConsoleVtStyleDefault();

    for (int i = 3; i < token_count && item_count < 32; i++) {
        if (ConsoleVtIsBorderStyleName(tokens[i])) {
            border = ConsoleVtParseBorderStyle(tokens[i]);
            continue;
        }
        if (strncmp(tokens[i], "hi:", 3) == 0) {
            ConsoleVtParseStyleToken(tokens[i] + 3, &highlight_style);
            continue;
        }
        if (ConsoleVtParseStyleToken(tokens[i], &border_style)) continue;
        items[item_count++] = tokens[i];
    }

    if (item_count == 0) {
        KTerm_WriteString(term, "\x1B[31mError: vt_menu requires at least one item label\x1B[0m\n");
        return;
    }

    KTerm_WriteFormat(term, "\x1B[90mPulldown at %d,%d — arrows/Enter, mouse click, Esc cancel\x1B[0m\n", row, col);
    ConsoleVtMenuShow(row, col, items, item_count, border, &item_style, &highlight_style, &border_style);
}

static void ConsoleVtRunComboCommand(char* tokens[], int token_count) {
    if (token_count < 4) {
        KTerm_WriteString(term,
            "\x1B[31mUsage: vt_combo <row> <col> <item> [item...]\x1B[0m\n"
            "\x1B[90mCollapsed field with ▼ — Enter/Space/Down opens list; Esc collapses\x1B[0m\n");
        return;
    }

    int row = atoi(tokens[1]);
    int col = atoi(tokens[2]);
    const char* items[32];
    int item_count = 0;
    ConsoleVtBorderStyle border = CONSOLE_VT_BORDER_DEC_SINGLE;
    ConsoleVtStyle item_style = ConsoleVtStyleDefault();
    ConsoleVtStyle highlight_style = ConsoleVtStyleDefault();
    ConsoleVtStyle border_style = ConsoleVtStyleDefault();

    for (int i = 3; i < token_count && item_count < 32; i++) {
        if (ConsoleVtIsBorderStyleName(tokens[i])) {
            border = ConsoleVtParseBorderStyle(tokens[i]);
            continue;
        }
        if (strncmp(tokens[i], "hi:", 3) == 0) {
            ConsoleVtParseStyleToken(tokens[i] + 3, &highlight_style);
            continue;
        }
        if (ConsoleVtParseStyleToken(tokens[i], &border_style)) continue;
        items[item_count++] = tokens[i];
    }

    if (item_count == 0) {
        KTerm_WriteString(term, "\x1B[31mError: vt_combo requires at least one item label\x1B[0m\n");
        return;
    }

    KTerm_WriteFormat(term, "\x1B[90mCombo at %d,%d — Enter/Space/Down open, Esc collapse\x1B[0m\n", row, col);
    ConsoleVtWidgetShowList(row, col, CONSOLE_VT_WIDGET_COMBO, items, item_count,
                            border, &item_style, &highlight_style, &border_style);
}

static void ConsoleVtRunDialogCommand(char* tokens[], int token_count) {
    if (token_count < 4) {
        KTerm_WriteString(term,
            "\x1B[31mUsage: vt_dialog <row> <col> <width> <message>\x1B[0m\n"
            "\x1B[90mLeft/Right or Tab focus — Enter confirms OK/Cancel\x1B[0m\n");
        return;
    }

    int row = atoi(tokens[1]);
    int col = atoi(tokens[2]);
    int width = atoi(tokens[3]);
    char message[160];
    message[0] = '\0';
    for (int i = 4; i < token_count; i++) {
        if (message[0]) strncat(message, " ", sizeof(message) - strlen(message) - 1);
        strncat(message, tokens[i], sizeof(message) - strlen(message) - 1);
    }

    ConsoleVtStyle border_style = ConsoleVtStyleDefault();
    border_style.fg_ansi = 3;
    border_style.bold = true;
    ConsoleVtStyle highlight = ConsoleVtStyleDefault();
    highlight.bg_ansi = 4;
    highlight.bold = true;

    KTerm_WriteFormat(term, "\x1B[90mDialog at %d,%d\x1B[0m\n", row, col);
    ConsoleVtWidgetShowDialog(row, col, width, message, CONSOLE_VT_BORDER_UNICODE_LIGHT,
                              &border_style, &highlight, &border_style);
}

static void ConsoleVtRunStylesCaptureCommand(void) {
    if (!capture_path) {
        KTerm_WriteString(term,
            "\x1B[33mSet KTERM_CAPTURE_SCREENSHOT=<path> before vt_styles_capture\x1B[0m\n"
            "\x1B[90mOptional: KTERM_CAPTURE_EXIT=1 to exit after capture\x1B[0m\n");
        return;
    }
    capture_frame = 0;
    KTerm_WriteString(term, "\x1B[2J\x1B[H");
    ConsoleVtDemoStyles(term);
    KTerm_WriteString(term, "\x1B[90mScreenshot scheduled (~0.5s at 60 FPS)\x1B[0m\n");
}

#endif /* CONSOLE_VT_UI_H */
