/**********************************************************************************************
 *
 * @file console_ed.h
 *   (c) 2025-2026 Jacques Morel
 * @brief Embedded terminal text editor inside the KaOS Terminal Console
 *
 **********************************************************************************************/
#ifndef CONSOLE_ED_H
#define CONSOLE_ED_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    bool active;
    bool dirty;
    bool new_file;
    char filepath[512];
    int edit_top_row;      // 2 (1-based)
    int edit_bottom_row;   // rows - 1 (1-based)
    char* undo_buffer;
    size_t undo_buffer_size;
} ConsoleEditor;

static ConsoleEditor editor = {0};

static void EditorPaintChrome(void) {
    KTermSession* session = GET_SESSION(term);
    int rows = session->rows;
    int cols = session->cols;

    // Status bar (row 1) — Bold Cyan Inverse, Protected
    KTerm_WriteString(term, "\x1B[1;1H\x1B[0 q\x1B[1;36;7m");
    
    char status_text[256];
    snprintf(status_text, sizeof(status_text), " KaOS Editor %s File: %s %s | Cursor: %d:%d ",
             editor.dirty ? "*" : " ",
             editor.filepath[0] ? editor.filepath : "[No Name]",
             editor.new_file ? "[NEW]" : "",
             GET_SESSION(term)->cursor.y + 1, GET_SESSION(term)->cursor.x + 1);
             
    KTerm_WriteString(term, status_text);
    
    int text_len = (int)strlen(status_text);
    if (text_len < cols) {
        for (int i = 0; i < cols - text_len; i++) {
            KTerm_WriteChar(term, ' ');
        }
    }
    KTerm_WriteString(term, "\x1B[0m");
    KTerm_WriteString(term, "\x1B[1 q"); // Protect row 1

    // Function bar (last row) — Protected
    KTerm_WriteFormat(term, "\x1B[%d;1H\x1B[0 q\x1B[7m F1 Save \x1B[0m\x1B[7m F2 Undo \x1B[0m\x1B[7m F8 Exit \x1B[0m", rows);
    
    KTerm_WriteString(term, "\x1B[7m");
    int fn_len = 8 * 3; // Approx
    if (fn_len < cols) {
        for (int i = 0; i < cols - fn_len; i++) {
            KTerm_WriteChar(term, ' ');
        }
    }
    KTerm_WriteString(term, "\x1B[0m");
    KTerm_WriteString(term, "\x1B[1 q"); // Protect last row
}

static void EditorUpdateStatusBar(void) {
    if (!editor.active) return;
    KTermSession* session = GET_SESSION(term);
    int cols = session->cols;

    // Save cursor position
    KTerm_WriteString(term, "\x1B[s");

    // Move to row 1, set unprotected temporarily to overwrite status text
    KTerm_WriteString(term, "\x1B[1;1H\x1B[2 q\x1B[1;36;7m");
    
    char status_text[256];
    snprintf(status_text, sizeof(status_text), " KaOS Editor %s File: %s %s | Cursor: %d:%d ",
             editor.dirty ? "*" : " ",
             editor.filepath[0] ? editor.filepath : "[No Name]",
             editor.new_file ? "[NEW]" : "",
             GET_SESSION(term)->cursor.y + 1, GET_SESSION(term)->cursor.x + 1);
             
    KTerm_WriteString(term, status_text);
    
    int text_len = (int)strlen(status_text);
    if (text_len < cols) {
        for (int i = 0; i < cols - text_len; i++) {
            KTerm_WriteChar(term, ' ');
        }
    }
    KTerm_WriteString(term, "\x1B[0m");
    KTerm_WriteString(term, "\x1B[1 q"); // Protect row 1 again

    // Restore cursor position
    KTerm_WriteString(term, "\x1B[u");
}

static void EditorEnter(const char* filepath, bool is_new) {
    KTermSession* session = GET_SESSION(term);
    editor.active = true;
    editor.dirty = false;
    editor.new_file = is_new;
    
    if (filepath) {
        strncpy(editor.filepath, filepath, sizeof(editor.filepath) - 1);
    } else {
        editor.filepath[0] = '\0';
    }

    editor.edit_top_row = 2;
    editor.edit_bottom_row = session->rows - 1;

    // Clear screen
    KTerm_WriteString(term, "\x1B[2J\x1B[H");

    // Enable local echo
    KTerm_WriteString(term, "\x1B[?12h");

    // Paint chrome
    EditorPaintChrome();

    // Set scroll margins
    KTerm_WriteFormat(term, "\x1B[%d;%dr", editor.edit_top_row, editor.edit_bottom_row);
    // Origin Mode on
    KTerm_WriteString(term, "\x1B[?6h");
    // Autowrap on
    KTerm_WriteString(term, "\x1B[?7h");

    // Move cursor to top-left of scroll region
    KTerm_WriteString(term, "\x1B[H");

    // Load file if not new
    if (!is_new && filepath && filepath[0]) {
        FILE* f = fopen(filepath, "r");
        if (f) {
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fseek(f, 0, SEEK_SET);

            if (sz > 0) {
                editor.undo_buffer = (char*)malloc(sz + 1);
                size_t read_bytes = fread(editor.undo_buffer, 1, sz, f);
                editor.undo_buffer[read_bytes] = '\0';
                editor.undo_buffer_size = read_bytes;

                fseek(f, 0, SEEK_SET);
                char line[512];
                while (fgets(line, sizeof(line), f)) {
                    size_t len = strlen(line);
                    while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n')) {
                        line[len - 1] = '\0';
                        len--;
                    }
                    KTerm_WriteString(term, line);
                    KTerm_WriteString(term, "\r\n");
                }
            } else {
                editor.undo_buffer = (char*)malloc(1);
                editor.undo_buffer[0] = '\0';
                editor.undo_buffer_size = 0;
            }
            fclose(f);
        } else {
            KTerm_WriteFormat(term, "\x1B[31mError: could not open file '%s'\x1B[0m\n", filepath);
            editor.new_file = true;
            editor.undo_buffer = (char*)malloc(1);
            editor.undo_buffer[0] = '\0';
            editor.undo_buffer_size = 0;
        }
    } else {
        editor.undo_buffer = (char*)malloc(1);
        editor.undo_buffer[0] = '\0';
        editor.undo_buffer_size = 0;
    }

    // Set cursor to start
    KTerm_WriteString(term, "\x1B[H");

    console.prompt_pending = false;
    console.input_enabled = false;
    console.in_command = false;
    console.waiting_for_prompt_cursor_pos = false;

    EditorUpdateStatusBar();
}

static void EditorRelayout(void) {
    if (!editor.active) return;
    KTermSession* session = GET_SESSION(term);
    editor.edit_bottom_row = session->rows - 1;

    KTerm_WriteString(term, "\x1B[s"); // Save cursor
    KTerm_WriteString(term, "\x1B[r"); // Reset scroll margins temporarily
    KTerm_WriteString(term, "\x1B[?6l"); // Origin Mode off

    EditorPaintChrome();

    KTerm_WriteFormat(term, "\x1B[%d;%dr", editor.edit_top_row, editor.edit_bottom_row);
    KTerm_WriteString(term, "\x1B[?6h"); // Origin Mode on
    KTerm_WriteString(term, "\x1B[u"); // Restore cursor
    
    EditorUpdateStatusBar();
}

static char* EditorExtractRegion(size_t* out_len) {
    KTermSession* session = GET_SESSION(term);
    int cols = session->cols;
    int edit_height = editor.edit_bottom_row - editor.edit_top_row + 1;

    size_t cap = edit_height * (cols + 2) + 1;
    char* buf = (char*)malloc(cap);
    size_t len = 0;

    for (int y = editor.edit_top_row - 1; y < editor.edit_bottom_row; y++) {
        int last_non_space = -1;
        for (int x = cols - 1; x >= 0; x--) {
            EnhancedTermChar* cell = KTerm_GetCell(term, x, y);
            unsigned int ch = cell ? cell->ch : 0;
            if (ch != 0 && ch != ' ' && ch != '\t') {
                last_non_space = x;
                break;
            }
        }

        for (int x = 0; x <= last_non_space; x++) {
            EnhancedTermChar* cell = KTerm_GetCell(term, x, y);
            unsigned int ch = cell ? cell->ch : ' ';
            if (ch >= 32 && ch < 127) {
                buf[len++] = (char)ch;
            } else if (ch == '\t') {
                buf[len++] = '\t';
            } else {
                buf[len++] = ' ';
            }
        }
        buf[len++] = '\n';
    }
    buf[len] = '\0';
    if (out_len) *out_len = len;
    return buf;
}

static void EditorSave(void) {
    if (!editor.active || !editor.filepath[0]) return;

    size_t len = 0;
    char* doc_text = EditorExtractRegion(&len);

    FILE* f = fopen(editor.filepath, "w");
    if (f) {
        fwrite(doc_text, 1, len, f);
        fclose(f);

        if (editor.undo_buffer) {
            free(editor.undo_buffer);
        }
        editor.undo_buffer = doc_text;
        editor.undo_buffer_size = len;

        editor.dirty = false;
        editor.new_file = false;

        KTerm_WriteString(term, "\x1B[s"); // Save cursor
        KTerm_WriteFormat(term, "\x1B[%d;1H\x1B[0 q\x1B[32;7m Saved: %s (%zu chars) \x1B[0m", GET_SESSION(term)->rows, editor.filepath, len);
        KTerm_WriteString(term, "\x1B[1 q\x1B[u"); // restore
        
        EditorUpdateStatusBar();
    } else {
        free(doc_text);
        KTerm_WriteString(term, "\x1B[s"); // Save cursor
        KTerm_WriteFormat(term, "\x1B[%d;1H\x1B[0 q\x1B[31;7m Failed to save: %s \x1B[0m", GET_SESSION(term)->rows, editor.filepath);
        KTerm_WriteString(term, "\x1B[1 q\x1B[u"); // restore
    }
}

static void EditorUndo(void) {
    if (!editor.active) return;

    KTerm_WriteString(term, "\x1B[H\x1B[J"); // clear edit region

    if (editor.undo_buffer && editor.undo_buffer_size > 0) {
        const char* p = editor.undo_buffer;
        char line[512];
        size_t offset = 0;
        while (offset < editor.undo_buffer_size) {
            size_t line_len = 0;
            while (offset + line_len < editor.undo_buffer_size && p[offset + line_len] != '\n') {
                line_len++;
            }
            size_t copy_len = line_len < sizeof(line) - 1 ? line_len : sizeof(line) - 1;
            memcpy(line, &p[offset], copy_len);
            line[copy_len] = '\0';
            
            KTerm_WriteString(term, line);
            KTerm_WriteString(term, "\r\n");
            
            offset += line_len + 1;
        }
    }

    KTerm_WriteString(term, "\x1B[H"); // home
    editor.dirty = false;
    
    KTerm_WriteString(term, "\x1B[s"); // Save cursor
    KTerm_WriteFormat(term, "\x1B[%d;1H\x1B[0 q\x1B[33;7m Restored to last save \x1B[0m", GET_SESSION(term)->rows);
    KTerm_WriteString(term, "\x1B[1 q\x1B[u"); // restore

    EditorUpdateStatusBar();
}

static void EditorExit(void) {
    if (!editor.active) return;

    editor.active = false;
    if (editor.undo_buffer) {
        free(editor.undo_buffer);
        editor.undo_buffer = NULL;
    }
    editor.undo_buffer_size = 0;

    KTerm_WriteString(term, "\x1B[?12l"); // Disable local echo
    KTerm_WriteString(term, "\x1B[r"); // Reset scroll margins
    KTerm_WriteString(term, "\x1B[?6l"); // Origin Mode off
    KTerm_WriteString(term, "\x1B[0 q"); // Clear protection
    KTerm_WriteString(term, "\x1B[2J\x1B[H"); // Clear screen

    KTerm_WriteString(term, "\x1B[33mEditor exited.\x1B[0m\n");

    console.prompt_pending = true;
    console.input_enabled = false;
    console.in_command = false;
}

static bool EditorProcessInput(const char* response_data, size_t length) {
    (void)response_data;
    (void)length;
    return false; // let character data fall through to K-Term
}

static bool EditorHandleResponse(const char* response_data, size_t length) {
    if (!editor.active) {
        return false;
    }

    const char* current_pos = response_data;
    int remaining_length = (int)length;

    while (remaining_length > 0) {
        int consumed_bytes = 0;

        // Check for F1 (Save)
        if (consumed_bytes == 0 && remaining_length >= 4 &&
            current_pos[0] == '\x1B' && current_pos[1] == '[' &&
            current_pos[2] == '1' && current_pos[3] == '1' &&
            (remaining_length < 5 || current_pos[4] == '~')) {
            EditorSave();
            return true;
        }
        if (consumed_bytes == 0 && remaining_length >= 3 &&
            current_pos[0] == '\x1B' && current_pos[1] == 'O' && current_pos[2] == 'P') {
            EditorSave();
            return true;
        }

        // Check for F2 (Undo)
        if (consumed_bytes == 0 && remaining_length >= 4 &&
            current_pos[0] == '\x1B' && current_pos[1] == '[' &&
            current_pos[2] == '1' && current_pos[3] == '2' &&
            (remaining_length < 5 || current_pos[4] == '~')) {
            EditorUndo();
            return true;
        }
        if (consumed_bytes == 0 && remaining_length >= 3 &&
            current_pos[0] == '\x1B' && current_pos[1] == 'O' && current_pos[2] == 'Q') {
            EditorUndo();
            return true;
        }

        // Check for F8 (Exit)
        if (consumed_bytes == 0 && remaining_length >= 4 &&
            current_pos[0] == '\x1B' && current_pos[1] == '[' &&
            current_pos[2] == '1' && current_pos[3] == '9' &&
            (remaining_length < 5 || current_pos[4] == '~')) {
            EditorExit();
            return true;
        }

        // Check for CPR: \x1B[{row};{col}R
        if (consumed_bytes == 0 && remaining_length >= 3 &&
            current_pos[0] == '\x1B' && current_pos[1] == '[') {
            const char* r_char = (const char*)memchr(current_pos, 'R', remaining_length);
            if (r_char != NULL && (r_char - current_pos) < remaining_length) {
                consumed_bytes = (int)((r_char - current_pos) + 1);
            }
        }

        // Check for Ctrl-C (0x03) -> Exit
        if (consumed_bytes == 0 && (unsigned char)*current_pos == 0x03) {
            EditorExit();
            return true;
        }

        // Skip escape sequence
        if (consumed_bytes == 0 && (unsigned char)*current_pos == 0x1B) {
            int skip = 1;
            if (skip < remaining_length && current_pos[skip] == '[') {
                skip++;
                while (skip < remaining_length &&
                       (unsigned char)current_pos[skip] >= 0x30 &&
                       (unsigned char)current_pos[skip] <= 0x3F) {
                    skip++;
                }
                while (skip < remaining_length &&
                       (unsigned char)current_pos[skip] >= 0x20 &&
                       (unsigned char)current_pos[skip] <= 0x2F) {
                    skip++;
                }
                if (skip < remaining_length &&
                    (unsigned char)current_pos[skip] >= 0x40 &&
                    (unsigned char)current_pos[skip] <= 0x7E) {
                    skip++;
                }
            } else if (skip < remaining_length && current_pos[skip] == 'O') {
                skip++;
                if (skip < remaining_length) {
                    skip++;
                }
            } else if (skip < remaining_length) {
                skip++;
            }
            consumed_bytes = skip;
        } else if (consumed_bytes == 0) {
            unsigned char c = (unsigned char)*current_pos;
            if (c >= 32 || c == '\r' || c == '\n' || c == '\b' || c == 0x7F) {
                if (!editor.dirty) {
                    editor.dirty = true;
                }
            }
            consumed_bytes = 1;
        }

        current_pos += consumed_bytes;
        remaining_length -= consumed_bytes;
    }

    EditorUpdateStatusBar();
    return true;
}

#endif /* CONSOLE_ED_H */
