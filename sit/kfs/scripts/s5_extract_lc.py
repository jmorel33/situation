#!/usr/bin/env python3
"""S5: Extract lc implementation from kfs_impl.h into kfs_impl_lc.h."""
from pathlib import Path

BASE = Path(__file__).resolve().parent.parent / "kfs"
IMPL = BASE / "kfs_impl.h"
LC = BASE / "kfs_impl_lc.h"

lines = IMPL.read_text(encoding="utf-8").splitlines(keepends=True)

# 1-based inclusive: epics through kfs_validate_script (before #endif)
LC_START = 17
LC_END = 6893

extracted = lines[LC_START - 1 : LC_END]

header = """/**
 * @file kfs_impl_lc.h
 * @brief KFS implementation — linking & content (architecture.db + artifacts.db).
 *
 * LC = Linking & Content (not lifecycle — see kfs_impl_core.h).
 * Included only via kfs_impl.h when KFS_IMPLEMENTATION is defined.
 *
 * Split phase: S5 (extracted from kfs_impl.h).
 */
#ifndef KFS_IMPL_LC_H
#define KFS_IMPL_LC_H

#ifdef KFS_IMPLEMENTATION

/* SECTION: epics (both monolith banners) — S5.1 */
/* SECTION: topics — S5.2 */
/* SECTION: notes (both monolith banners) — S5.3 */
/* SECTION: linking + artifacts — S5.4 */
/* SECTION: advanced load (both monolith banners) — S5.5 */
/* SECTION: misc (validate_script, orphans) — S5.6 */

"""

footer = """
#endif /* KFS_IMPLEMENTATION */

#endif /* KFS_IMPL_LC_H */
"""

LC.write_text(header + "".join(extracted) + footer, encoding="utf-8")

remove = set(range(LC_START, LC_END + 1))
new_lines = [line for i, line in enumerate(lines, start=1) if i not in remove]
out = "".join(new_lines)

needle = '#include "kfs_impl_auth.h"\n'
insert = '#include "kfs_impl_auth.h"\n#include "kfs_impl_lc.h"\n'
if needle not in out:
    raise SystemExit("Could not find kfs_impl_auth.h include in kfs_impl.h")
out = out.replace(needle, insert, 1)

IMPL.write_text(out, encoding="utf-8")
print(f"Wrote {LC.name}: {len((header + ''.join(extracted) + footer).splitlines())} lines")
print(f"Wrote {IMPL.name}: {len(out.splitlines())} lines")