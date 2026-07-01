#ifndef CONSOLE_CONFIG_H
#define CONSOLE_CONFIG_H

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

typedef struct CursorPositionTracker {
    bool waiting_for_position;
    bool position_received;
    int row;
    int col;
} CursorPositionTracker;

// Console state structure
typedef struct Console {
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

#endif /* CONSOLE_CONFIG_H */
