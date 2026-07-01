/**********************************************************************************************
 *
 * @file console_forward.h
 *   (c) 2025-2026 Jacques Morel
 * @brief Forward declarations and global variable declarations for the console example.
 *
 **********************************************************************************************/
#ifndef CONSOLE_FORWARD_H
#define CONSOLE_FORWARD_H

#include <stdbool.h>
#include <stddef.h>

// Forward declare structures/typedefs from other headers if needed
typedef struct CursorPositionTracker CursorPositionTracker;
typedef struct Console Console;

// --- Global Variable Declarations (extern) ---
extern bool should_exit;
extern bool console_initialized;
extern int capture_frame;
extern const char* capture_path;
extern bool capture_exit;

extern Console console;
extern KTerm* term;
extern CursorPositionTracker cursor_tracker;
extern KTShell shell_proc;
extern bool shell_mode;

// --- Forward Declarations of Internal Helper Functions ---
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

// Forward declarations for situation.h helper print functions
void SitHelperPrintDeviceInfo(void);
void SitHelperPrintDisplayInfo(SituationDisplayInfo* displays, int count);
void SitHelperPrintAudioDeviceInfo(SituationAudioDeviceInfo* devices, int count);

// --- Text Editor Prototypes ---
static void EditorEnter(const char* filepath, bool is_new);
static void EditorExit(void);
static void EditorRelayout(void);
static void EditorSave(void);
static void EditorUndo(void);
static bool EditorProcessInput(const char* response_data, size_t length);
static bool EditorHandleResponse(const char* response_data, size_t length);

static bool ConsoleVtWidgetIsActive(void);
static bool ConsoleVtMenuIsActive(void);
static bool ConsoleVtMenuHandleResponse(const char* response_data, size_t length);
static void ConsoleVtDemoStyles(KTerm* t);
static void ConsoleVtRunBoxCommand(char* tokens[], int token_count);
static void ConsoleVtRunMenuCommand(char* tokens[], int token_count);
static void ConsoleVtRunComboCommand(char* tokens[], int token_count);
static void ConsoleVtRunDialogCommand(char* tokens[], int token_count);
static void ConsoleVtRunStylesCaptureCommand(void);

#endif /* CONSOLE_FORWARD_H */
