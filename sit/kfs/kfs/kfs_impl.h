/**
 * @file kfs/kfs_impl.h
 * @brief KFS implementation orchestrator — include chain only (no function bodies).
 *
 * Do not include directly. Define KFS_IMPLEMENTATION in exactly one TU, then #include "kfs.h".
 * Bodies live in kfs_impl_core.h, kfs_impl_auth.h, kfs_impl_lc.h (see doc/done/impl_split_plan.md).
 */
#ifndef KFS_IMPL_H
#define KFS_IMPL_H

#ifdef KFS_IMPLEMENTATION

#include "kfs_api.h"
#include "kfs_impl_fwd.h"
#include "kfs_impl_core.h"
#include "kfs_impl_auth.h"
#include "kfs_impl_lc.h"

#endif /* KFS_IMPLEMENTATION */

#endif /* KFS_IMPL_H */

