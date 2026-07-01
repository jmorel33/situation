#ifndef CONSOLE_CLI_INPUT_H
#define CONSOLE_CLI_INPUT_H

static void HandleExtendedKeyInput(const char* sequence) {
    if (strcmp(sequence, "\x1B[A") == 0 || strcmp(sequence, "\x1BOA") == 0) NavigateHistory(-1);
    else if (strcmp(sequence, "\x1B[B") == 0 || strcmp(sequence, "\x1BOB") == 0) NavigateHistory(1);
    else if (strcmp(sequence, "\x1B[D") == 0 || strcmp(sequence, "\x1BOD") == 0) { if (console.edit_pos > 0) { console.edit_pos--; RedrawEditLine(); } }
    else if (strcmp(sequence, "\x1B[C") == 0 || strcmp(sequence, "\x1BOC") == 0) { if (console.edit_pos < console.edit_length) { console.edit_pos++; RedrawEditLine(); } }
    else if (strcmp(sequence, "\x1B[H") == 0) { console.edit_pos = 0; RedrawEditLine(); }
    else if (strcmp(sequence, "\x1B[F") == 0) { console.edit_pos = console.edit_length; RedrawEditLine(); }
    else if (strcmp(sequence, "\x1B[3~") == 0) {
        if (console.edit_pos < console.edit_length) {
            memmove(&console.edit_buffer[console.edit_pos],
                    &console.edit_buffer[console.edit_pos + 1],
                    console.edit_length - console.edit_pos);
            console.edit_length--;
            console.edit_buffer[console.edit_length] = '\0';
            RedrawEditLine();
        }
    }
}

static void HandleEnterKey(void) {
    if (!console.input_enabled) return;
    KTerm_WriteChar(term, '\n');
    if (console.edit_length > 0) {
        AddToHistory(console.edit_buffer);
        ConsoleCopyString(console.command_buffer, sizeof(console.command_buffer), console.edit_buffer);
        ClearEditBuffer();
        console.input_enabled = false;
        console.in_command = true;
        ProcessCommand(console.command_buffer);
    } else {
        console.prompt_pending = true;
        console.input_enabled = false;
    }
}

static void HandleKeyEvent(const char* sequence, size_t length) {
    if (!(console.input_enabled && !console.in_command) &&
        !(length == 1 && sequence[0] == 0x03)) {
        return;
    }

    if (length == 1 && (sequence[0] == '\r' || sequence[0] == '\n')) {
        HandleEnterKey();
        return;
    }
    if (length == 1 && (sequence[0] == '\b' || sequence[0] == 0x7F)) {
        HandleBackspaceKey();
        return;
    }
    if (length == 1 && sequence[0] == '\t') {
        HandleTabKey();
        return;
    }

    if (length == 1 && sequence[0] >= 0x01 && sequence[0] <= 0x1A) {
        int ctrl_char_code = sequence[0];
        switch (ctrl_char_code) {
            case 0x01:
                if (console.input_enabled) {
                    console.edit_pos = 0;
                    RedrawEditLine();
                }
                break;
            case 0x02:
                if (console.input_enabled && console.edit_pos > 0) {
                    console.edit_pos--;
                    RedrawEditLine();
                }
                break;
            case 0x03:
                KTerm_WriteChar(term, '^');
                KTerm_WriteChar(term, 'C');
                KTerm_WriteChar(term, '\n');
                ClearEditBuffer();
                console.in_command = false;
                console.waiting_for_prompt_cursor_pos = false;
                console.prompt_pending = true;
                console.input_enabled = false;
                break;
            case 0x04:
                if (console.edit_length == 0) {
                    ProcessCommand("exit");
                } else if (console.edit_pos < console.edit_length) {
                    for (int i = console.edit_pos; i < console.edit_length - 1; i++) {
                        console.edit_buffer[i] = console.edit_buffer[i + 1];
                    }
                    console.edit_length--;
                    console.edit_buffer[console.edit_length] = '\0';
                    RedrawEditLine();
                }
                break;
            case 0x05:
                console.edit_pos = console.edit_length;
                RedrawEditLine();
                break;
            case 0x06:
                if (console.edit_pos < console.edit_length) {
                    console.edit_pos++;
                    RedrawEditLine();
                }
                break;
            case 0x08:
                HandleBackspaceKey();
                break;
            case 0x0A:
                GET_SESSION(term)->cursor.y++;
                if (GET_SESSION(term)->cursor.y > GET_SESSION(term)->scroll_bottom) {
                    GET_SESSION(term)->cursor.y = GET_SESSION(term)->scroll_bottom;
                    KTerm_ScrollUpRegion(term, GET_SESSION(term)->scroll_top, GET_SESSION(term)->scroll_bottom, 1);
                }
                if (GET_SESSION(term)->ansi_modes.line_feed_new_line) {
                    GET_SESSION(term)->cursor.x = GET_SESSION(term)->left_margin;
                }
                break;
            case 0x0B:
                console.edit_length = console.edit_pos;
                console.edit_buffer[console.edit_length] = '\0';
                RedrawEditLine();
                break;
            case 0x0C:
                KTerm_WriteString(term, "\x1B[2J\x1B[H");
                console.prompt_pending = true;
                console.input_enabled = false;
                console.waiting_for_prompt_cursor_pos = false;
                break;
            case 0x0E:
                NavigateHistory(1);
                break;
            case 0x10:
                NavigateHistory(-1);
                break;
            case 0x15:
                ClearEditBuffer();
                RedrawEditLine();
                break;
            case 0x17:
                if (console.edit_pos == 0) break;
                {
                    int end_pos = console.edit_pos;
                    int start_pos = console.edit_pos;
                    while (start_pos > 0 && console.edit_buffer[start_pos - 1] == ' ') start_pos--;
                    while (start_pos > 0 && console.edit_buffer[start_pos - 1] != ' ') start_pos--;
                    int num_to_delete = end_pos - start_pos;
                    if (num_to_delete > 0) {
                        memmove(&console.edit_buffer[start_pos],
                                &console.edit_buffer[end_pos],
                                console.edit_length - end_pos + 1);
                        console.edit_length -= num_to_delete;
                        console.edit_pos = start_pos;
                        RedrawEditLine();
                    }
                }
                break;
            default:
                break;
        }
        return;
    }

    if (length > 1 && sequence[0] == '\x1B') {
        if (!GET_SESSION(term)->raw_mode) {
            HandleExtendedKeyInput(sequence);
        }
        return;
    }

    if (length == 1 && sequence[0] >= 32) {
        HandlePrintableKey(sequence[0]);
    }
}

#endif /* CONSOLE_CLI_INPUT_H */
