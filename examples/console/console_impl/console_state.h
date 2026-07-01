#ifndef CONSOLE_STATE_H
#define CONSOLE_STATE_H

// Define the global console state variables (declared as extern in console_forward.h)
bool should_exit = false;
bool console_initialized = false;
int capture_frame = 0;
const char* capture_path = NULL;
bool capture_exit = false;

Console console = {0};
KTerm* term = NULL;
CursorPositionTracker cursor_tracker = {0};
KTShell shell_proc = {0};
bool shell_mode = false;  // true = pass-through to shell subprocess

#endif /* CONSOLE_STATE_H */
