#ifndef CONSOLE_CLI_EDIT_H
#define CONSOLE_CLI_EDIT_H

// Command line editing functions
static void ClearEditBuffer(void) {
    memset(console.edit_buffer, 0, sizeof(console.edit_buffer));
    console.edit_pos = 0;
    console.edit_length = 0;
}

static void RedrawEditLine(void) {
    if (!console.echo_enabled) return;
    if (console.waiting_for_prompt_cursor_pos || console.prompt_line_y == 0 || console.prompt_start_x == 0) {
        return;
    }

    char move_cmd[32];
    KTerm_WriteString(term, "\x1B[?25l");
    snprintf(move_cmd, sizeof(move_cmd), "\x1B[%d;%dH", console.prompt_line_y, console.prompt_start_x);
    KTerm_WriteString(term, move_cmd);
    KTerm_WriteString(term, "\x1B[K");

    if (console.password_mode) {
        for (int i = 0; i < console.edit_length; i++) KTerm_WriteChar(term, '*');
    } else {
        for (int i = 0; i < console.edit_length; i++) {
            KTerm_WriteChar(term, console.edit_buffer[i]);
        }
    }

    snprintf(move_cmd, sizeof(move_cmd), "\x1B[%d;%dH", console.prompt_line_y, console.prompt_start_x + console.edit_pos);
    KTerm_WriteString(term, move_cmd);
    KTerm_WriteString(term, "\x1B[?25h");
}

static void HandlePrintableKey(int key_code) {
    if (!console.input_enabled) return;
    if (console.edit_length < MAX_COMMAND_BUFFER - 1) {
        if (console.edit_pos < console.edit_length) {
            memmove(&console.edit_buffer[console.edit_pos + 1], &console.edit_buffer[console.edit_pos], console.edit_length - console.edit_pos);
        }
        console.edit_buffer[console.edit_pos] = (char)key_code;
        console.edit_length++;
        console.edit_pos++;
        console.edit_buffer[console.edit_length] = '\0';
        RedrawEditLine();
    }
}

static void HandleBackspaceKey(void) {
    if (!console.input_enabled) return;
    if (console.edit_pos > 0) {
        console.edit_pos--;
        memmove(&console.edit_buffer[console.edit_pos], &console.edit_buffer[console.edit_pos + 1], console.edit_length - console.edit_pos);
        console.edit_length--;
        RedrawEditLine();
    }
}

static void AddToHistory(const char* command) {
    if (strlen(command) == 0) return;
    if (console.history_count > 0 && strcmp(command, console.command_history[console.history_count - 1]) == 0) return;
    if (console.history_count >= 32) {
        for (int i = 0; i < 31; i++) {
            ConsoleCopyString(console.command_history[i], sizeof(console.command_history[i]), console.command_history[i + 1]);
        }
        console.history_count = 31;
    }
    ConsoleCopyString(console.command_history[console.history_count], sizeof(console.command_history[console.history_count]), command);
    console.history_count++;
    console.history_pos = console.history_count;
}

static void NavigateHistory(int direction) {
    if (console.history_count == 0) return;
    if (direction < 0) {
        if (console.history_pos > 0) console.history_pos--;
    } else {
        if (console.history_pos < console.history_count - 1) {
            console.history_pos++;
        } else {
            ClearEditBuffer();
            RedrawEditLine();
            return;
        }
    }
    ConsoleCopyString(console.edit_buffer, sizeof(console.edit_buffer), console.command_history[console.history_pos]);
    console.edit_length = strlen(console.edit_buffer);
    console.edit_pos = console.edit_length;
    RedrawEditLine();
}

static int TokenizeCommand(const char* command, char* tokens[], char** buffer_to_free) {
    *buffer_to_free = NULL;
    if (command == NULL || strlen(command) == 0) return 0;
    char* writable_command = strdup(command);
    if (writable_command == NULL) {
        fprintf(stderr, "Error: Memory allocation failed in TokenizeCommand for: %s\n", command);
        return 0;
    }
    *buffer_to_free = writable_command;
    int token_count = 0;
    char* context;
    const char* delimiters = " \t";
    char* token = strtok_r(writable_command, delimiters, &context);
    while (token != NULL && token_count < MAX_TOKENS) {
        tokens[token_count++] = token;
        token = strtok_r(NULL, delimiters, &context);
    }
    return token_count;
}

static bool CompleteFromStringList(const char* partial, int word_start, const char* const* items, int item_count) {
    const char* matches[32];
    int match_count = 0;
    int partial_len = (int)strlen(partial);
    for (int j = 0; j < item_count && match_count < 32; j++) {
        if (strncmp(items[j], partial, partial_len) == 0) {
            matches[match_count++] = items[j];
        }
    }
    if (match_count == 0) return false;
    if (match_count == 1) {
        CompleteWord(matches[0], word_start);
        return true;
    }
    ShowCompletionMatches(matches, match_count, partial);
    CompleteCommonPrefix(matches, match_count, partial, word_start);
    return true;
}

#if defined(_WIN32)
static bool CompletePathWord(const char* partial, int word_start) {
    char search[MAX_PATH + 2];
    if (!partial || partial[0] == '\0') {
        ConsoleCopyString(search, sizeof(search), "*");
    } else {
        snprintf(search, sizeof(search), "%s*", partial);
    }
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(search, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return false;
    const char* matches[32];
    char match_storage[32][MAX_PATH];
    int match_count = 0;
    do {
        if (fd.cFileName[0] == '.' &&
            (fd.cFileName[1] == '\0' || (fd.cFileName[1] == '.' && fd.cFileName[2] == '\0'))) {
            continue;
        }
        if (match_count >= 32) continue;
        ConsoleCopyString(match_storage[match_count], sizeof(match_storage[match_count]), fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            size_t len = strlen(match_storage[match_count]);
            if (len + 1 < sizeof(match_storage[match_count])) {
                match_storage[match_count][len] = '\\';
                match_storage[match_count][len + 1] = '\0';
            }
        }
        matches[match_count] = match_storage[match_count];
        match_count++;
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
    if (match_count == 0) return false;
    if (match_count == 1) {
        CompleteWord(matches[0], word_start);
        return true;
    }
    ShowCompletionMatches(matches, match_count, partial);
    CompleteCommonPrefix(matches, match_count, partial, word_start);
    return true;
}
#endif

static bool CompleteCommand(const char* partial, int word_start) {
    static const char* commands[] = {
        "clear", "cls", "echo", "test", "help", "graphics", "blink", "echo_on", "noecho", "mouse_on", "mouse_off", "password", "normal", "history", "exit", "quit", "edit",
        "pipeline_stats", "set_fps", "set_budget", "color_test", "cursor_test", "scroll_test", "performance", "demo", "rainbow",
        "vt_styles", "vt_demo", "vt_box", "vt_menu", "vt_combo", "vt_dialog", "vt_styles_capture",
        "type", "shell", "font", "pwd", "cd", "ls", "dir", "sysinfo", "ps", "processes", "threads", "workers",
        "term_status", "term_vtlevel", "term_da", "term_runtest", "term_showinfo", "term_diagbuffers", "term_size", "sys_info", "sys_displays", "sys_audio", "sys_userdir"
    };
    static const int num_commands = sizeof(commands) / sizeof(commands[0]);
    if (word_start == 0) {
        const char* matches[32];
        int match_count = 0;
        int partial_len = strlen(partial);
        for (int i = 0; i < num_commands && match_count < 32; i++) {
            if (strncmp(commands[i], partial, partial_len) == 0) {
                matches[match_count++] = commands[i];
            }
        }
        if (match_count == 0) return false;
        if (match_count == 1) {
            CompleteWord(matches[0], word_start);
            return true;
        }
        ShowCompletionMatches(matches, match_count, partial);
        CompleteCommonPrefix(matches, match_count, partial, word_start);
        return true;
    }
    char first_word[MAX_COMMAND_BUFFER];
    int first_word_len = 0;
    while (first_word_len < console.edit_length && console.edit_buffer[first_word_len] != ' ') first_word_len++;
    if (first_word_len > 0 && first_word_len < MAX_COMMAND_BUFFER) {
        strncpy(first_word, console.edit_buffer, first_word_len);
        first_word[first_word_len] = '\0';
        if (strcmp(first_word, "set_fps") == 0) {
            static const char* fps_values[] = {"30", "60", "120"};
            return CompleteFromStringList(partial, word_start, fps_values, 3);
        } else if (strcmp(first_word, "set_budget") == 0) {
            static const char* budget_values[] = {"0.1", "0.5", "1.0"};
            return CompleteFromStringList(partial, word_start, budget_values, 3);
        } else if (strcmp(first_word, "font") == 0) {
            static const char* font_names[] = {
                "VT220", "IBM", "VGA", "ULTIMATE", "CP437_16", "NEC", "TOSHIBA", "TRIDENT",
                "COMPAQ", "OLYMPIAD", "MC6847", "NEOGEO", "ATASCII", "PETSCII", "PETSCII_SHIFT",
                "TOPAZ", "PREPPIE", "VCR"
            };
            return CompleteFromStringList(partial, word_start, font_names,
                (int)(sizeof(font_names) / sizeof(font_names[0])));
#if defined(_WIN32)
        } else if (strcmp(first_word, "cd") == 0 || strcmp(first_word, "type") == 0) {
            return CompletePathWord(partial, word_start);
#endif
        }
    }
    return false;
}

static void CompleteWord(const char* completion, int word_start) {
    int current_word_len = console.edit_pos - word_start;
    int completion_len = strlen(completion);
    int chars_to_add = completion_len - current_word_len;
    if (chars_to_add <= 0) return;
    if (console.edit_length + chars_to_add >= MAX_COMMAND_BUFFER - 1) return;
    if (console.edit_pos < console.edit_length) {
        memmove(&console.edit_buffer[console.edit_pos + chars_to_add], &console.edit_buffer[console.edit_pos], console.edit_length - console.edit_pos);
    }
    strncpy(&console.edit_buffer[console.edit_pos], completion + current_word_len, chars_to_add);
    console.edit_pos += chars_to_add;
    console.edit_length += chars_to_add;
    console.edit_buffer[console.edit_length] = '\0';
    if (word_start == 0 && console.edit_pos == console.edit_length && console.edit_length < MAX_COMMAND_BUFFER - 2) {
        console.edit_buffer[console.edit_length++] = ' ';
        console.edit_buffer[console.edit_length] = '\0';
        console.edit_pos++;
    }
    RedrawEditLine();
}

static void ShowCompletionMatches(const char* matches[], int count, const char* partial) {
    (void)partial;
    KTerm_WriteChar(term, '\n');
    int max_width = 0;
    for (int i = 0; i < count; i++) { int len = strlen(matches[i]); if (len > max_width) max_width = len; }
    max_width += 2;
    int cols = DEFAULT_TERM_WIDTH / max_width; if (cols == 0) cols = 1;
    for (int i = 0; i < count; i++) {
        KTerm_WriteString(term, matches[i]);
        if ((i + 1) % cols == 0 || i == count - 1) KTerm_WriteChar(term, '\n');
        else { int len = strlen(matches[i]); for (int j = len; j < max_width; j++) KTerm_WriteChar(term, ' '); }
    }
    ShowPrompt();
    if (console.edit_length > 0) RedrawEditLine();
}

static void CompleteCommonPrefix(const char* matches[], int count, const char* partial, int word_start) {
    if (count < 1) return;
    int common_len = strlen(matches[0]);
    for (int i = 1; i < count; i++) {
        int j = 0;
        while (j < common_len && j < strlen(matches[i]) && matches[0][j] == matches[i][j]) j++;
        common_len = j;
    }
    int current_len = strlen(partial);
    if (common_len > current_len) {
        char common_prefix[MAX_COMMAND_BUFFER];
        strncpy(common_prefix, matches[0], common_len);
        common_prefix[common_len] = '\0';
        CompleteWord(common_prefix, word_start);
    }
}

static bool AttemptTabCompletion(void) {
    int word_start = console.edit_pos;
    while (word_start > 0 && console.edit_buffer[word_start - 1] != ' ') word_start--;
    int word_len = console.edit_pos - word_start;
    char partial_word[MAX_COMMAND_BUFFER];
    if (word_len > 0) strncpy(partial_word, console.edit_buffer + word_start, word_len);
    partial_word[word_len] = '\0';
    return CompleteCommand(partial_word, word_start);
}

static void HandleTabKey(void) {
    if (GET_SESSION(term)->raw_mode) { HandlePrintableKey('\t'); return; }
    if (AttemptTabCompletion()) return;
    if (console.edit_length > 0) {
        int next_tab_pos = ((console.edit_pos / 4) + 1) * 4;
        int spaces_to_add = next_tab_pos - console.edit_pos;
        if (spaces_to_add == 0) spaces_to_add = 4;
        for (int i = 0; i < spaces_to_add && console.edit_length < MAX_COMMAND_BUFFER - 1; i++) {
            HandlePrintableKey(' ');
        }
    }
}

static void ShowPrompt(void) {
    KTerm_WriteString(term, "\x1B[32mKaOS>\x1B[0m ");
    console.waiting_for_prompt_cursor_pos = true;
    console.input_enabled = false;
    console.line_ready = false;
    cursor_tracker.waiting_for_position = true;
    cursor_tracker.position_received = false;
    KTerm_WriteString(term, "\x1B[6n");
    console.prompt_pending = false;
}

#endif /* CONSOLE_CLI_EDIT_H */
