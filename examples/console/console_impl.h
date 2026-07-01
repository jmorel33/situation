/**********************************************************************************************
 *
 * @file console_impl.h
 *   (c) 2025-2026 Jacques Morel
 * @brief Internal implementation orchestrator for KaOS Terminal Console
 *
 **********************************************************************************************/
#ifndef CONSOLE_IMPL_H
#define CONSOLE_IMPL_H

#if defined(_WIN32)
  #define NOMINMAX
#endif

#define KTERM_IMPLEMENTATION
#define STB_TRUETYPE_IMPLEMENTATION
#include "sit/k-term/kterm.h"

#define KTERM_IO_SIT_IMPLEMENTATION
#include "kt_io_sit.h"

#define KT_SHELL_IMPLEMENTATION
#include "sit/k-term/kt_shell.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

// Include forward declarations so that all modules can reference each other
#include "console_forward.h"

// Include all implementation files
#include "console_impl/console_config.h"
#include "console_impl/console_state.h"
#include "console_impl/console_utils.h"
#include "console_impl/console_cli_edit.h"
#include "console_impl/console_sysinfo.h"
#include "console_impl/console_ed.h" // The new editor header
#include "console_impl/console_vt_ui.h"
#include "console_impl/console_commands.h"
#include "console_impl/console_cli_input.h"
#include "console_impl/console_response.h"
#include "console_impl/console_lifecycle.h"

#endif /* CONSOLE_IMPL_H */
