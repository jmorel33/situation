#ifndef CONSOLE_RESPONSE_H
#define CONSOLE_RESPONSE_H

/*
 * HandleKTermResponse routing (Phase 0):
 *   shell_mode       -> pass-through to KTShell (no CLI, no editor).
 *   editor_test_mode -> EditorTestHandleResponse (local echo + chrome; no CLI line editor).
 *   default          -> CLI prompt CPR + line editor via HandleKeyEvent.
 */

static void HandleTitleChange(KTerm* t, const char* title, bool is_icon) {
    (void)t;
    if (!is_icon && title && title[0]) {
        char buf[256];
        snprintf(buf, sizeof(buf), "KaOS - %s", title);
        SituationSetWindowTitle(buf);
    }
}

static void HandleKTermResponse(void* ctx, KTermSession* session, const char* response_data, size_t length) {
    KTerm* term = (KTerm*)ctx;

    if (shell_mode) {
        KTShell_Write(&shell_proc, response_data, length);
        return;
    }

    if (EditorHandleResponse(response_data, length)) {
        return;
    }

    if (ConsoleVtMenuHandleResponse(response_data, length)) {
        return;
    }

    const char* current_pos = response_data;
    int remaining_length = length;

    while (remaining_length > 0) {
        int consumed_bytes = 0;

        if (console.waiting_for_prompt_cursor_pos) {
            if (remaining_length >= 3 && current_pos[0] == '\x1B' && current_pos[1] == '[') {
                const char* r_char = (const char*)memchr(current_pos, 'R', remaining_length);
                if (r_char != NULL && (r_char - current_pos < remaining_length)) {
                    int cpr_len = (r_char - current_pos) + 1;
                    if (ParseCSIResponse(current_pos, cpr_len)) {
                        if (cursor_tracker.position_received) {
                            console.prompt_line_y = cursor_tracker.row;
                            console.prompt_start_x = cursor_tracker.col;
                            console.waiting_for_prompt_cursor_pos = false;
                            console.input_enabled = true;
                            cursor_tracker.position_received = false;
                            RedrawEditLine();
                            consumed_bytes = cpr_len;
                        }
                    }
                }
            }
        }

        if (consumed_bytes == 0 && remaining_length >= 3 && current_pos[0] == '\x1B' && current_pos[1] == '[') {
            const char* c_char = (const char*)memchr(current_pos, 'c', remaining_length);

            if (c_char != NULL && (c_char - current_pos < remaining_length) &&
                (current_pos[2] == '?' || current_pos[2] == '>' || current_pos[2] == '=' || current_pos[2] >= '0' && current_pos[2] <='9')) {
                int da_len = (c_char - current_pos) + 1;
                KTerm_WriteString(term, "\n\x1B[36mKTerm DA:\x1B[0m ");
                for(int i=0; i<da_len; ++i) {
                     if (current_pos[i] >= 32 && current_pos[i] < 127) KTerm_WriteChar(term, current_pos[i]);
                     else if (current_pos[i] == '\x1B') KTerm_WriteString(term, "ESC");
                     else KTerm_WriteFormat(term, "[%02X]", (unsigned char)current_pos[i]);
                }
                KTerm_WriteString(term, "\n");
                if (!console.waiting_for_prompt_cursor_pos) {
                    console.prompt_pending = true;
                    console.input_enabled = false;
                }
                consumed_bytes = da_len;
            }
        }

        if (remaining_length > 2 && current_pos[0] == '\x1B' && current_pos[1] == '[') {
            if (ConsoleVtWidgetIsActive()) {
                /* Widget handler above consumes keyboard/mouse for active overlays. */
            } else if (current_pos[2] == 'M' || (current_pos[2] == '<' && strchr(current_pos, 'M') != NULL) || (current_pos[2] == '<' && strchr(current_pos, 'm') != NULL) ) {
                consumed_bytes = remaining_length;
            }
        }

        if (consumed_bytes == 0) {
            if ((unsigned char)*current_pos == 0x1B) {
                int skip = 1;
                if (skip < remaining_length && current_pos[skip] == '[') {
                    skip++;
                    while (skip < remaining_length && (unsigned char)current_pos[skip] >= 0x30 && (unsigned char)current_pos[skip] <= 0x3F) {
                        skip++;
                    }
                    while (skip < remaining_length && (unsigned char)current_pos[skip] >= 0x20 && (unsigned char)current_pos[skip] <= 0x2F) {
                        skip++;
                    }
                    if (skip < remaining_length && (unsigned char)current_pos[skip] >= 0x40 && (unsigned char)current_pos[skip] <= 0x7E) {
                        skip++;
                    }
                } else if (skip < remaining_length && current_pos[skip] == 'O') {
                    skip++;
                    if (skip < remaining_length) skip++;
                } else if (skip < remaining_length) {
                    skip++;
                }
                consumed_bytes = skip;
            } else if ((unsigned char)*current_pos == 0x0D || (unsigned char)*current_pos == 0x0A) {
                HandleKeyEvent("\r", 1);
                consumed_bytes = 1;
            } else if ((unsigned char)*current_pos == 0x08 || (unsigned char)*current_pos == 0x7F) {
                HandleKeyEvent("\x08", 1);
                consumed_bytes = 1;
            } else if ((unsigned char)*current_pos == 0x09) {
                HandleKeyEvent("\t", 1);
                consumed_bytes = 1;
            } else if ((unsigned char)*current_pos < 0x20) {
                HandleKeyEvent(current_pos, 1);
                consumed_bytes = 1;
            } else if ((unsigned char)*current_pos > 0x7E) {
                consumed_bytes = 1;
            } else {
                HandleKeyEvent(current_pos, 1);
                consumed_bytes = 1;
            }
        }

        current_pos += consumed_bytes;
        remaining_length -= consumed_bytes;
    }
}

#endif /* CONSOLE_RESPONSE_H */
