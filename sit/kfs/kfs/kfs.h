/**
 * @file kfs/kfs/kfs.h
 * @brief Kaizen Filing System (KFS) — SQLite-backed asset and knowledge store.
 *
 * (c) 2025-2026 Jacques Morel — MIT Licensed
 *
 * KFS is a domain-isolated, ACL-secured content store (Artifacts, Topics, Epics,
 * Notes, Actors, Domains). See doc/architecture.md (internals) and doc/kfs_guide.md (security).
 *
 * Include contract (add -I sit/kfs to your compile flags):
 *
 * @code
 * // In exactly one translation unit:
 * #define KFS_IMPLEMENTATION
 * #include "kfs/kfs.h"
 *
 * // Everywhere else:
 * #include "kfs/kfs.h"
 * @endcode
 *
 * Alternate with only -I sit: #include "kfs/kfs/kfs.h"
 *
 * Bootstrap order (memory — required for embedders using SQLite elsewhere):
 *   1. kfs_mem_init(cfg)  — before any KFS or sqlite3_* call (cfg NULL = defaults)
 *   2. kfs_init(...)      — calls kfs_mem_init(NULL) if step 1 was skipped
 *   3. kfs_close / kfs_mem_shutdown — close all GameDB handles before shutdown
 *
 * API-returned pointers use kfs_mem_free() (same heap as KFS_MALLOC). See kfs_mem.h
 * and doc/memory_alloc_plan.md for caps, macro overrides, and SQLite exceptions.
 *
 * @version 2.3.0
 * @date June 30, 2026
 */
#include "kfs_version.h"

#ifndef KFS_H
#define KFS_H

#include "kfs_api.h"

#ifdef KFS_IMPLEMENTATION
#include "kfs_impl.h"
#endif /* KFS_IMPLEMENTATION */

#endif /* KFS_H */