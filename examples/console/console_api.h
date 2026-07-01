/**********************************************************************************************
 *
 * @file console_api.h
 *   (c) 2025-2026 Jacques Morel
 * @brief Public API definitions for the KaOS Terminal Console
 *
 **********************************************************************************************/
#ifndef CONSOLE_API_H
#define CONSOLE_API_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ConsoleConfig {
    int term_cols;
    int term_rows;
    int settle_delay_ms;
} ConsoleConfig;

#define CONSOLE_CONFIG_DEFAULT { .term_cols = 80, .term_rows = 50, .settle_delay_ms = 100 }

bool Console_Init(const ConsoleConfig* config);
void Console_Update(void);
void Console_Shutdown(void);
bool Console_ShouldExit(void);

#ifdef __cplusplus
}
#endif

#endif /* CONSOLE_API_H */
