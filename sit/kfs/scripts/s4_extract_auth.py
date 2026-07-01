#!/usr/bin/env python3
"""S4: Extract auth implementation from kfs_impl.h into kfs_impl_auth.h."""
from pathlib import Path

BASE = Path(__file__).resolve().parent.parent / "kfs"
IMPL = BASE / "kfs_impl.h"
AUTH = BASE / "kfs_impl_auth.h"

lines = IMPL.read_text(encoding="utf-8").splitlines(keepends=True)

# 1-based inclusive: legacy users through legacy kfs_add_user block (before EPIC)
AUTH_START = 16
AUTH_END = 5962

extracted = lines[AUTH_START - 1 : AUTH_END]

header = """/**
 * @file kfs_impl_auth.h
 * @brief KFS implementation — registry.db (actors, domains, schemes, permission).
 *
 * Included only via kfs_impl.h when KFS_IMPLEMENTATION is defined.
 *
 * Split phase: S4 (extracted from kfs_impl.h).
 */
#ifndef KFS_IMPL_AUTH_H
#define KFS_IMPL_AUTH_H

#ifdef KFS_IMPLEMENTATION

/* SECTION: bootstrap + legacy users — S4.1 */
/* SECTION: actors & groups — S4.2 */
/* SECTION: security schemes + kfs_check_permission — S4.3 */
/* SECTION: domains + entity policy — S4.4 */
/* SECTION: legacy kfs_add_user — S4.5 */

"""

footer = """
#endif /* KFS_IMPLEMENTATION */

#endif /* KFS_IMPL_AUTH_H */
"""

AUTH.write_text(header + "".join(extracted) + footer, encoding="utf-8")

remove = set(range(AUTH_START, AUTH_END + 1))
new_lines = [line for i, line in enumerate(lines, start=1) if i not in remove]
out = "".join(new_lines)

needle = '#include "kfs_impl_core.h"\n'
insert = '#include "kfs_impl_core.h"\n#include "kfs_impl_auth.h"\n'
if needle not in out:
    raise SystemExit('Could not find kfs_impl_core.h include in kfs_impl.h')
out = out.replace(needle, insert, 1)

IMPL.write_text(out, encoding="utf-8")
print(f"Wrote {AUTH.name}: {len((header + ''.join(extracted) + footer).splitlines())} lines")
print(f"Wrote {IMPL.name}: {len(out.splitlines())} lines")