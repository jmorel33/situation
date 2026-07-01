/**
 * @file kfs/kfs.c
 * @brief Single translation unit that instantiates the KFS implementation.
 *
 * (c) 2025-2026 Jacques Morel — MIT Licensed
 *
 * Link the resulting object (or libkfs.a) into any binary that uses KFS.
 * Other TUs include kfs.h without KFS_IMPLEMENTATION.
 */
#ifdef KFS_MEM_USE_MYBUDDY
#define MYBUDDY_IMPLEMENTATION
#include "mybuddy/mybuddy.h"
#endif

#define KFS_IMPLEMENTATION
#include "kfs.h"