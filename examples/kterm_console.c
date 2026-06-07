/**********************************************************************************************
 *
 * @file kterm_console.c
 *   (c) 2025-2026 Jacques Morel
 * @brief Command-Line Interface (CLI) for KaOS Terminal
 * @version 0.2
 *
 * @section Overview:
 *   console.h is a single-header stub for a command-line interface built on top of situation.h and kterm.h.
 *   It provides basic command processing, history navigation, tab completion, and integration with system queries via situation.h.
 *   This is currently a minimal implementation and will evolve into a more robust CLI with advanced features like scripting, plugins, and full system control.
 *
 * @section Key Features:
 *   - **Command Processing:** Tokenizes and executes commands like 'help', 'clear', 'echo', system info queries.
 *   - **Input Editing:** Supports line editing with backspace, arrows, history (up/down), tab completion.
 *   - **History Management:** Stores up to 32 recent commands for navigation.
 *   - **Tab Completion:** Context-aware completion for commands and arguments.
 *   - **Password Mode:** Masks input for sensitive commands.
 *   - **Integration with situation.h:** Commands for hardware info (CPU/GPU/RAM), displays, audio devices, user directory.
 *   - **KTerm Diagnostics:** Commands to query terminal status, VT level, device attributes, run tests.
 *   - **Performance Tools:** Set FPS/budget, run output tests.
 *
 * @section Design Principles:
 *   - **Stub Status:** This is a placeholder for a full-featured CLI. Future enhancements include proper error handling, modular commands, scripting support.
 *   - **Single-Header:** Define CONSOLE_IMPLEMENTATION in one .c file for full inclusion.
 *   - **Dependency on situation.h and kterm.h:** Leverages situation.h for platform abstraction and kterm.h for VT emulation and pipeline.
 *   - **Windows-Focused:** Primarily tested on Windows; POSIX fallbacks limited.
 *
 * @section Concurrency Model
 *   - **Single-Threaded:** This library is NOT THREAD-SAFE. All functions must be called from the main thread.
 *
 * @section Usage Models:
 *   A) Header-Only: #define CONSOLE_IMPLEMENTATION in one .c/.cpp file before including console.h.
 *
 * @section Dependencies:
 *   - **Required:** situation.h (with SITUATION_IMPLEMENTATION), kterm.h (with KTERM_IMPLEMENTATION), miniaudio.h (with MA_IMPLEMENTATION)
 *   - **Standard Libs:** <string.h>, <stdlib.h>, <stdio.h>, <math.h>, <stdint.h>
 *   - **Windows APIs:** Winsock2.h (for network info via situation.h)
**********************************************************************************************/
#if defined(_WIN32)
  #define NOMINMAX
#endif

#define SITUATION_USE_OPENGL
#include "situation.h"

// --- KTerm: Header-only (not part of situation.h) ---
#define KTERM_IMPLEMENTATION
#define STB_TRUETYPE_IMPLEMENTATION
#include "sit/k-term/kterm.h"

#define KTERM_IO_SIT_IMPLEMENTATION
#include "kt_io_sit.h"

#define KT_SHELL_IMPLEMENTATION
#include "sit/k-term/kt_shell.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

// #define MAX_COMMAND_BUFFER 1024  // Removed - K-Term defines this as 262144
#define MAX_TOKENS 64
#define MAX_HISTORY 32

// Debug output control
#ifndef CONSOLE_DEBUG
#define CONSOLE_DEBUG 0  // Disabled by default; set to 1 for debug builds
#endif

#if CONSOLE_DEBUG
#define CONSOLE_LOG(...) do { fprintf(stderr, "[Console] "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); fflush(stderr); } while(0)
#else
#define CONSOLE_LOG(...) do {} while(0)
#endif

typedef struct {
    bool waiting_for_position;
    bool position_received;
    int row;
    int col;
} CursorPositionTracker;

// Console state structure
typedef struct {
    char edit_buffer[MAX_COMMAND_BUFFER];
    int edit_pos;
    int edit_length;
    int edit_display_start;
    char command_buffer[MAX_COMMAND_BUFFER];
    bool line_ready;
    bool in_command;
    char command_history[MAX_HISTORY][MAX_COMMAND_BUFFER];
    int history_count;
    int history_pos;
    bool prompt_pending;
    bool waiting_for_pipeline; // Unused in provided code snippet
    int prompt_start_x;
    int prompt_line_y;
    bool echo_enabled;
    bool input_enabled;
    bool password_mode;
    bool waiting_for_prompt_cursor_pos;
} Console;

// Global exit flag
static bool should_exit = false;

// Forward declarations
static void ClearEditBuffer(void);
static void RedrawEditLine(void);
static void HandlePrintableKey(int key_code);
static void HandleBackspaceKey(void);
static void AddToHistory(const char* command);
static void NavigateHistory(int direction);
static int TokenizeCommand(const char* command, char* tokens[], char** buffer_to_free);
static void CompleteWord(const char* completion, int word_start);
static void ShowCompletionMatches(const char* matches[], int count, const char* partial);
static void CompleteCommonPrefix(const char* matches[], int count, const char* partial, int word_start);
static bool CompleteCommand(const char* partial, int word_start);
static bool AttemptTabCompletion(void);
static void HandleTabKey(void);
static void ShowPrompt(void);
static void ProcessCommand(const char* command);
static void HandleExtendedKeyInput(const char* sequence);
static void HandleEnterKey(void);
static void HandleKeyEvent(const char* sequence, size_t length);
static void HandleKTermResponse(void* ctx, KTermSession* session, const char* response, size_t length);


// ***** NEW: Forward declarations for situation.h helper print functions *****
void SitHelperPrintDeviceInfo(void);
void SitHelperPrintDisplayInfo(SituationDisplayInfo* displays, int count);
void SitHelperPrintAudioDeviceInfo(SituationAudioDeviceInfo* devices, int count);
// *************************************************************************

// Global console instance
Console console = {0};
static KTerm* term = NULL;
static CursorPositionTracker cursor_tracker = {0};
static KTShell shell_proc = {0};
static bool shell_mode = false;  // true = pass-through to shell subprocess

static void ConsoleCopyString(char* dst, size_t dst_size, const char* src) {
    if (!dst || dst_size == 0) return;
    if (!src) src = "";
    snprintf(dst, dst_size, "%s", src);
}

static bool ParseCSIResponse(const char* response, size_t length) {
    // Check for cursor position report: ESC[n;mR
    if (length > 3 && response[0] == '\x1B' && response[1] == '[') {
        int i = 2;
        int row = 0, col = 0;
        
        // Parse row
        while (i < length && response[i] >= '0' && response[i] <= '9') {
            row = row * 10 + (response[i] - '0');
            i++;
        }
        
        if (i < length && response[i] == ';') {
            i++;
            // Parse column
            while (i < length && response[i] >= '0' && response[i] <= '9') {
                col = col * 10 + (response[i] - '0');
                i++;
            }
            
            if (i < length && response[i] == 'R') {
                // Valid cursor position response
                if (cursor_tracker.waiting_for_position) {
                    cursor_tracker.row = row;
                    cursor_tracker.col = col;
                    cursor_tracker.position_received = true;
                    cursor_tracker.waiting_for_position = false;
                    return true;
                }
            }
        }
    }
    return false;
}


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
    
    // Hide cursor during redraw to prevent ghost cursor artifacts
    KTerm_WriteString(term, "\x1B[?25l");
    
    snprintf(move_cmd, sizeof(move_cmd), "\x1B[%d;%dH", console.prompt_line_y, console.prompt_start_x);
    KTerm_WriteString(term, move_cmd);
    
    // Clear from cursor to end of line before redrawing
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
    
    // Show cursor again
    KTerm_WriteString(term, "\x1B[?25h");
}

static void HandlePrintableKey(int key_code) {
    if (!console.input_enabled) return;

    if (console.edit_length < MAX_COMMAND_BUFFER - 1) {
        // Insert character at current position in the CLI's buffer
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
        // Shift remaining characters left in the CLI's buffer
        memmove(&console.edit_buffer[console.edit_pos], &console.edit_buffer[console.edit_pos + 1], console.edit_length - console.edit_pos); // No +1 needed here if length is already reduced
        console.edit_length--;
        // console.edit_buffer[console.edit_length] = '\0'; // Ensure null termination

        // Redraw the entire line.
        // The terminal library (if local echo is on) might have just moved the cursor back.
        // RedrawEditLine ensures the character is visually erased and the line is correct.
        // The sequence "\b \b" is a common way for a HOST to tell a simple terminal
        // to backspace, erase, and backspace again if the terminal doesn't do it automatically
        // on just a BS character. Here, RedrawEditLine is more comprehensive.
        RedrawEditLine();
    }
}

// Command history management
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
        if (console.history_pos > 0) {
            console.history_pos--;
        }
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

// Tokenization function
static int TokenizeCommand(const char* command, char* tokens[], char** buffer_to_free) {
    *buffer_to_free = NULL;
    if (command == NULL || strlen(command) == 0) return 0;

    // strtok_r modifies the string, so we need a writable copy.
    char* writable_command = strdup(command);
    if (writable_command == NULL) {
        fprintf(stderr, "Error: Memory allocation failed in TokenizeCommand for: %s\n", command);
        // KTerm_WriteString(term, "\n\x1B[31mError: Out of memory during tokenization.\x1B[0m\n"); // Optional: inform user
        return 0;
    }
    *buffer_to_free = writable_command; // Important: So ProcessCommand can free it
    int token_count = 0;
    char* context; // For strtok_r state

    // Delimiters: space and tab. Add others if needed (e.g., \n, \r if they can be in commands).
    const char* delimiters = " \t";
    char* token = strtok_r(writable_command, delimiters, &context);
    while (token != NULL && token_count < MAX_TOKENS) {
        tokens[token_count++] = token; // Store pointer to the token within writable_command
        token = strtok_r(NULL, delimiters, &context); // Get next token
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

// Tab completion system
static bool CompleteCommand(const char* partial, int word_start) {
    static const char* commands[] = {
        "clear", "cls", "echo", "test", "help", "graphics", "blink", "echo_on", "noecho", "mouse_on", "mouse_off", "password", "normal", "history", "exit", "quit",
        "pipeline_stats", "set_fps", "set_budget", "color_test", "cursor_test", "scroll_test", "performance", "demo", "rainbow",
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

        if (match_count == 0) {
            return false;
        } else if (match_count == 1) {
            CompleteWord(matches[0], word_start);
            return true;
        } else {
            ShowCompletionMatches(matches, match_count, partial);
            CompleteCommonPrefix(matches, match_count, partial, word_start);
            return true;
        }
    } else {
        char first_word[MAX_COMMAND_BUFFER];
        int first_word_len = 0;
        while (first_word_len < console.edit_length && console.edit_buffer[first_word_len] != ' ') {
            first_word_len++;
        }
        
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
    // Your original tab-to-spaces logic
    if (console.edit_length > 0) {
        int next_tab_pos = ((console.edit_pos / 4) + 1) * 4; // Assuming tab stop = 4
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
    KTerm_WriteString(term, "\x1B[6n"); // DSR Request Cursor Position
    console.prompt_pending = false;
}

static void ProcessCommand(const char* command) {
    char* tokens[MAX_TOKENS];
    char* buffer_to_free = NULL;
    int token_count = TokenizeCommand(command, tokens, &buffer_to_free);

    if (token_count == 0) {
        console.prompt_pending = true;
        if (buffer_to_free) free(buffer_to_free);
        return;
    }
    const char* cmd = tokens[0];

    if (strcmp(cmd, "cls") == 0) {
        if (token_count > 1) {
            KTerm_WriteString(term, "\x1B[31mError: 'cls' takes no arguments\x1B[0m\n");
        } else {
            KTerm_WriteString(term, "\x1B[3J\x1B[2J\x1B[H"); // Void scrollback + erase display + home
        }
    } else if (strcmp(cmd, "clear") == 0) {
        if (token_count > 1) {
            KTerm_WriteString(term, "\x1B[31mError: 'clear' takes no arguments\x1B[0m\n");
        } else {
            KTerm_WriteString(term, "\x1B[2J\x1B[H"); // Erase display + home (scrollback preserved)
        }
    } else if (strcmp(cmd, "echo") == 0) {
        if (token_count == 1) {
            KTerm_WriteString(term, "\n");
        } else {
            for (int i = 1; i < token_count; i++) {
                KTerm_WriteString(term, tokens[i]);
                if (i < token_count - 1) KTerm_WriteChar(term, ' ');
            }
            KTerm_WriteString(term, "\n");
        }
    } else if (strcmp(cmd, "noecho") == 0) {
        if (token_count > 1) {
            KTerm_WriteString(term, "\x1B[31mError: 'noecho' takes no arguments\x1B[0m\n");
        } else {
            console.echo_enabled = false;  // Update console's state
            KTerm_WriteString(term, "\x1B[?12l");  // Send DEC private mode reset for local echo
            KTerm_WriteString(term, "Echo disabled\n");
        }
    } else if (strcmp(cmd, "echo_on") == 0) {
        if (token_count > 1) {
            KTerm_WriteString(term, "\x1B[31mError: 'echo_on' takes no arguments\x1B[0m\n");
        } else {
            console.echo_enabled = true;  // Update console's state
            KTerm_WriteString(term, "\x1B[?12h");  // Send DEC private mode set for local echo
            KTerm_WriteString(term, "Echo enabled\n");
        }
    } else if (strcmp(cmd, "mouse_on") == 0) {
        if (token_count > 1) {
            KTerm_WriteString(term, "\x1B[31mError: 'mouse_on' takes no arguments\x1B[0m\n");
        } else {
            KTerm_SetMouseTracking(term, MOUSE_TRACKING_SGR);
            KTerm_EnableMouseFeature(term, "sgr", true);
            KTerm_WriteString(term, "Mouse reporting enabled (SGR).\n");
        }
    } else if (strcmp(cmd, "mouse_off") == 0) {
        if (token_count > 1) {
            KTerm_WriteString(term, "\x1B[31mError: 'mouse_off' takes no arguments\x1B[0m\n");
        } else {
            KTerm_EnableMouseFeature(term, "sgr", false);
            KTerm_SetMouseTracking(term, MOUSE_TRACKING_OFF);
            KTerm_WriteString(term, "Mouse reporting disabled.\n");
        }
    } else if (strcmp(cmd, "password") == 0) {
        if (token_count > 1) {
            KTerm_WriteString(term, "\x1B[31mError: 'password' takes no arguments\x1B[0m\n");
        } else {
            console.password_mode = true;  // Use console state
            KTerm_WriteString(term, "Password mode enabled (input will show as *)\n");
        }
    } else if (strcmp(cmd, "normal") == 0) {
        if (token_count > 1) {
            KTerm_WriteString(term, "\x1B[31mError: 'normal' takes no arguments\x1B[0m\n");
        } else {
            console.password_mode = false;  // Use console state
            KTerm_WriteString(term, "Normal input mode\n");
        }
    } else if (strcmp(cmd, "test") == 0) {
        if (token_count > 1) {
            KTerm_WriteString(term, "\x1B[31mError: 'test' takes no arguments\x1B[0m\n");
        } else {
            const char* test_seq = "\x1B[31mRed \x1B[32mGreen \x1B[33mYellow \x1B[34mBlue \x1B[35mMagenta \x1B[36mCyan \x1B[37mWhite\x1B[0m\n";
            KTerm_WriteString(term, test_seq);
        }
    } else if (strcmp(cmd, "color_test") == 0) {
        if (token_count > 1) {
            KTerm_WriteString(term, "\x1B[31mError: 'color_test' takes no arguments\x1B[0m\n");
        } else {
            KTerm_WriteString(term, "Standard Colors:\n");
            for (int i = 0; i < 8; i++) KTerm_WriteFormat(term, "\x1B[%dm███ ", 30 + i);
            KTerm_WriteString(term, "\x1B[0m\nBright Colors:\n");
            for (int i = 0; i < 8; i++) KTerm_WriteFormat(term, "\x1B[%dm███ ", 90 + i);
            KTerm_WriteString(term, "\x1B[0m\n\n256-color palette (first 32):\n");
            for (int i = 0; i < 32; i++) {
                KTerm_WriteFormat(term, "\x1B[38;5;%dm█", i);
                if ((i + 1) % 16 == 0) KTerm_WriteString(term, "\n");
            }
            KTerm_WriteString(term, "\x1B[0m\n");
        }
    } else if (strcmp(cmd, "rainbow") == 0) {
        if (token_count == 1) {
            const char* text = "Rainbow colors using true color support!";
            int len = strlen(text);
            for (int i = 0; i < len; i++) {
                int r = (int)(127 * (1 + sin(i * 0.3)));
                int g = (int)(127 * (1 + sin(i * 0.3 + 2)));
                int b = (int)(127 * (1 + sin(i * 0.3 + 4)));
                KTerm_WriteFormat(term, "\x1B[38;2;%d;%d;%dm%c", r, g, b, text[i]);
            }
            KTerm_WriteString(term, "\x1B[0m\n");
        } else {
            int char_idx_overall = 0;
            for (int i = 1; i < token_count; i++) {
                const char* text_segment = tokens[i];
                int len = strlen(text_segment);
                for (int j = 0; j < len; j++) {
                    int r = (int)(127 * (1 + sin(char_idx_overall * 0.3)));
                    int g = (int)(127 * (1 + sin(char_idx_overall * 0.3 + 2.094395)));
                    int b = (int)(127 * (1 + sin(char_idx_overall * 0.3 + 4.188790)));
                    KTerm_WriteFormat(term, "\x1B[38;2;%d;%d;%dm%c", r, g, b, text_segment[j]);
                    char_idx_overall++;
                }
                if (i < token_count - 1) {
                    KTerm_WriteChar(term, ' ');
                    char_idx_overall++;
                }
            }
            KTerm_WriteString(term, "\x1B[0m\n");
        }
    } else if (strcmp(cmd, "cursor_test") == 0) {
        if (token_count > 1) KTerm_WriteString(term, "\x1B[31mError: 'cursor_test' takes no arguments\x1B[0m\n");
        else {
            KTerm_WriteString(term, "Cursor movement test:\nMoving cursor around...\n");
            KTerm_WriteString(term, "\x1B[10;10H*\x1B[12;15H*\x1B[8;20H*\x1B[15;5H*\x1B[H");
        }
    } else if (strcmp(cmd, "scroll_test") == 0) {
        if (token_count > 1) KTerm_WriteString(term, "\x1B[31mError: 'scroll_test' takes no arguments\x1B[0m\n");
        else {
            KTerm_WriteString(term, "Scroll test - generating many lines:\n");
            for (int i = 1; i <= 60; i++) KTerm_WriteFormat(term, "Line %d - This is a scrolling test\n", i);
        }
    } else if (strcmp(cmd, "performance") == 0) {
        if (token_count > 1) KTerm_WriteString(term, "\x1B[31mError: 'performance' takes no arguments\x1B[0m\n");
        else {
            KTerm_WriteString(term, "Performance test - sending large amount of data:\n");
            for (int i = 0; i < 1000; i++) KTerm_WriteFormat(term, "Performance test line %d with some text content\n", i);
        }
    } else if (strcmp(cmd, "demo") == 0) {
        if (token_count > 1) KTerm_WriteString(term, "\x1B[31mError: 'demo' takes no arguments\x1B[0m\n");
        else {
            KTerm_WriteString(term, "\x1B[2J\x1B[H");
            // Title with CP437 double-line box
            KTerm_WriteString(term, "   \x1B[36m");
            KTerm_WriteChar(term, 0xC9);
            for (int i = 0; i < 74; i++) KTerm_WriteChar(term, 0xCD);
            KTerm_WriteChar(term, 0xBB);
            KTerm_WriteString(term, "\x1B[0m\n   \x1B[36m");
            KTerm_WriteChar(term, 0xBA);
            KTerm_WriteString(term, "\x1B[0m  \x1B[1;37mK-Term Capability Showcase\x1B[0m");
            KTerm_WriteString(term, "                                              \x1B[36m");
            KTerm_WriteChar(term, 0xBA);
            KTerm_WriteString(term, "\x1B[0m\n   \x1B[36m");
            KTerm_WriteChar(term, 0xC8);
            for (int i = 0; i < 74; i++) KTerm_WriteChar(term, 0xCD);
            KTerm_WriteChar(term, 0xBC);
            KTerm_WriteString(term, "\x1B[0m\n\n");

            // Text attributes
            KTerm_WriteString(term, "   \x1B[1;33mText Attributes:\x1B[0m\n");
            KTerm_WriteString(term, "   \x1B[1mBold\x1B[0m  \x1B[2mDim\x1B[0m  \x1B[3mItalic\x1B[0m  \x1B[4mUnderline\x1B[0m  \x1B[5mBlink\x1B[0m  \x1B[7mInverse\x1B[0m  \x1B[9mStrike\x1B[0m  \x1B[1;4;33mCombined\x1B[0m\n\n");

            // 16 standard colors (fg + bg)
            KTerm_WriteString(term, "   \x1B[1;33m16-Color Palette:\x1B[0m\n   ");
            for (int i = 0; i < 8; i++) KTerm_WriteFormat(term, "\x1B[%dm \x1B[1m%-2d\x1B[0m", 40 + i, i);
            KTerm_WriteString(term, "\n   ");
            for (int i = 0; i < 8; i++) KTerm_WriteFormat(term, "\x1B[%dm \x1B[1m%-2d\x1B[0m", 100 + i, i + 8);
            KTerm_WriteString(term, "\n\n");

            // 256-color cube (6x6x6 section)
            KTerm_WriteString(term, "   \x1B[1;33m256-Color Cube (216 colors):\x1B[0m\n   ");
            for (int g = 0; g < 6; g++) {
                for (int r = 0; r < 6; r++) {
                    for (int b = 0; b < 6; b++) {
                        int idx = 16 + r * 36 + g * 6 + b;
                        KTerm_WriteFormat(term, "\x1B[48;5;%dm ", idx);
                    }
                }
                KTerm_WriteString(term, "\x1B[0m\n   ");
            }
            KTerm_WriteString(term, "\n");

            // Truecolor gradient
            KTerm_WriteString(term, "   \x1B[1;33mTruecolor Gradient (24-bit):\x1B[0m\n   ");
            for (int i = 0; i < 72; i++) {
                int r = (int)(127.5 * (1.0 + sin(i * 0.09)));
                int g = (int)(127.5 * (1.0 + sin(i * 0.09 + 2.094)));
                int b = (int)(127.5 * (1.0 + sin(i * 0.09 + 4.189)));
                KTerm_WriteFormat(term, "\x1B[48;2;%d;%d;%dm ", r, g, b);
            }
            KTerm_WriteString(term, "\x1B[0m\n   ");
            for (int i = 0; i < 72; i++) {
                int v = (int)(i * 255.0 / 72);
                KTerm_WriteFormat(term, "\x1B[48;2;%d;%d;%dm ", v, v, v);
            }
            KTerm_WriteString(term, "\x1B[0m\n\n");

            // CP437 block art
            KTerm_WriteString(term, "   \x1B[1;33mCP437 Block Elements:\x1B[0m\n");
            KTerm_WriteString(term, "   \x1B[37m");
            // Shading gradient
            const unsigned char shades[] = { 0xB0, 0xB1, 0xB2, 0xDB, 0xDB, 0xB2, 0xB1, 0xB0 };
            for (int row = 0; row < 3; row++) {
                KTerm_WriteString(term, "   ");
                for (int rep = 0; rep < 4; rep++) {
                    for (int i = 0; i < 8; i++) {
                        int c = 31 + rep * 2;
                        KTerm_WriteFormat(term, "\x1B[%dm", c);
                        KTerm_WriteChar(term, shades[i]);
                    }
                    KTerm_WriteChar(term, ' ');
                }
                KTerm_WriteString(term, "\x1B[0m\n");
            }
            KTerm_WriteString(term, "\n");

            // Single/double line box drawing comparison
            KTerm_WriteString(term, "   \x1B[1;33mBox Drawing (single/double/mixed):\x1B[0m\n");
            KTerm_WriteString(term, "   \x1B[32m");
            KTerm_WriteChar(term, 0xDA); for (int i=0;i<8;i++) KTerm_WriteChar(term, 0xC4);
            KTerm_WriteChar(term, 0xBF);
            KTerm_WriteString(term, "  ");
            KTerm_WriteChar(term, 0xC9); for (int i=0;i<8;i++) KTerm_WriteChar(term, 0xCD);
            KTerm_WriteChar(term, 0xBB);
            KTerm_WriteString(term, "  ");
            KTerm_WriteChar(term, 0xD6); for (int i=0;i<8;i++) KTerm_WriteChar(term, 0xC4);
            KTerm_WriteChar(term, 0xB7);
            KTerm_WriteString(term, "\x1B[0m\n");
            KTerm_WriteString(term, "   \x1B[32m");
            KTerm_WriteChar(term, 0xB3); KTerm_WriteString(term, " Single "); KTerm_WriteChar(term, 0xB3);
            KTerm_WriteString(term, "  ");
            KTerm_WriteChar(term, 0xBA); KTerm_WriteString(term, " Double "); KTerm_WriteChar(term, 0xBA);
            KTerm_WriteString(term, "  ");
            KTerm_WriteChar(term, 0xBA); KTerm_WriteString(term, " Mixed  "); KTerm_WriteChar(term, 0xB3);
            KTerm_WriteString(term, "\x1B[0m\n");
            KTerm_WriteString(term, "   \x1B[32m");
            KTerm_WriteChar(term, 0xC0); for (int i=0;i<8;i++) KTerm_WriteChar(term, 0xC4);
            KTerm_WriteChar(term, 0xD9);
            KTerm_WriteString(term, "  ");
            KTerm_WriteChar(term, 0xC8); for (int i=0;i<8;i++) KTerm_WriteChar(term, 0xCD);
            KTerm_WriteChar(term, 0xBC);
            KTerm_WriteString(term, "  ");
            KTerm_WriteChar(term, 0xD3); for (int i=0;i<8;i++) KTerm_WriteChar(term, 0xC4);
            KTerm_WriteChar(term, 0xBD);
            KTerm_WriteString(term, "\x1B[0m\n\n");

            KTerm_WriteString(term, "   \x1B[90mType 'color_test', 'graphics', or 'rainbow <text>' for more.\x1B[0m\n\n");
        }
    } else if (strcmp(cmd, "graphics") == 0) {
        if (token_count > 1) KTerm_WriteString(term, "\x1B[31mError: 'graphics' takes no arguments\x1B[0m\n");
        else {
            KTerm_WriteString(term, "\n\x1B[1;33m   CP437 Character Set Showcase:\x1B[0m\n\n");
            // Full CP437 high bytes in rows of 16
            KTerm_WriteString(term, "   \x1B[90m     0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\x1B[0m\n");
            for (int row = 8; row < 16; row++) {
                KTerm_WriteFormat(term, "   \x1B[90m%X0:\x1B[0m ", row);
                for (int col = 0; col < 16; col++) {
                    unsigned char ch = (unsigned char)(row * 16 + col);
                    KTerm_WriteFormat(term, " \x1B[36m%c\x1B[0m ", ch);
                }
                KTerm_WriteString(term, "\n");
            }
            KTerm_WriteString(term, "\n");

            // Decorative separator using various line chars
            KTerm_WriteString(term, "   \x1B[33m");
            for (int i = 0; i < 72; i++) {
                unsigned char sep_chars[] = { 0xC4, 0xC4, 0xC4, 0xFE, 0xC4, 0xC4, 0xC4, 0xF9 };
                KTerm_WriteChar(term, sep_chars[i % 8]);
            }
            KTerm_WriteString(term, "\x1B[0m\n\n");

            // Block art mini scene
            KTerm_WriteString(term, "   \x1B[1;33mBlock Art:\x1B[0m\n");
            KTerm_WriteString(term, "   \x1B[34m");
            KTerm_WriteChar(term, 0xDB); KTerm_WriteChar(term, 0xDB);
            KTerm_WriteString(term, "\x1B[44m  \x1B[0m\x1B[33m");
            KTerm_WriteChar(term, 0xDB);
            KTerm_WriteString(term, "\x1B[0m\x1B[34m");
            KTerm_WriteChar(term, 0xDB); KTerm_WriteChar(term, 0xDB);
            KTerm_WriteString(term, "\x1B[0m   \x1B[32m");
            // Tree
            KTerm_WriteString(term, "    "); KTerm_WriteChar(term, 0x1E); KTerm_WriteString(term, "\n");
            KTerm_WriteString(term, "   \x1B[34m");
            KTerm_WriteChar(term, 0xDB); KTerm_WriteChar(term, 0xDB); KTerm_WriteChar(term, 0xDB);
            KTerm_WriteChar(term, 0xDB); KTerm_WriteChar(term, 0xDB); KTerm_WriteChar(term, 0xDB);
            KTerm_WriteChar(term, 0xDB);
            KTerm_WriteString(term, "\x1B[0m   \x1B[32m");
            KTerm_WriteString(term, "   "); KTerm_WriteChar(term, 0xB2); KTerm_WriteChar(term, 0xDB); KTerm_WriteChar(term, 0xB2);
            KTerm_WriteString(term, "\x1B[0m\n");
            KTerm_WriteString(term, "   \x1B[34m");
            for (int i = 0; i < 7; i++) KTerm_WriteChar(term, 0xDB);
            KTerm_WriteString(term, "\x1B[0m\x1B[32m   ");
            KTerm_WriteString(term, "  "); KTerm_WriteChar(term, 0xB1); KTerm_WriteChar(term, 0xDB); KTerm_WriteChar(term, 0xDB); KTerm_WriteChar(term, 0xDB); KTerm_WriteChar(term, 0xB1);
            KTerm_WriteString(term, "\x1B[0m\n");
            KTerm_WriteString(term, "   \x1B[33m");
            for (int i = 0; i < 30; i++) KTerm_WriteChar(term, 0xB0);
            KTerm_WriteString(term, "\x1B[0m\n\n");
        }
    } else if (strcmp(cmd, "blink") == 0) {
        if (token_count > 1) KTerm_WriteString(term, "\x1B[31mError: 'blink' takes no arguments\x1B[0m\n");
        else KTerm_WriteString(term, "This text should \x1B[5mblink\x1B[0m if blinking is supported.\n");
    } else if (strcmp(cmd, "history") == 0) {
        if (token_count > 1) KTerm_WriteString(term, "\x1B[31mError: 'history' takes no arguments\x1B[0m\n");
        else {
            KTerm_WriteString(term, "Command history:\n");
            for (int i = 0; i < console.history_count; i++) KTerm_WriteFormat(term, "%2d: %s\n", i + 1, console.command_history[i]);
        }
    } else if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0) {
        if (token_count > 1) KTerm_WriteString(term, "\x1B[31mError: 'exit/quit' takes no arguments\x1B[0m\n");
        else {
            KTerm_WriteString(term, "Goodbye!\n");
            should_exit = true;
        }
    } else if (strcmp(cmd, "pipeline_stats") == 0) {
        if (token_count > 1) KTerm_WriteString(term, "\x1B[31mError: 'pipeline_stats' takes no arguments\x1B[0m\n");
        else KTerm_ShowDiagnostics(term);
    } else if (strcmp(cmd, "set_fps") == 0) {
        if (token_count != 2) KTerm_WriteString(term, "\x1B[31mError: 'set_fps' requires one argument (FPS value)\x1B[0m\n");
        else {
            int fps = atoi(tokens[1]);
            if (fps > 0 && fps <= 120) {
                KTerm_SetPipelineTargetFPS(term, fps);
                KTerm_WriteFormat(term, "Target FPS set to %d\n", fps);
            } else KTerm_WriteString(term, "Invalid FPS value (1-120)\n");
        }
    } else if (strcmp(cmd, "set_budget") == 0) {
        if (token_count != 2) KTerm_WriteString(term, "\x1B[31mError: 'set_budget' requires one argument (percentage 0.0-1.0)\x1B[0m\n");
        else {
            double pct = atof(tokens[1]);
            if (pct > 0.0 && pct <= 1.0) {
                KTerm_SetPipelineTimeBudget(term, pct);
                KTerm_WriteFormat(term, "Pipeline time budget set to %.1f%%\n", pct * 100.0);
            } else KTerm_WriteString(term, "Invalid budget percentage (0.01-1.0)\n");
        }
    } else if (strcmp(cmd, "term_status") == 0) {
        if (token_count > 1) {
            KTerm_WriteString(term, "\x1B[31mError: 'term_status' takes no arguments\x1B[0m\n");
        } else {
            // Get the status from the terminal library
            KTermStatus status = KTerm_GetStatus(term); // API call to kterm.h

            // CLI formats and displays the returned data
            KTerm_WriteString(term, "\n--- KTerm Library Status ---\n");
            KTerm_WriteFormat(term, "Input Pipeline Usage: %zu bytes\n", status.pipeline_usage);
            KTerm_WriteFormat(term, "Keyboard Event Usage: %zu events\n", status.key_usage); // Assuming this is from GET_SESSION(term)->vt_keyboard.buffer_count
            KTerm_WriteFormat(term, "Input Pipeline Overflowed: %s\n", status.overflow_detected ? "YES" : "NO");
            KTerm_WriteFormat(term, "Avg Char Process Time: %.6f ms\n", status.avg_process_time * 1000.0);
            KTerm_WriteString(term, "-----------------------------\n");
        }
    } else if (strcmp(cmd, "term_vtlevel") == 0) {
        if (token_count > 1) {
            KTerm_WriteString(term, "\x1B[31mError: 'term_vtlevel' takes no arguments\x1B[0m\n");
        } else {
            // Get the level from the terminal library
            VTLevel level = KTerm_GetLevel(term); // API call to kterm.h

            // CLI formats and displays the returned data
            KTerm_WriteFormat(term, "\nCurrent KTerm VT Level: %d (", level);
            switch (level) {
                case VT_LEVEL_52:   KTerm_WriteString(term, "VT52"); break;
                case VT_LEVEL_100:  KTerm_WriteString(term, "VT100"); break;
                case VT_LEVEL_220:  KTerm_WriteString(term, "VT220"); break;
                case VT_LEVEL_320:  KTerm_WriteString(term, "VT320"); break;
                case VT_LEVEL_420:  KTerm_WriteString(term, "VT420"); break;
                case VT_LEVEL_510:  KTerm_WriteString(term, "VT510"); break;
                case VT_LEVEL_520:  KTerm_WriteString(term, "VT520"); break;
                case VT_LEVEL_525:  KTerm_WriteString(term, "VT525"); break;
                case VT_LEVEL_K95:  KTerm_WriteString(term, "K95"); break;
                case VT_LEVEL_XTERM: KTerm_WriteString(term, "XTERM"); break;
                case VT_LEVEL_TT:    KTerm_WriteString(term, "Tera Term"); break;
                case VT_LEVEL_PUTTY: KTerm_WriteString(term, "PuTTY"); break;
                case VT_LEVEL_ANSI_SYS: KTerm_WriteString(term, "ANSI.SYS"); break;
                default: KTerm_WriteString(term, "Unknown"); break;
            }
            KTerm_WriteString(term, ")\n");
        }
    } else if (strcmp(cmd, "term_da") == 0) { // This one remains largely the same
        if (token_count > 1) {
            KTerm_WriteString(term, "\x1B[31mError: 'term_da' takes no arguments\x1B[0m\n");
        } else {
            KTerm_WriteString(term, "\nRequesting Primary DA (ESC[c)...\n");
            KTerm_WriteString(term, "\x1B[c"); // Send to terminal's input pipeline
            KTerm_WriteString(term, "Requesting Secondary DA (ESC[>c)...\n");
            KTerm_WriteString(term, "\x1B[>c"); // Send to terminal's input pipeline
            // The terminal library will respond via HandleKTermResponse
        }
    } else if (strcmp(cmd, "term_runtest") == 0) {
        if (token_count != 2) {
            KTerm_WriteString(term, "\x1B[31mError: 'term_runtest' requires one argument (e.g., cursor, colors, all)\x1B[0m\n");
        } else {
            KTerm_WriteFormat(term, "\nRequesting terminal to run test: %s\n", tokens[1]);
            // Call the terminal library's function.
            // This function (in kterm.h) MUST write its own output to the pipeline.
            KTerm_RunTest(term, tokens[1]);
        }
    } else if (strcmp(cmd, "term_showinfo") == 0) {
        if (token_count > 1) {
            KTerm_WriteString(term, "\x1B[31mError: 'term_showinfo' takes no arguments\x1B[0m\n");
        } else {
            // Call the terminal library's function.
            // This function (in kterm.h) MUST write its own output to the pipeline.
            KTerm_WriteString(term, "\nRequesting terminal to show its info:\n");
            KTerm_ShowInfo(term);
        }
    } else if (strcmp(cmd, "term_diagbuffers") == 0) {
        if (token_count > 1) {
            KTerm_WriteString(term, "\x1B[31mError: 'term_diagbuffers' takes no arguments\x1B[0m\n");
        } else {
            // Call the terminal library's function.
            // This function (in kterm.h) MUST write its own output to the pipeline.
            KTerm_WriteString(term, "\nRequesting terminal to show buffer diagnostics:\n");
            KTerm_ShowDiagnostics(term);
        }
    } else if (strcmp(cmd, "term_size") == 0) {
        if (token_count > 1) {
            KTerm_WriteString(term, "\x1B[31mError: 'term_size' takes no arguments\x1B[0m\n");
        } else {
            KTermSession* session = GET_SESSION(term);
            KTerm_WriteString(term, "\n\x1B[1;33m--- Terminal Dimensions ---\x1B[0m\n");
            KTerm_WriteFormat(term, "  Columns: %d\n", session->cols);
            KTerm_WriteFormat(term, "  Rows:    %d\n", session->rows);
            KTerm_WriteFormat(term, "  (CSI 18t response: \\e[8;%d;%dt)\n", session->rows, session->cols);
            KTerm_WriteString(term, "----------------------------\n");
        }
    } else if (strcmp(cmd, "pipeline_stats") == 0) { // This CLI-specific alias can stay as is
        if (token_count > 1) KTerm_WriteString(term, "\x1B[31mError: 'pipeline_stats' takes no arguments\x1B[0m\n");
        else KTerm_ShowDiagnostics(term); // Or call the 'term_diagbuffers' logic if you prefer consistency

    }  else if (strcmp(cmd, "sys_info") == 0) {
        KTerm_WriteString(term, "\n\x1B[1;33m--- System Device Information ---\x1B[0m\n");
        SitHelperPrintDeviceInfo();
    } else if (strcmp(cmd, "sys_displays") == 0) {
        KTerm_WriteString(term, "\n\x1B[1;33m--- Physical Display Information ---\x1B[0m\n");
        int display_count = 0;
        SituationDisplayInfo* displays = NULL;
        SituationGetDisplays(&displays, &display_count);
        if (displays) {
            SitHelperPrintDisplayInfo(displays, display_count);
            SituationFreeDisplays(displays, display_count);
            KTerm_WriteFormat(term, "  Current Situation Mon Index: %d\n", SituationGetCurrentMonitor());
        } else {
            char* err_msg = NULL;
        SituationGetLastErrorMsg(&err_msg);
            KTerm_WriteFormat(term, "\x1B[31mError getting display info: %s\x1B[0m\n", err_msg ? err_msg : "Unknown");
            if (err_msg) free(err_msg);
        }
    } else if (strcmp(cmd, "sys_audio") == 0) {
        KTerm_WriteString(term, "\n\x1B[1;33m--- Audio Playback Device Information ---\x1B[0m\n");
        int audio_device_count = 0;
        SituationAudioDeviceInfo* audio_devices = SituationGetAudioDevices(&audio_device_count);
        if (audio_devices) {
            SitHelperPrintAudioDeviceInfo(audio_devices, audio_device_count);
            free(audio_devices);
        } else {
            char* err_msg = NULL;
        SituationGetLastErrorMsg(&err_msg);
            KTerm_WriteFormat(term, "\x1B[31mError getting audio devices: %s\x1B[0m\n", err_msg ? err_msg : "No devices or error");
            if (err_msg) free(err_msg);
        }
    } else if (strcmp(cmd, "sys_userdir") == 0) {
        KTerm_WriteString(term, "\n\x1B[1;33m--- User Directory ---\x1B[0m\n");
        char* user_dir = SituationGetUserDirectory();
        if (user_dir) {
            KTerm_WriteFormat(term, "  User Profile Directory: %s\n", user_dir);
            free(user_dir);
        } else {
            char* err_msg = NULL;
        SituationGetLastErrorMsg(&err_msg);
            KTerm_WriteFormat(term, "\x1B[31mError getting user directory: %s\x1B[0m\n", err_msg ? err_msg : "Unknown");
            if (err_msg) free(err_msg);
        }
    } else if (strcmp(cmd, "type") == 0) {
        if (token_count < 2) {
            KTerm_WriteString(term, "\x1B[31mUsage: type <filepath>\x1B[0m\n");
        } else {
            // Reconstruct path from remaining tokens (handles spaces in paths)
            char filepath[MAX_COMMAND_BUFFER];
            filepath[0] = '\0';
            for (int i = 1; i < token_count; i++) {
                if (i > 1) strcat(filepath, " ");
                strcat(filepath, tokens[i]);
            }
            FILE* f = fopen(filepath, "rb");
            if (!f) {
                KTerm_WriteFormat(term, "\x1B[31mError: Cannot open '%s'\x1B[0m\n", filepath);
            } else {
                // Get file size
                fseek(f, 0, SEEK_END);
                long file_size = ftell(f);
                fseek(f, 0, SEEK_SET);

                if (file_size <= 0) {
                    KTerm_WriteString(term, "\x1B[90m(empty file)\x1B[0m\n");
                } else {
                    // Pipe raw bytes — CP437 high bytes are handled natively via GR charset
                    char chunk[4096];
                    size_t bytes_read;
                    while ((bytes_read = fread(chunk, 1, sizeof(chunk), f)) > 0) {
                        KTerm_PushInput(term, chunk, bytes_read);
                    }
                }
                fclose(f);
            }
        }
    } else if (strcmp(cmd, "pwd") == 0) {
        char cwd[MAX_PATH];
        if (GetCurrentDirectoryA(MAX_PATH, cwd)) {
            KTerm_WriteFormat(term, "%s\n", cwd);
        } else {
            KTerm_WriteString(term, "\x1B[31mError: Could not get current directory\x1B[0m\n");
        }
    } else if (strcmp(cmd, "cd") == 0) {
        if (token_count < 2) {
            char cwd[MAX_PATH];
            if (GetCurrentDirectoryA(MAX_PATH, cwd))
                KTerm_WriteFormat(term, "%s\n", cwd);
        } else {
            char path[MAX_PATH];
            path[0] = '\0';
            for (int i = 1; i < token_count; i++) {
                if (i > 1) strcat(path, " ");
                strcat(path, tokens[i]);
            }
            if (!SetCurrentDirectoryA(path)) {
                KTerm_WriteFormat(term, "\x1B[31mcd: no such directory: %s\x1B[0m\n", path);
            }
        }
    } else if (strcmp(cmd, "ls") == 0 || strcmp(cmd, "dir") == 0) {
        const char* target = (token_count > 1) ? tokens[1] : ".";
        char search_path[MAX_PATH];
        snprintf(search_path, sizeof(search_path), "%s\\*", target);
        WIN32_FIND_DATAA fd;
        HANDLE hFind = FindFirstFileA(search_path, &fd);
        if (hFind == INVALID_HANDLE_VALUE) {
            KTerm_WriteFormat(term, "\x1B[31mls: cannot access '%s'\x1B[0m\n", target);
        } else {
            do {
                if (strcmp(fd.cFileName, ".") == 0) continue;
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    KTerm_WriteFormat(term, "\x1B[1;34m%s/\x1B[0m\n", fd.cFileName);
                } else {
                    LARGE_INTEGER filesize;
                    filesize.LowPart = fd.nFileSizeLow;
                    filesize.HighPart = fd.nFileSizeHigh;
                    if (filesize.QuadPart < 1024)
                        KTerm_WriteFormat(term, "  %s  (%lld B)\n", fd.cFileName, filesize.QuadPart);
                    else if (filesize.QuadPart < 1024*1024)
                        KTerm_WriteFormat(term, "  %s  (%.1f KB)\n", fd.cFileName, filesize.QuadPart / 1024.0);
                    else
                        KTerm_WriteFormat(term, "  %s  (%.1f MB)\n", fd.cFileName, filesize.QuadPart / (1024.0*1024.0));
                }
            } while (FindNextFileA(hFind, &fd));
            FindClose(hFind);
        }
    } else if (strcmp(cmd, "sysinfo") == 0) {
        SituationCPUInfo cpu;
        SituationGPUInfo gpu;
        SituationMemoryInfo mem;
        SituationGetCPUInfo(&cpu);
        SituationGetGPUInfo(&gpu);
        SituationGetMemoryInfo(&mem);
        SituationOSInfo os = SituationGetOSInfo();

        const char* state_str = "READY";
        switch (SituationGetInitState()) {
            case SITUATION_STATE_UNINITIALIZED: state_str = "UNINITIALIZED"; break;
            case SITUATION_STATE_INITIALIZING:  state_str = "INITIALIZING"; break;
            case SITUATION_STATE_READY:         state_str = "READY"; break;
            case SITUATION_STATE_SHUTTING_DOWN: state_str = "SHUTTING DOWN"; break;
        }

        // Box header
        KTerm_WriteString(term, "\n   \x1B[36m");
        KTerm_WriteChar(term, 0xC9);
        for (int i = 0; i < 64; i++) KTerm_WriteChar(term, 0xCD);
        KTerm_WriteChar(term, 0xBB);
        KTerm_WriteString(term, "\x1B[0m\n");

        KTerm_WriteString(term, "   \x1B[36m");
        KTerm_WriteChar(term, 0xBA);
        KTerm_WriteString(term, "\x1B[0m \x1B[1;37mKaOS System Information\x1B[0m                                        \x1B[36m");
        KTerm_WriteChar(term, 0xBA);
        KTerm_WriteString(term, "\x1B[0m\n");

        // Separator
        KTerm_WriteString(term, "   \x1B[36m");
        KTerm_WriteChar(term, 0xCC);
        for (int i = 0; i < 64; i++) KTerm_WriteChar(term, 0xCD);
        KTerm_WriteChar(term, 0xB9);
        KTerm_WriteString(term, "\x1B[0m\n");

        // Content rows (padded to 64 chars inside the box)
        char line[128];

        snprintf(line, sizeof(line), " Situation : v%-12s  KTerm : v%-12s  [%s]",
            SituationGetVersionString(), KTERM_VERSION_STRING, state_str);
        KTerm_WriteString(term, "   \x1B[36m");
        KTerm_WriteChar(term, 0xBA);
        KTerm_WriteFormat(term, "\x1B[0m%-64s\x1B[36m", line);
        KTerm_WriteChar(term, 0xBA);
        KTerm_WriteString(term, "\x1B[0m\n");

        snprintf(line, sizeof(line), " OS        : %s (%s)", os.name, os.version);
        KTerm_WriteString(term, "   \x1B[36m");
        KTerm_WriteChar(term, 0xBA);
        KTerm_WriteFormat(term, "\x1B[0m%-64s\x1B[36m", line);
        KTerm_WriteChar(term, 0xBA);
        KTerm_WriteString(term, "\x1B[0m\n");

        snprintf(line, sizeof(line), " Backend   : %s", SituationGetGraphicsBackendName());
        KTerm_WriteString(term, "   \x1B[36m");
        KTerm_WriteChar(term, 0xBA);
        KTerm_WriteFormat(term, "\x1B[0m%-64s\x1B[36m", line);
        KTerm_WriteChar(term, 0xBA);
        KTerm_WriteString(term, "\x1B[0m\n");

        // Separator
        KTerm_WriteString(term, "   \x1B[36m");
        KTerm_WriteChar(term, 0xCC);
        for (int i = 0; i < 64; i++) KTerm_WriteChar(term, 0xCD);
        KTerm_WriteChar(term, 0xB9);
        KTerm_WriteString(term, "\x1B[0m\n");

        snprintf(line, sizeof(line), " CPU       : %s (%u cores @ %.2f GHz)",
            cpu.name, cpu.thread_count, cpu.clock_speed_ghz);
        line[64] = '\0'; // truncate if too long
        KTerm_WriteString(term, "   \x1B[36m");
        KTerm_WriteChar(term, 0xBA);
        KTerm_WriteFormat(term, "\x1B[0m%-64s\x1B[36m", line);
        KTerm_WriteChar(term, 0xBA);
        KTerm_WriteString(term, "\x1B[0m\n");

        snprintf(line, sizeof(line), " RAM       : %.1f GB free / %.1f GB total",
            mem.available_bytes / (1024.0*1024*1024),
            mem.total_bytes / (1024.0*1024*1024));
        KTerm_WriteString(term, "   \x1B[36m");
        KTerm_WriteChar(term, 0xBA);
        KTerm_WriteFormat(term, "\x1B[0m%-64s\x1B[36m", line);
        KTerm_WriteChar(term, 0xBA);
        KTerm_WriteString(term, "\x1B[0m\n");

        if (gpu.dedicated_memory_bytes > 0)
            snprintf(line, sizeof(line), " GPU       : %s (%llu MB VRAM)",
                gpu.name, (unsigned long long)(gpu.dedicated_memory_bytes / (1024*1024)));
        else
            snprintf(line, sizeof(line), " GPU       : %s", gpu.name);
        line[64] = '\0';
        KTerm_WriteString(term, "   \x1B[36m");
        KTerm_WriteChar(term, 0xBA);
        KTerm_WriteFormat(term, "\x1B[0m%-64s\x1B[36m", line);
        KTerm_WriteChar(term, 0xBA);
        KTerm_WriteString(term, "\x1B[0m\n");

        snprintf(line, sizeof(line), " VRAM Used : %llu MB  |  Draw Calls: %u  |  FPS: %d",
            (unsigned long long)(SituationGetVRAMUsage() / (1024*1024)),
            SituationGetDrawCallCount(), SituationGetFPS());
        KTerm_WriteString(term, "   \x1B[36m");
        KTerm_WriteChar(term, 0xBA);
        KTerm_WriteFormat(term, "\x1B[0m%-64s\x1B[36m", line);
        KTerm_WriteChar(term, 0xBA);
        KTerm_WriteString(term, "\x1B[0m\n");

        // Separator
        KTerm_WriteString(term, "   \x1B[36m");
        KTerm_WriteChar(term, 0xCC);
        for (int i = 0; i < 64; i++) KTerm_WriteChar(term, 0xCD);
        KTerm_WriteChar(term, 0xB9);
        KTerm_WriteString(term, "\x1B[0m\n");

        snprintf(line, sizeof(line), " Audio     : %s (%d Hz, Vol: %.0f%%)",
            SituationGetActiveAudioDeviceName(),
            SituationGetAudioPlaybackSampleRate(),
            SituationGetAudioMasterVolume() * 100.0f);
        line[64] = '\0';
        KTerm_WriteString(term, "   \x1B[36m");
        KTerm_WriteChar(term, 0xBA);
        KTerm_WriteFormat(term, "\x1B[0m%-64s\x1B[36m", line);
        KTerm_WriteChar(term, 0xBA);
        KTerm_WriteString(term, "\x1B[0m\n");

        int display_count = SituationGetMonitorCount();
        for (int i = 0; i < display_count && i < 4; i++) {
            snprintf(line, sizeof(line), " Display[%d]: %s %dx%d @ %d Hz",
                i, SituationGetMonitorName(i), SituationGetMonitorWidth(i),
                SituationGetMonitorHeight(i), SituationGetMonitorRefreshRate(i));
            line[64] = '\0';
            KTerm_WriteString(term, "   \x1B[36m");
            KTerm_WriteChar(term, 0xBA);
            KTerm_WriteFormat(term, "\x1B[0m%-64s\x1B[36m", line);
            KTerm_WriteChar(term, 0xBA);
            KTerm_WriteString(term, "\x1B[0m\n");
        }

        // Separator
        KTerm_WriteString(term, "   \x1B[36m");
        KTerm_WriteChar(term, 0xCC);
        for (int i = 0; i < 64; i++) KTerm_WriteChar(term, 0xCD);
        KTerm_WriteChar(term, 0xB9);
        KTerm_WriteString(term, "\x1B[0m\n");

        int storage_count = SituationGetStorageDeviceCount();
        for (int i = 0; i < storage_count && i < 4; i++) {
            char storage_name[SITUATION_MAX_DEVICE_NAME_LEN];
            uint64_t capacity_bytes = 0;
            uint64_t free_bytes = 0;
            if (!SituationGetStorageDevice(i, storage_name, sizeof(storage_name), &capacity_bytes, &free_bytes)) {
                continue;
            }
            snprintf(line, sizeof(line), " Disk[%d]   : %s  %.1f GB (%.1f free)",
                i, storage_name,
                capacity_bytes / (1024.0*1024*1024),
                free_bytes / (1024.0*1024*1024));
            line[64] = '\0';
            KTerm_WriteString(term, "   \x1B[36m");
            KTerm_WriteChar(term, 0xBA);
            KTerm_WriteFormat(term, "\x1B[0m%-64s\x1B[36m", line);
            KTerm_WriteChar(term, 0xBA);
            KTerm_WriteString(term, "\x1B[0m\n");
        }

        // Box footer
        KTerm_WriteString(term, "   \x1B[36m");
        KTerm_WriteChar(term, 0xC8);
        for (int i = 0; i < 64; i++) KTerm_WriteChar(term, 0xCD);
        KTerm_WriteChar(term, 0xBC);
        KTerm_WriteString(term, "\x1B[0m\n\n");
    } else if (strcmp(cmd, "ps") == 0 || strcmp(cmd, "processes") == 0) {
        int count = 0;
        SituationProcessInfo* procs = SituationGetProcessList(&count);
        if (!procs || count == 0) {
            KTerm_WriteString(term, "\x1B[31mError: Could not enumerate processes\x1B[0m\n");
        } else {
            KTerm_WriteFormat(term, "\x1B[1;33m  %-8s %-10s %s\x1B[0m\n", "PID", "Memory", "Name");
            for (int i = 0; i < count; i++) {
                if (procs[i].memory_bytes == 0 && procs[i].name[0] == '\0') continue;
                if (procs[i].memory_bytes < 1024*1024)
                    KTerm_WriteFormat(term, "  %-8u %6.0f KB  %s\n",
                        procs[i].pid, procs[i].memory_bytes / 1024.0, procs[i].name);
                else
                    KTerm_WriteFormat(term, "  %-8u %6.1f MB  %s\n",
                        procs[i].pid, procs[i].memory_bytes / (1024.0*1024), procs[i].name);
            }
            KTerm_WriteFormat(term, "\n  \x1B[90m(%d processes)\x1B[0m\n", count);
            SituationFreeProcessList(procs, count);
        }
    } else if (strcmp(cmd, "threads") == 0 || strcmp(cmd, "workers") == 0) {
#ifdef SITUATION_ENABLE_THREADING
        SituationThreadPool* pool = SituationGetInternalThreadPool();
        if (!pool) {
            KTerm_WriteString(term, "\x1B[31mThread pool not active\x1B[0m\n");
        } else {
            SituationThreadPoolSnapshot snap;
            SituationGetThreadPoolSnapshot(pool, &snap);

            KTerm_WriteString(term, "\n\x1B[1;36m--- Situation Thread Pool ---\x1B[0m\n");
            KTerm_WriteFormat(term, "  State: %s  |  Workers: %zu  |  Active Jobs: %d\n",
                snap.pool_active ? "\x1B[32mACTIVE\x1B[0m" : "\x1B[31mINACTIVE\x1B[0m",
                snap.worker_count, snap.active_jobs);
            KTerm_WriteFormat(term, "  Queues: Low=%zu  High=%zu  |  Jobs: %llu submitted, %llu completed\n",
                snap.low_queue_depth, snap.high_queue_depth,
                (unsigned long long)snap.stats_jobs_submitted, (unsigned long long)snap.stats_jobs_completed);
            KTerm_WriteString(term, "\n\x1B[1;33m  Role            Name                  CPU  Active\x1B[0m\n");
            for (int i = 0; i < snap.slot_count; i++) {
                const char* role_str = "???";
                switch (snap.slots[i].role) {
                    case SIT_THREAD_ROLE_WORKER: role_str = "Worker"; break;
                    case SIT_THREAD_ROLE_IO:     role_str = "I/O"; break;
                    case SIT_THREAD_ROLE_RENDER: role_str = "Render"; break;
                    case SIT_THREAD_ROLE_AUDIO:  role_str = "Audio"; break;
                    case SIT_THREAD_ROLE_MAIN:   role_str = "Main"; break;
                    default: break;
                }
                KTerm_WriteFormat(term, "  %-14s  %-20s  %3d  %s\n",
                    role_str, snap.slots[i].name,
                    snap.slots[i].last_logical_cpu,
                    snap.slots[i].active ? "\x1B[32mYES\x1B[0m" : "\x1B[90mno\x1B[0m");
            }
            KTerm_WriteFormat(term, "\n  \x1B[33mAudio\x1B[0m: %s  |  Rate: %d Hz  |  Vol: %.0f%%\n",
                SituationIsAudioDevicePlaying() ? "\x1B[32mPlaying\x1B[0m" : "\x1B[90mStopped\x1B[0m",
                SituationGetAudioPlaybackSampleRate(),
                SituationGetAudioMasterVolume() * 100.0f);

            const char* init_str = "Unknown";
            switch (SituationGetInitState()) {
                case SITUATION_STATE_UNINITIALIZED: init_str = "UNINITIALIZED"; break;
                case SITUATION_STATE_INITIALIZING:  init_str = "INITIALIZING"; break;
                case SITUATION_STATE_READY:         init_str = "\x1B[32mREADY\x1B[0m"; break;
                case SITUATION_STATE_SHUTTING_DOWN: init_str = "SHUTTING DOWN"; break;
            }
            KTerm_WriteFormat(term, "  \x1B[33mInit State\x1B[0m: %s\n\n", init_str);
        }
#else
        KTerm_WriteString(term, "\x1B[31mThreading not enabled in this build\x1B[0m\n");
#endif
    } else if (strcmp(cmd, "font") == 0) {
        if (token_count < 2) {
            KTerm_WriteString(term, "\x1B[1;33mAvailable fonts:\x1B[0m\n");
            KTerm_WriteString(term, "  \x1B[36mVT220\x1B[0m          8x10\n");
            KTerm_WriteString(term, "  \x1B[36mIBM\x1B[0m           10x10\n");
            KTerm_WriteString(term, "  \x1B[36mVGA\x1B[0m            8x8\n");
            KTerm_WriteString(term, "  \x1B[36mULTIMATE\x1B[0m       8x16\n");
            KTerm_WriteString(term, "  \x1B[36mCP437_16\x1B[0m       8x16\n");
            KTerm_WriteString(term, "  \x1B[36mNEC\x1B[0m            8x16\n");
            KTerm_WriteString(term, "  \x1B[36mTOSHIBA\x1B[0m        8x16\n");
            KTerm_WriteString(term, "  \x1B[36mTRIDENT\x1B[0m        8x16\n");
            KTerm_WriteString(term, "  \x1B[36mCOMPAQ\x1B[0m         8x16\n");
            KTerm_WriteString(term, "  \x1B[36mOLYMPIAD\x1B[0m       8x16\n");
            KTerm_WriteString(term, "  \x1B[36mMC6847\x1B[0m         8x8\n");
            KTerm_WriteString(term, "  \x1B[36mNEOGEO\x1B[0m         8x8\n");
            KTerm_WriteString(term, "  \x1B[36mATASCII\x1B[0m        8x8\n");
            KTerm_WriteString(term, "  \x1B[36mPETSCII\x1B[0m        8x8\n");
            KTerm_WriteString(term, "  \x1B[36mPETSCII_SHIFT\x1B[0m  8x8\n");
            KTerm_WriteString(term, "  \x1B[36mTOPAZ\x1B[0m          8x8\n");
            KTerm_WriteString(term, "  \x1B[36mPREPPIE\x1B[0m        8x8\n");
            KTerm_WriteString(term, "  \x1B[36mVCR\x1B[0m           12x14\n");
            KTerm_WriteString(term, "\nUsage: \x1B[33mfont <name>\x1B[0m\n");
        } else {
            KTerm_SetFont(term, tokens[1]);
            KTerm_WriteFormat(term, "Font set to: %s\n", tokens[1]);
        }
    } else if (strcmp(cmd, "shell") == 0) {
        if (shell_mode) {
            KTerm_WriteString(term, "\x1B[33mAlready in shell mode.\x1B[0m\n");
        } else {
            const char* shell_cmd = (token_count > 1) ? tokens[1] : NULL;
            KTerm_WriteString(term, "\x1B[32mStarting shell...\x1B[0m\n");
            if (KTShell_Start(&shell_proc, shell_cmd, term->width, term->height)) {
                shell_mode = true;
                console.input_enabled = false;
                console.prompt_pending = false;
                SituationSetWindowTitle("KaOS - Shell");
                // Don't show prompt — shell provides its own
                if (buffer_to_free) free(buffer_to_free);
                return;
            } else {
                KTerm_WriteString(term, "\x1B[31mError: Failed to start shell.\x1B[0m\n");
            }
        }
    } else if (strcmp(cmd, "help") == 0) {
        const char* help_text_page1 =
            "\x1B[1;36mKaOS KTerm Help - Page 1\x1B[0m\n"
            "\x1B[1;32mBasic Commands:\x1B[0m\n"
            "  \x1B[33mhelp\x1B[0m             - Show this help (help 2, help 3 for more)\n"
            "  \x1B[33mcls\x1B[0m              - Clear screen and void scrollback\n"
            "  \x1B[33mclear\x1B[0m            - Clear screen (scrollback preserved)\n"
            "  \x1B[33mecho [text...]\x1B[0m   - Echo text (or newline)\n"
            "  \x1B[33mtype <filepath>\x1B[0m  - Pipe file contents to terminal (supports ANSI art)\n"
            "  \x1B[33mshell [cmd]\x1B[0m     - Start system shell (default: cmd.exe / /bin/sh)\n"
            "  \x1B[33mfont [name]\x1B[0m     - List fonts or switch font (e.g. font VGA)\n"
            "  \x1B[33mhistory\x1B[0m          - Show command history\n"
            "  \x1B[33mexit/quit\x1B[0m        - Exit console\n"
            "\x1B[1;32mKTerm Control:\x1B[0m\n"
            "  \x1B[33mecho_on/noecho\x1B[0m    - Toggle terminal's local echo (ESC[?12h/l)\n"
            "  \x1B[33mmouse_on/mouse_off\x1B[0m - Enable/disable mouse click reporting (SGR)\n"
            "  \x1B[33mpassword/normal\x1B[0m  - Toggle CLI's password input display mode (*)\n"
            "\x1B[1;32mDemo Commands:\x1B[0m\n"
            "  \x1B[33mdemo\x1B[0m             - General features demo\n"
            "  \x1B[33mtest\x1B[0m             - Basic color test (old)\n"
            "  \x1B[33mcolor_test\x1B[0m       - ANSI & 256-color demo\n"
            "  \x1B[33mrainbow [txt...]\x1B[0m - True color rainbow text\n"
            "  \x1B[33mgraphics\x1B[0m         - Box drawing & block characters demo\n"
            "  \x1B[33mblink\x1B[0m            - Blinking text test\n"
            "  \x1B[33mscroll_test\x1B[0m      - Multi-line scrolling demo\n"
            "\x1B[90mShortcuts: \x1B[33m↑/↓\x1B[90m History, \x1B[33mTab\x1B[90m Complete, \x1B[33mCtrl+C\x1B[90m Interrupt, \x1B[33mCtrl+L\x1B[90m Clear\x1B[0m\n";
        const char* help_text_page2 =
            "\x1B[1;36mKaOS KTerm Help - Page 2\x1B[0m\n"
            "\x1B[1;32mKTerm Library Diagnostics:\x1B[0m\n"
            "  \x1B[33mterm_status\x1B[0m      - Show terminal library's KTerm_GetStatus(term)\n"
            "  \x1B[33mterm_vtlevel\x1B[0m     - Display current VT compatibility level\n"
            "  \x1B[33mterm_da\x1B[0m          - Request Primary & Secondary Device Attributes\n"
            "  \x1B[33mterm_diagbuffers\x1B[0m - Show terminal's internal buffer diagnostics\n"
            "  \x1B[33mterm_size\x1B[0m        - Show terminal grid dimensions (cols x rows)\n"
            "  \x1B[33mterm_showinfo\x1B[0m    - Display terminal's full internal info screen\n"
            "  \x1B[33mterm_runtest \x1B[36m<name>\x1B[0m - Run internal terminal test suite\n"
            "     \x1B[36m<name>\x1B[0m: \x1B[90mcursor, colors, charset, mouse, modes, all\x1B[0m\n"
            "\x1B[1;32mPerformance Related:\x1B[0m\n"
            "  \x1B[33mperformance\x1B[0m      - Run CLI's high-volume output test\n"
            "  \x1B[33mpipeline_stats\x1B[0m   - Alias for term_diagbuffers (CLI specific)\n"
            "  \x1B[33mset_fps <val>\x1B[0m      - Set terminal's pipeline target FPS (1-120)\n"
            "  \x1B[33mset_budget <pct>\x1B[0m  - Set term's pipeline time budget (0.01-1.0)\n"
            "\x1B[1;32mSystem Information (via situation.h):\x1B[0m\n"
            "  \x1B[33msys_info\x1B[0m         - Show detailed hardware/OS information\n"
            "  \x1B[33msys_displays\x1B[0m     - List physical display monitors and modes\n"
            "  \x1B[33msys_audio\x1B[0m        - List available audio playback devices\n"
            "  \x1B[33msys_userdir\x1B[0m      - Show current user's profile directory\n"
            "\x1B[90mNote: KTerm diagnostic commands query/use the terminal library's features.\x1B[0m\n";
        const char* help_text_page3 =
            "\x1B[1;36mKaOS KTerm Help - Page 3\x1B[0m\n"
            "\x1B[1;32mFilesystem:\x1B[0m\n"
            "  \x1B[33mpwd\x1B[0m              - Print current working directory\n"
            "  \x1B[33mcd <path>\x1B[0m        - Change directory\n"
            "  \x1B[33mls [path]\x1B[0m        - List directory contents\n"
            "  \x1B[33mdir [path]\x1B[0m       - Alias for ls\n"
            "\x1B[1;32mSystem Introspection:\x1B[0m\n"
            "  \x1B[33msysinfo\x1B[0m          - Full system snapshot (OS, CPU, GPU, RAM, audio, displays)\n"
            "  \x1B[33mps\x1B[0m               - List running OS processes (PID, memory, name)\n"
            "  \x1B[33mprocesses\x1B[0m        - Alias for ps\n"
            "  \x1B[33mthreads\x1B[0m          - Show Situation internal thread pool status\n"
            "  \x1B[33mworkers\x1B[0m          - Alias for threads\n"
            "\x1B[1;32mAppearance:\x1B[0m\n"
            "  \x1B[33mfont\x1B[0m             - List available built-in fonts\n"
            "  \x1B[33mfont <name>\x1B[0m      - Switch font (e.g. font VGA, font NEC)\n"
            "\x1B[90mTip: help 1 = basics, help 2 = diagnostics, help 3 = this page\x1B[0m\n";
        if (token_count == 1 || (token_count == 2 && strcmp(tokens[1], "1") == 0)) {
            KTerm_WriteString(term, help_text_page1);
        } else if (token_count == 2 && strcmp(tokens[1], "2") == 0) {
            KTerm_WriteString(term, help_text_page2);
        } else if (token_count == 2 && strcmp(tokens[1], "3") == 0) {
            KTerm_WriteString(term, help_text_page3);
        } else {
            KTerm_WriteString(term, "\x1B[31mUsage: help [1|2|3]\x1B[0m\n");
        }

    } else {
        KTerm_WriteString(term, "\x1B[31mUnknown command: \x1B[0m");
        KTerm_WriteString(term, cmd);
        KTerm_WriteString(term, "\n\x1B[90mType 'help' for available commands.\x1B[0m\n");
    }
    
    console.prompt_pending = true;
    console.in_command = false;
    
    if (buffer_to_free) {
        free(buffer_to_free);
    }
}

// Keyboard event handling
static void HandleExtendedKeyInput(const char* sequence) {
    if (strcmp(sequence, "\x1B[A") == 0 || strcmp(sequence, "\x1BOA") == 0) NavigateHistory(-1);
    else if (strcmp(sequence, "\x1B[B") == 0 || strcmp(sequence, "\x1BOB") == 0) NavigateHistory(1);
    else if (strcmp(sequence, "\x1B[D") == 0 || strcmp(sequence, "\x1BOD") == 0) { if (console.edit_pos > 0) { console.edit_pos--; RedrawEditLine(); } }
    else if (strcmp(sequence, "\x1B[C") == 0 || strcmp(sequence, "\x1BOC") == 0) { if (console.edit_pos < console.edit_length) { console.edit_pos++; RedrawEditLine(); } }
    else if (strcmp(sequence, "\x1B[H") == 0) { console.edit_pos = 0; RedrawEditLine(); } // Home
    else if (strcmp(sequence, "\x1B[F") == 0) { console.edit_pos = console.edit_length; RedrawEditLine(); } // End
    else if (strcmp(sequence, "\x1B[3~") == 0) { // Delete
        if (console.edit_pos < console.edit_length) {
            memmove(&console.edit_buffer[console.edit_pos],
                    &console.edit_buffer[console.edit_pos + 1],
                    console.edit_length - console.edit_pos); // Corrected memmove
            console.edit_length--;
            console.edit_buffer[console.edit_length] = '\0'; // Ensure null term
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
    // This function is now called by HandleKTermResponse AFTER DSR processing.
    // It receives fully processed VT sequences or characters.

    // Critical check: Only process key events if CLI input is enabled
    // AND we are not in the middle of processing a command (console.in_command).
    // Ctrl+C is a special case that should often bypass this.
    if (!(console.input_enabled && !console.in_command) &&
        !(length == 1 && sequence[0] == 0x03)) { // Allow Ctrl+C (0x03)
        return;
    }

    // REMOVE THE DEBUG KTerm_WriteFormat from here, as it will go to the terminal screen.
    // If you need this debug, print it to stderr or a log file.
    // fprintf(stderr, "[DEBUG HandleKeyEvent: Seq='%.*s' (Len:%d) input_enabled:%d]\n", length, sequence, length, console.input_enabled);


    // --- Handle specific key sequences for line editing ---

    if (length == 1 && (sequence[0] == '\r' || sequence[0] == '\n')) { // Enter
        HandleEnterKey();
        return;
    }
    if (length == 1 && (sequence[0] == '\b' || sequence[0] == 0x7F)) { // Backspace or DEL
        HandleBackspaceKey(); // This function should redraw
        return;
    }
    if (length == 1 && sequence[0] == '\t') { // Tab
        HandleTabKey(); // This function should redraw if changes are made
        return;
    }

    // Handle Ctrl+<char> sequences (0x01 to 0x1A)
    if (length == 1 && sequence[0] >= 0x01 && sequence[0] <= 0x1A) {
        int ctrl_char_code = sequence[0];
        switch (ctrl_char_code) {
            case 0x01:  // Ctrl+A - Move to beginning of line
                if (console.input_enabled) { // Check again, as Ctrl+C might have changed it
                    console.edit_pos = 0;
                    RedrawEditLine();
                }
                break;
            case 0x02:  // Ctrl+B - Move back one character
                if (console.input_enabled && console.edit_pos > 0) {
                    console.edit_pos--;
                    RedrawEditLine();
                }
                break;
            case 0x03:  // Ctrl+C - Interrupt / Clear line
                // console.input_enabled might already be false if called from Ctrl+C itself.
                KTerm_WriteChar(term, '^'); // Visually show ^C
                KTerm_WriteChar(term, 'C');
                KTerm_WriteChar(term, '\n');

                ClearEditBuffer();
                // RedrawEditLine(); // Not needed, new prompt will be shown
                
                console.in_command = false; // Ensure not stuck in command processing
                console.waiting_for_prompt_cursor_pos = false; // Cancel any pending DSR wait
                console.prompt_pending = true; // Request a new prompt
                console.input_enabled = false; // Disable input until new prompt DSR is back
                break;
            case 0x04:  // Ctrl+D - Delete char or EOF
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
            case 0x05:  // Ctrl+E - Move to end of line
                console.edit_pos = console.edit_length;
                RedrawEditLine();
                break;
            case 0x06:  // Ctrl+F - Move forward one character
                if (console.edit_pos < console.edit_length) {
                    console.edit_pos++;
                    RedrawEditLine();
                }
                break;
            case 0x08:  // Ctrl+H - Backspace
                HandleBackspaceKey();
                break;
            case 0x0A: // LF - Line Feed
                // case 0x0B: // VT - Vertical Tab (often treated same as LF)
                // case 0x0C: // FF - Form Feed (often treated same as LF or clears screen + home)
                GET_SESSION(term)->cursor.y++;
                if (GET_SESSION(term)->cursor.y > GET_SESSION(term)->scroll_bottom) { // Key condition
                    GET_SESSION(term)->cursor.y = GET_SESSION(term)->scroll_bottom;   // Clamp cursor to last line of region
                    KTerm_ScrollUpRegion(term, GET_SESSION(term)->scroll_top, GET_SESSION(term)->scroll_bottom, 1); // Scroll content
                }
                if (GET_SESSION(term)->ansi_modes.line_feed_new_line) { // LNM mode
                    GET_SESSION(term)->cursor.x = GET_SESSION(term)->left_margin; // Move to left margin of current line
                }
                break;
            case 0x0B:  // Ctrl+K - Kill to end of line
                console.edit_length = console.edit_pos;
                console.edit_buffer[console.edit_length] = '\0';
                RedrawEditLine();
                break;
            case 0x0C:  // Ctrl+L - Clear screen
                KTerm_WriteString(term, "\x1B[2J\x1B[H"); // Send clear screen and home to terminal
                console.prompt_pending = true;        // A new prompt will be needed
                console.input_enabled = false;        // Wait for DSR for new prompt
                console.waiting_for_prompt_cursor_pos = false; // Reset DSR wait state
                // No need to RedrawEditLine() here as the screen is cleared.
                break;
            case 0x0E:  // Ctrl+N - Next history
                NavigateHistory(1);
                break;
            case 0x10:  // Ctrl+P - Previous history
                NavigateHistory(-1);
                break;
            case 0x15:  // Ctrl+U - Clear line
                ClearEditBuffer();
                RedrawEditLine();
                break;
            case 0x17:  // Ctrl+W - Delete word
                if (console.edit_pos == 0) break;
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
                break;
            default:
                // Ignore other control characters
                break;
        }
        return;
    }

    // Handle extended key sequences (like arrows, Home, End - usually multi-byte ESC sequences)
    // These arrive as full sequences like "\x1B[A"
    if (length > 1 && sequence[0] == '\x1B') { // Common start for escape sequences
        // DSR would have been caught by HandleKTermResponse already.
        if (!GET_SESSION(term)->raw_mode) { // Assuming GET_SESSION(term)->raw_mode is from your terminal library
            HandleExtendedKeyInput(sequence); // This function should call RedrawEditLine if needed
        } else {
            // In raw mode, the application might want to see the raw sequence.
            // For a CLI, raw mode usually means the line editor is bypassed.
            // If your terminal library sends raw sequences in raw_mode,
            // you'd probably not call HandleKeyEvent in that case, or HandleKeyEvent
            // would just buffer it for a different kind of processing.
            // For now, let's assume HandleExtendedKeyInput knows what to do.
        }
        return;
    }

    // Handle printable characters
    // This assumes single-byte printable chars. For UTF-8, length could be > 1.
    // Your current HandlePrintableKey takes an `int key_code`.
    // If `sequence` can be multi-byte for a single character (UTF-8), this needs adjustment.
    // Assuming ASCII for now based on `HandlePrintableKey`'s signature.
    if (length == 1 && sequence[0] >= 32) { // ASCII 32 (space) and up
        HandlePrintableKey(sequence[0]);
        // RedrawEditLine() is NOT called here because HandlePrintableKey should
        // ideally send the character to the pipeline IF local echo is OFF.
        // If terminal local echo is ON, the terminal library handles display.
        // If your HandlePrintableKey also calls RedrawEditLine, that's fine.
    }

    // Unhandled sequences could be logged if necessary
    // else {
    //     fprintf(stderr, "CLI: Unhandled key sequence in HandleKeyEvent: '%.*s'\n", length, sequence);
    // }
}

// Title callback — updates the OS window title when shell programs emit OSC 2
static void HandleTitleChange(KTerm* t, const char* title, bool is_icon) {
    (void)t;
    if (!is_icon && title && title[0]) {
        char buf[256];
        snprintf(buf, sizeof(buf), "KaOS - %s", title);
        SituationSetWindowTitle(buf);
    }
}

// Response callback (Sink)
static void HandleKTermResponse(void* ctx, KTermSession* session, const char* response_data, size_t length) {
    KTerm* term = (KTerm*)ctx;

    // Shell mode: forward all terminal responses to the shell's stdin
    if (shell_mode) {
        KTShell_Write(&shell_proc, response_data, length);
        return;
    }

    const char* current_pos = response_data;
    int remaining_length = length;

    while (remaining_length > 0) {
        // Try to parse DSR for cursor position: ESC[<row>;<col>R
        // Need a way for ParseCSIResponse to indicate how many bytes it consumed if successful.
        // Let's modify ParseCSIResponse or create a new helper.

        int consumed_bytes = 0;

        // --- Attempt to parse Cursor Position Report (CPR) ---
        if (console.waiting_for_prompt_cursor_pos) { // Only look for CPR if we are expecting it for the prompt
            int cpr_row, cpr_col;
            // Helper function to find and parse CPR:
            // int find_and_parse_cpr(const char* buffer, int buffer_len, int* out_row, int* out_col, int* out_consumed_len)
            // Returns 1 if CPR found and parsed, 0 otherwise.
            // out_consumed_len is how much of 'buffer' the CPR took.
            // For simplicity now, let's assume if it starts with ESC[ and ends with R and contains ;
            // Note: This is a simplified check for this specific scenario. A full parser is more robust.
            
            // Try to match ESC[...R at the current_pos
            if (remaining_length >= 3 && current_pos[0] == '\x1B' && current_pos[1] == '[') {
                const char* r_char = (const char*)memchr(current_pos, 'R', remaining_length);
                if (r_char != NULL && (r_char - current_pos < remaining_length)) {
                    int cpr_len = (r_char - current_pos) + 1;
                    // Now try to parse just this segment as a DSR
                    if (ParseCSIResponse(current_pos, cpr_len)) { // ParseCSIResponse needs to be robust enough
                                                                  // or modified to take a max_len for this segment
                        if (cursor_tracker.position_received) { // ParseCSIResponse sets this
                            console.prompt_line_y = cursor_tracker.row;
                            console.prompt_start_x = cursor_tracker.col;
                            console.waiting_for_prompt_cursor_pos = false;
                            console.input_enabled = true;
                            cursor_tracker.position_received = false; // Consume
                            RedrawEditLine();
                            consumed_bytes = cpr_len;
                        } else {
                             // It was some other ESC[...R sequence, or ParseCSIResponse needs adjustment
                        }
                    }
                }
            }
        }

        // --- Attempt to parse Device Attributes ---
        if (consumed_bytes == 0 && remaining_length >= 3 && current_pos[0] == '\x1B' && current_pos[1] == '[') {
            char end_char_da = current_pos[remaining_length -1]; // This is problematic if DA isn't last
            const char* c_char = (const char*)memchr(current_pos, 'c', remaining_length);

            if (c_char != NULL && (c_char - current_pos < remaining_length) &&
                (current_pos[2] == '?' || current_pos[2] == '>' || current_pos[2] == '=' || current_pos[2] >= '0' && current_pos[2] <='9')) { // Basic DA check
                
                int da_len = (c_char - current_pos) + 1;
                KTerm_WriteString(term, "\n\x1B[36mKTerm DA:\x1B[0m ");
                for(int i=0; i<da_len; ++i) {
                     if (current_pos[i] >= 32 && current_pos[i] < 127) KTerm_WriteChar(term, current_pos[i]);
                     else if (current_pos[i] == '\x1B') KTerm_WriteString(term, "ESC");
                     else KTerm_WriteFormat(term, "[%02X]", (unsigned char)current_pos[i]);
                }
                KTerm_WriteString(term, "\n");
                // If we just printed DA, we likely need a new prompt display cycle
                if (!console.waiting_for_prompt_cursor_pos) { // Avoid if we are already waiting for prompt DSR
                    console.prompt_pending = true;
                    console.input_enabled = false;
                }
                consumed_bytes = da_len;
            }
        }

        if (remaining_length > 2 && current_pos[0] == '\x1B' && current_pos[1] == '[') {
            if (current_pos[2] == 'M' || (current_pos[2] == '<' && strchr(current_pos, 'M') != NULL) || (current_pos[2] == '<' && strchr(current_pos, 'm') != NULL) ) {
                consumed_bytes = remaining_length; // Consume the whole mouse report
            }
        }

        if (consumed_bytes == 0) {
            // Check if this looks like the start of an unrecognized escape sequence
            if ((unsigned char)*current_pos == 0x1B) {
                int skip = 1;
                if (skip < remaining_length && current_pos[skip] == '[') {
                    // CSI sequence: ESC [ <params> <final>
                    skip++; // skip '['
                    // Skip parameter bytes (0x30-0x3F: digits, semicolons, etc.)
                    while (skip < remaining_length && (unsigned char)current_pos[skip] >= 0x30 && (unsigned char)current_pos[skip] <= 0x3F) {
                        skip++;
                    }
                    // Skip intermediate bytes (0x20-0x2F)
                    while (skip < remaining_length && (unsigned char)current_pos[skip] >= 0x20 && (unsigned char)current_pos[skip] <= 0x2F) {
                        skip++;
                    }
                    // Skip final byte (0x40-0x7E)
                    if (skip < remaining_length && (unsigned char)current_pos[skip] >= 0x40 && (unsigned char)current_pos[skip] <= 0x7E) {
                        skip++;
                    }
                } else if (skip < remaining_length && current_pos[skip] == 'O') {
                    // SS3 sequence: ESC O <char>
                    skip++; // skip 'O'
                    if (skip < remaining_length) skip++; // skip final char
                } else if (skip < remaining_length) {
                    // Other ESC sequence: ESC <char>
                    skip++;
                }
                consumed_bytes = skip;
            } else if ((unsigned char)*current_pos == 0x0D || (unsigned char)*current_pos == 0x0A) {
                // CR or LF — treat as Enter key
                HandleKeyEvent("\r", 1);
                consumed_bytes = 1;
            } else if ((unsigned char)*current_pos == 0x08 || (unsigned char)*current_pos == 0x7F) {
                // BS or DEL — treat as backspace
                HandleKeyEvent("\x08", 1);
                consumed_bytes = 1;
            } else if ((unsigned char)*current_pos == 0x09) {
                // Tab
                HandleKeyEvent("\t", 1);
                consumed_bytes = 1;
            } else if ((unsigned char)*current_pos < 0x20) {
                // Other control characters — pass through (Ctrl+C, Ctrl+U, etc.)
                HandleKeyEvent(current_pos, 1);
                consumed_bytes = 1;
            } else if ((unsigned char)*current_pos > 0x7E) {
                // High bytes — discard
                consumed_bytes = 1;
            } else {
                // Printable ASCII — treat as keyboard input
                HandleKeyEvent(current_pos, 1);
                consumed_bytes = 1;
            }
        }

        current_pos += consumed_bytes;
        remaining_length -= consumed_bytes;
    }
}

// Helper functions to print Situation.h info via KTerm_Write*
void SitHelperPrintDeviceInfo(void) {
    SituationCPUInfo cpu;
    SituationGPUInfo gpu;
    SituationMemoryInfo mem;
    SituationGetCPUInfo(&cpu);
    SituationGetGPUInfo(&gpu);
    SituationGetMemoryInfo(&mem);

    KTerm_WriteString(term, "  \x1B[1;34mCPU:\x1B[0m\n");
    KTerm_WriteFormat(term, "    Name: \x1B[37m%s\x1B[0m\n", cpu.name);
    KTerm_WriteFormat(term, "    Threads: \x1B[37m%u\x1B[0m  Cores: \x1B[37m%u\x1B[0m\n", cpu.thread_count, cpu.core_count);
    KTerm_WriteFormat(term, "    Clock Speed: \x1B[37m%.2f GHz\x1B[0m\n", cpu.clock_speed_ghz);

    KTerm_WriteString(term, "  \x1B[1;34mGPU:\x1B[0m\n");
    KTerm_WriteFormat(term, "    Name: \x1B[37m%s\x1B[0m\n", gpu.name);
    KTerm_WriteFormat(term, "    Dedicated VRAM: \x1B[37m%llu MB\x1B[0m\n", gpu.dedicated_memory_bytes / (1024 * 1024));

    KTerm_WriteString(term, "  \x1B[1;34mRAM:\x1B[0m\n");
    KTerm_WriteFormat(term, "    Total: \x1B[37m%llu MB\x1B[0m\n", mem.total_bytes / (1024 * 1024));
    KTerm_WriteFormat(term, "    Available: \x1B[37m%llu MB\x1B[0m\n", mem.available_bytes / (1024 * 1024));

    int storage_count = SituationGetStorageDeviceCount();
    KTerm_WriteFormat(term, "  \x1B[1;34mStorage Devices (%d found):\x1B[0m\n", storage_count);
    for (int i = 0; i < storage_count; ++i) {
        char storage_name[SITUATION_MAX_DEVICE_NAME_LEN];
        uint64_t capacity_bytes = 0;
        uint64_t free_bytes = 0;
        if (!SituationGetStorageDevice(i, storage_name, sizeof(storage_name), &capacity_bytes, &free_bytes)) {
            continue;
        }
        KTerm_WriteFormat(term, "    [%d] Name: \x1B[37m%s\x1B[0m\n", i, storage_name);
        KTerm_WriteFormat(term, "        Capacity: \x1B[37m%llu GB\x1B[0m\n", capacity_bytes / (1024 * 1024 * 1024));
        KTerm_WriteFormat(term, "        Free Space: \x1B[37m%llu GB\x1B[0m\n", free_bytes / (1024 * 1024 * 1024));
    }

    int network_count = SituationGetNetworkAdapterCount();
    KTerm_WriteFormat(term, "  \x1B[1;34mNetwork Adapters (%d found):\x1B[0m\n", network_count);
    for (int i = 0; i < network_count; ++i) {
        char adapter_name[SITUATION_MAX_DEVICE_NAME_LEN];
        if (!SituationGetNetworkAdapterName(i, adapter_name, sizeof(adapter_name))) {
            continue;
        }
        KTerm_WriteFormat(term, "    [%d] Name: \x1B[37m%s\x1B[0m\n", i, adapter_name);
    }

    int input_count = SituationGetInputDeviceCount();
    KTerm_WriteFormat(term, "  \x1B[1;34mInput Devices (%d found):\x1B[0m\n", input_count);
    for (int i = 0; i < input_count; ++i) {
        char input_name[SITUATION_MAX_DEVICE_NAME_LEN];
        if (!SituationGetInputDeviceName(i, input_name, sizeof(input_name))) {
            continue;
        }
        KTerm_WriteFormat(term, "    [%d] Name: \x1B[37m%s\x1B[0m\n", i, input_name);
    }
    KTerm_WriteString(term, "\x1B[0m");
}

void SitHelperPrintDisplayInfo(SituationDisplayInfo* displays, int count) {
    if (!displays || count == 0) {
        KTerm_WriteString(term, "  \x1B[31mNo display information available.\x1B[0m\n");
        return;
    }
    KTerm_WriteFormat(term, "  Found \x1B[1;37m%d\x1B[0m physical display(s):\n", count);
    for (int i = 0; i < count; ++i) {
        KTerm_WriteFormat(term, "  \x1B[1;34mDisplay [%d]:\x1B[0m \x1B[37m%s\x1B[0m\n", i, displays[i].name);
        KTerm_WriteFormat(term, "    Primary: \x1B[37m%s\x1B[0m\n", displays[i].is_primary ? "Yes" : "No");
        KTerm_WriteFormat(term, "    Current Mode: \x1B[37m%dx%d @ %dHz, %d-bit\x1B[0m\n",
               displays[i].current_mode.width, displays[i].current_mode.height,
               displays[i].current_mode.refresh_rate, displays[i].current_mode.color_depth);
        KTerm_WriteFormat(term, "    Available Modes (\x1B[37m%d\x1B[0m found):\n", displays[i].available_mode_count);
        for (int j = 0; j < displays[i].available_mode_count; ++j) {
            if (j < 3 || j > displays[i].available_mode_count - 2) { // Print first 3 and last 1
                 KTerm_WriteFormat(term, "      - \x1B[37m%dx%d @ %dHz, %d-bit\x1B[0m\n",
                       displays[i].available_modes[j].width, displays[i].available_modes[j].height,
                       displays[i].available_modes[j].refresh_rate, displays[i].available_modes[j].color_depth);
            } else if (j == 3 && displays[i].available_mode_count > 4) {
                KTerm_WriteFormat(term, "      - \x1B[90m... (and %d more)\x1B[0m\n", displays[i].available_mode_count - 4);
            }
        }
    }
    KTerm_WriteString(term, "\x1B[0m"); // Reset color
}

void SitHelperPrintAudioDeviceInfo(SituationAudioDeviceInfo* devices, int count) {
    if (!devices || count == 0) {
        KTerm_WriteString(term, "  \x1B[31mNo audio device information available.\x1B[0m\n");
        return;
    }
    KTerm_WriteFormat(term, "  Found \x1B[1;37m%d\x1B[0m audio playback device(s):\n", count);
    for (int i = 0; i < count; ++i) {
        KTerm_WriteFormat(term, "  \x1B[1;34mDevice [%d]\x1B[0m (ID: \x1B[37m%s\x1B[0m): \x1B[37m%s\x1B[0m\n", i, devices[i].id, devices[i].name);
        KTerm_WriteFormat(term, "    Default Playback: \x1B[37m%s\x1B[0m\n", devices[i].is_default_playback ? "Yes" : "No");
    }
    KTerm_WriteString(term, "\x1B[0m"); // Reset color
}

// Main application
int main(void) {
    int argc = 1; // Dummy if no real args; pass from main if available
    char* argv[] = {"console"}; // Dummy
    const char* window_title = "KaOS - Kaizen Operating System v0.1 (Situation-Aware)";
    int target_fps = 60; // Or get from GetPipelineTargetFPS() if kterm.h exposes it early

    SituationInitInfo init_info = {0};
    init_info.enable_vulkan_validation = true;
    init_info.window_width = 80 * 8 * 2;   // 80 cols × 8px × 2x scale = 1280
    init_info.window_height = 50 * 8 * 2;  // 50 rows × 8px × 2x scale = 800
    init_info.window_title = window_title;
    init_info.initial_active_window_flags = SITUATION_WINDOW_STATE_RESIZABLE | SITUATION_WINDOW_STATE_VSYNC_HINT | SITUATION_WINDOW_STATE_ALWAYS_RUN;
    init_info.initial_inactive_window_flags = SITUATION_WINDOW_STATE_ALWAYS_RUN;

    SituationError sit_init_err = SituationInit(
        argc,
        argv,
        &init_info
    );

    if (sit_init_err != SITUATION_SUCCESS) {
        char* err_msg = NULL;
        SituationGetLastErrorMsg(&err_msg);
        fprintf(stderr, "FATAL: SituationInit failed: %s\n", err_msg ? err_msg : "Unknown error");
        if (err_msg) free(err_msg);
        // If SituationInit fails (which includes Raylib's InitWindow), we probably can't continue
        return 1; 
    }
    
    // Disable Situation debug output
    SituationSetTraceLogLevel(SIT_LOG_NONE);
    
    SituationSetTargetFPS(target_fps); // Set after init
    CONSOLE_LOG("Situation.h initialized successfully.");
    
    // Control gate: Wait for Situation READY state before creating K-Term
    CONSOLE_LOG("Checking Situation state...");
    SituationInitState state = SituationGetInitState();
    CONSOLE_LOG("Situation state: %d (0=UNINIT, 1=INITIALIZING, 2=READY, 3=SHUTDOWN)", state);
    
    if (state != SITUATION_STATE_READY) {
        CONSOLE_LOG("FATAL: Situation not in READY state after init");
        SituationShutdown();
        return 1;
    }
    
    // Small delay to let render thread settle into idle state
    // This gives the render thread time to complete any pending operations
    // and enter its waiting state before we create K-Term resources
    CONSOLE_LOG("Waiting for render thread to settle...");
    Sleep(100); // 100ms should be enough for 1-2 frames at 60fps
    
    CONSOLE_LOG("Situation is READY - safe to create K-Term resources.");
    
    // Create K-Term immediately after Situation init (before main loop)
    CONSOLE_LOG("Creating K-Term...");
    
    KTermConfig term_config = {
        .width = 80,
        .height = 50,
        .input_buffer_size = 4 * 1024 * 1024  // 4 MB for file piping
    };
    
    term = KTerm_Create(term_config);
    CONSOLE_LOG("K-Term created: %p", (void*)term);
    
    if (!term) {
        CONSOLE_LOG("FATAL: Failed to create K-Term");
        SituationShutdown();
        return 1;
    }
    
    CONSOLE_LOG("K-Term creation successful, setting up output sink...");
    
    // Use modern Sink Output for zero-copy performance
    KTerm_SetOutputSink(term, HandleKTermResponse, term);
    
    // Window title updates from shell programs (OSC 2)
    KTerm_SetTitleCallback(term, HandleTitleChange);
    
    // Set CP437 as the GR charset (bytes 0x80-0xFF → CP437 box drawing, blocks, etc.)
    KTerm_SelectCharacterSet(term, 1, CHARSET_CP437); // G1 = CP437
    KTerm_WriteString(term, "\x1B~");                 // LS1R: GR = G1
    
    // Enable any-event mouse tracking + SGR encoding so the mouse highlight always follows
    KTerm_WriteString(term, "\x1B[?1003h");           // Any-event tracking (motion without buttons)
    KTerm_WriteString(term, "\x1B[?1006h");           // SGR extended mouse format
    
    // Welcome Message (raw CP437 box drawing: C9=╔ CD=═ BB=╗ BA=║ C8=╚ BC=╝)
    KTerm_WriteString(term, "   \x1B[36m");
    KTerm_WriteChar(term, 0xC9); // ╔
    for(int i=0;i<74;i++) KTerm_WriteChar(term, 0xCD); // ═
    KTerm_WriteChar(term, 0xBB); // ╗
    KTerm_WriteString(term, "\x1B[0m\n");
    KTerm_WriteString(term, "   \x1B[36m");
    KTerm_WriteChar(term, 0xBA); // ║
    KTerm_WriteString(term, "\x1B[0m \x1B[1;37mKaizen Operating System\x1B[0m - \x1B[33mK-Term v2.7.14\x1B[0m Console                       \x1B[36m");
    KTerm_WriteChar(term, 0xBA); // ║
    KTerm_WriteString(term, "\x1B[0m\n");
    KTerm_WriteString(term, "   \x1B[36m");
    KTerm_WriteChar(term, 0xC8); // ╚
    for(int i=0;i<74;i++) KTerm_WriteChar(term, 0xCD); // ═
    KTerm_WriteChar(term, 0xBC); // ╝
    KTerm_WriteString(term, "\x1B[0m\n\n");
    KTerm_WriteString(term, "   \x1B[32mWelcome to KaOS!\x1B[0m Type \x1B[1;33mhelp\x1B[0m for available commands.\n\n");
    
    CONSOLE_LOG("Calling ShowPrompt...");
    ShowPrompt();
    CONSOLE_LOG("K-Term fully initialized!");

    console.prompt_pending = false;
    console.in_command = false;
    console.line_ready = false;
    console.history_count = 0;
    console.history_pos = 0;
    console.echo_enabled = true;
    console.input_enabled = false;
    ClearEditBuffer();
    
    CONSOLE_LOG("Entering main loop...");
    
    // Check window state before loop
    bool should_close = SituationWindowShouldClose();
    CONSOLE_LOG("Window should close: %d", should_close);
    
    int frame_count = 0;
    const char* capture_path = getenv("KTERM_CAPTURE_SCREENSHOT");
    const bool capture_exit = getenv("KTERM_CAPTURE_EXIT") != NULL;
    int capture_frame = 0;

    // Main loop
    while (!SituationWindowShouldClose() && !should_exit) {
        frame_count++;
        
        // Poll events first
        SituationPollInputEvents();
        SituationUpdateTimers();

        if (term && SituationIsWindowResized()) {
            int w, h;
            SituationGetWindowSize(&w, &h);
            int cols = w / (DEFAULT_CHAR_WIDTH * 2);
            int rows = h / (DEFAULT_CHAR_HEIGHT * 2);
            KTerm_Resize(term, cols, rows);
            if (shell_mode) {
                KTShell_Resize(&shell_proc, cols, rows);
            }
        }

        // Shell mode: poll shell output and pipe to terminal
        if (shell_mode) {
            char shell_buf[4096];
            size_t n = KTShell_Read(&shell_proc, shell_buf, sizeof(shell_buf));
            if (n > 0) {
                KTerm_PushInput(term, shell_buf, n);
            }
            // Check if shell exited
            if (!KTShell_IsRunning(&shell_proc)) {
                shell_mode = false;
                KTShell_Stop(&shell_proc);
                KTerm_WriteString(term, "\n\x1B[33mShell exited.\x1B[0m\n");
                SituationSetWindowTitle("KaOS - Kaizen Operating System v0.1");
                // Re-enable built-in mode immediately
                console.prompt_pending = true;
                console.input_enabled = false;
                console.in_command = false;
                console.waiting_for_prompt_cursor_pos = false;
            }
        }

        if (!shell_mode && term && console.prompt_pending && !console.in_command && !console.waiting_for_prompt_cursor_pos) {
            CONSOLE_LOG("CLI MainLoop: Calling ShowPrompt.");
            ShowPrompt();
        }
        
        // Process input via K-Term's adapter
        if (term) {
            // In shell mode: capture keys directly and send to shell's stdin
            // (bypass kterm's input event pipeline for keyboard)
            if (shell_mode) {
                int rk;
                while ((rk = SituationGetKeyPressed()) != 0) {
                    char seq[8] = {0};
                    bool ctrl = SituationIsKeyDown(SIT_KEY_LEFT_CONTROL) || SituationIsKeyDown(SIT_KEY_RIGHT_CONTROL);
                    // Handle special keys
                    if (rk == SIT_KEY_ENTER)       { seq[0] = '\r'; }
                    else if (rk == SIT_KEY_BACKSPACE) { seq[0] = '\x08'; }
                    else if (rk == SIT_KEY_TAB)      { seq[0] = '\t'; }
                    else if (rk == SIT_KEY_ESCAPE)   { seq[0] = '\x1B'; }
                    else if (rk == SIT_KEY_UP)       { seq[0] = '\x1B'; seq[1] = '['; seq[2] = 'A'; }
                    else if (rk == SIT_KEY_DOWN)     { seq[0] = '\x1B'; seq[1] = '['; seq[2] = 'B'; }
                    else if (rk == SIT_KEY_RIGHT)    { seq[0] = '\x1B'; seq[1] = '['; seq[2] = 'C'; }
                    else if (rk == SIT_KEY_LEFT)     { seq[0] = '\x1B'; seq[1] = '['; seq[2] = 'D'; }
                    else if (rk == SIT_KEY_HOME)     { seq[0] = '\x1B'; seq[1] = '['; seq[2] = 'H'; }
                    else if (rk == SIT_KEY_END)      { seq[0] = '\x1B'; seq[1] = '['; seq[2] = 'F'; }
                    else if (rk == SIT_KEY_DELETE)    { seq[0] = '\x1B'; seq[1] = '['; seq[2] = '3'; seq[3] = '~'; }
                    else if (ctrl && rk >= 'A' && rk <= 'Z') { seq[0] = (char)(rk - 'A' + 1); }
                    else if (ctrl && rk >= 'a' && rk <= 'z') { seq[0] = (char)(rk - 'a' + 1); }
                    if (seq[0]) {
                        KTShell_Write(&shell_proc, seq, strlen(seq));
                    }
                }
                // Printable characters (with correct case from OS)
                int ch;
                while ((ch = SituationGetCharPressed()) != 0) {
                    char buf[4];
                    if (ch < 0x80) {
                        buf[0] = (char)ch;
                        KTShell_Write(&shell_proc, buf, 1);
                    }
                }
            } else {
                KTermSit_ProcessInput(term);
            }
            // Always update mouse tracking (even in shell mode)
            if (shell_mode) {
                KTermSit_UpdateMouse(term);
            }

            // Hide OS cursor when mouse is over our window, show when it leaves
            {
                Vector2 mpos = SituationGetMousePosition();
                int ww, wh;
                SituationGetWindowSize(&ww, &wh);
                bool mouse_in_window = (mpos.x >= 0 && mpos.y >= 0 && mpos.x < ww && mpos.y < wh);
                static bool cursor_hidden = false;
                if (mouse_in_window && !cursor_hidden) {
                    SituationHideCursor();
                    cursor_hidden = true;
                } else if (!mouse_in_window && cursor_hidden) {
                    SituationShowCursor();
                    cursor_hidden = false;
                }
            }

            KTerm_Update(term);

#ifdef KTERM_STANDALONE_MODE
            // Standalone: KTerm_Draw handles the full frame lifecycle internally
            KTerm_Draw(term);
            if (capture_path && ++capture_frame == 30) {
                SituationTakeScreenshot(capture_path);
                if (capture_exit) should_exit = true;
            }
#else
            // VD mode: host owns frame lifecycle, kterm is a compositable layer
            SituationAcquireFrameCommandBuffer();
            KTerm_Draw(term);
            SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
            SituationRenderVirtualDisplays(cmd);
            SituationEndFrame();
#endif
            if (capture_path && ++capture_frame == 30) {
                SituationTakeScreenshot(capture_path);
                if (capture_exit) should_exit = true;
            }
        }
    }
    
    // Clean up shell if still running
    if (shell_mode) {
        KTShell_Stop(&shell_proc);
    }

    SituationShutdown();

    KTerm_Destroy(term);
    
    return 0;
}
