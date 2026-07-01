/**
 * @file kfs_version.h
 * @brief KFS version macros — canonical source of truth.
 *
 * (c) 2025-2026 Jacques Morel — MIT Licensed
 *
 * Bump only this file when releasing a new library version.
 * All other KFS headers include this file; never define version macros elsewhere.
 *
 * The runtime string is available via kfs_get_version_string() declared in
 * kfs_api.h and implemented in kfs_impl_core.h.
 */
#ifndef KFS_VERSION_H
#define KFS_VERSION_H

#define KFS_VERSION_MAJOR       2
#define KFS_VERSION_MINOR       3
#define KFS_VERSION_PATCH       0
#define KFS_VERSION_REVISION    ""
#define KFS_VERSION_DESCRIPTION "Unified memory (KFS_* + SQLite vtable); optional MyBuddy profile C"

#endif /* KFS_VERSION_H */