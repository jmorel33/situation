#ifndef CONSOLE_EDITOR_H
#define CONSOLE_EDITOR_H

/* Phase 0 spike: chrome + scroll region without full editor. */
static bool editor_test_mode = false;
static bool editor_test_waiting_cpr = false;

static void EditorTestPaintChrome(void) {
    KTermSession* session = GET_SESSION(term);
    int rows = session->rows;
    if (rows < 3) {
        rows = 3;
    }

    /* Status bar (row 1) — protected */
    KTerm_WriteFormat(term, "\x1B[1;1H\x1B[0 q\x1B[7m Phase 0 edit_test \x1B[0m  rows=%d  F8=exit  F9=CPR \x1B[0m", rows);
    KTerm_WriteString(term, "\x1B[1 q");

    /* Function bar (last row) — protected */
    KTerm_WriteFormat(term, "\x1B[%d;1H\x1B[0 q\x1B[90m F1..F7 reserved  F8 exit  type in rows 2..%d \x1B[0m", rows, rows - 1);
    KTerm_WriteString(term, "\x1B[1 q");

    /* Scroll region: rows 2 .. rows-1 */
    KTerm_WriteFormat(term, "\x1B[2;%dr", rows - 1);
    KTerm_WriteString(term, "\x1B[2;1H");
}

static void EditorTestEnter(void) {
    KTermSession* session = GET_SESSION(term);

    editor_test_mode = true;
    editor_test_waiting_cpr = false;
    console.prompt_pending = false;
    console.input_enabled = false;
    console.in_command = false;
    console.waiting_for_prompt_cursor_pos = false;
    ClearEditBuffer();

    KTerm_WriteString(term, "\x1B[2J\x1B[H");
    EditorTestPaintChrome();

    /* Local echo so keystrokes land in the scroll region, not the CLI line editor. */
    KTerm_WriteString(term, "\x1B[?12h");

    KTerm_WriteString(term, "\x1B[36mEdit-test active.\x1B[0m Type in the middle rows; top/bottom chrome should stay protected.\n");
    KTerm_WriteString(term, "\x1B[2;1H");

    (void)session;
}

static void EditorTestExit(void) {
    if (!editor_test_mode) {
        return;
    }

    editor_test_mode = false;
    editor_test_waiting_cpr = false;
    cursor_tracker.waiting_for_position = false;
    cursor_tracker.position_received = false;

    KTermSession* session = GET_SESSION(term);
    int rows = session->rows;

    KTerm_WriteString(term, "\x1B[?12l");
    KTerm_WriteFormat(term, "\x1B[1;%dr", rows);
    KTerm_WriteString(term, "\x1B[0 q");
    KTerm_WriteString(term, "\n\x1B[33mEdit-test exited.\x1B[0m\n");

    console.prompt_pending = true;
    console.input_enabled = false;
}

static void EditorTestRequestCpr(void) {
    if (!editor_test_mode) {
        KTerm_WriteString(term, "\x1B[31mError: run 'edit_test' first\x1B[0m\n");
        return;
    }

    editor_test_waiting_cpr = true;
    cursor_tracker.waiting_for_position = true;
    cursor_tracker.position_received = false;
    KTerm_WriteString(term, "\x1B[6n");
}

static void EditorTestUpdateStatusCpr(int row, int col) {
    KTermSession* session = GET_SESSION(term);
    int rows = session->rows;

    KTerm_WriteFormat(term, "\x1B[1;1H\x1B[0 q\x1B[7m CPR: row=%d col=%d \x1B[0m  rows=%d  F8=exit \x1B[0m", row, col, rows);
    KTerm_WriteString(term, "\x1B[1 q");
    KTerm_WriteFormat(term, "\x1B[2;1H");
}

static bool EditorTestHandleResponse(const char* response_data, size_t length) {
    if (!editor_test_mode) {
        return false;
    }

    const char* current_pos = response_data;
    int remaining_length = (int)length;

    while (remaining_length > 0) {
        int consumed_bytes = 0;

        if (editor_test_waiting_cpr && remaining_length >= 3 &&
            current_pos[0] == '\x1B' && current_pos[1] == '[') {
            const char* r_char = (const char*)memchr(current_pos, 'R', remaining_length);
            if (r_char != NULL && (r_char - current_pos) < remaining_length) {
                int cpr_len = (int)((r_char - current_pos) + 1);
                if (ParseCSIResponse(current_pos, (size_t)cpr_len)) {
                    if (cursor_tracker.position_received) {
                        EditorTestUpdateStatusCpr(cursor_tracker.row, cursor_tracker.col);
                        editor_test_waiting_cpr = false;
                        cursor_tracker.position_received = false;
                        consumed_bytes = cpr_len;
                    }
                }
            }
        }

        if (consumed_bytes == 0 && remaining_length >= 4 &&
            current_pos[0] == '\x1B' && current_pos[1] == '[' &&
            current_pos[2] == '1' && current_pos[3] == '9' &&
            (remaining_length < 5 || current_pos[4] == '~')) {
            EditorTestExit();
            return true;
        }

        if (consumed_bytes == 0 && remaining_length >= 4 &&
            current_pos[0] == '\x1B' && current_pos[1] == '[' &&
            current_pos[2] == '2' && current_pos[3] == '0' &&
            (remaining_length < 5 || current_pos[4] == '~')) {
            EditorTestRequestCpr();
            return true;
        }

        if (consumed_bytes == 0 && (unsigned char)*current_pos == 0x03) {
            EditorTestExit();
            return true;
        }

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
            consumed_bytes = 1;
        }

        current_pos += consumed_bytes;
        remaining_length -= consumed_bytes;
    }

    return true;
}

#endif /* CONSOLE_EDITOR_H */
