#!/usr/bin/env python3
"""S3: Extract core implementation from kfs_impl.h into kfs_impl_core.h."""
from pathlib import Path

BASE = Path(__file__).resolve().parent.parent / "kfs"
IMPL = BASE / "kfs_impl.h"
CORE = BASE / "kfs_impl_core.h"

lines = IMPL.read_text(encoding="utf-8").splitlines(keepends=True)

# 1-based inclusive line ranges to extract
RANGES = [
    (12, 44),       # S3.1 platform
    (49, 376),      # S3.2 helpers
    (1110, 1434),   # S3.3 kfs_init
    (13532, 13894), # S3.4 memory frees
]

extracted = []
for start, end in RANGES:
    extracted.extend(lines[start - 1 : end])

header = """/**
 * @file kfs_impl_core.h
 * @brief KFS implementation — platform shims, DB lifecycle, utilities, memory frees.
 *
 * Included only via kfs_impl.h when KFS_IMPLEMENTATION is defined.
 * No business permission rules (kfs_check_permission lives in kfs_impl_auth.h).
 *
 * Split phase: S3 (extracted from kfs_impl.h).
 */
#ifndef KFS_IMPL_CORE_H
#define KFS_IMPL_CORE_H

#ifdef KFS_IMPLEMENTATION

/* SECTION: platform + CRT — S3.1 */
/* SECTION: static helpers — S3.2 */
/* SECTION: kfs_init / kfs_close — S3.3 */
/* SECTION: memory frees — S3.4 */

"""

footer = """
#endif /* KFS_IMPLEMENTATION */

#endif /* KFS_IMPL_CORE_H */
"""

CORE.write_text(header + "".join(extracted) + footer, encoding="utf-8")

remove = set()
for start, end in RANGES:
    for i in range(start, end + 1):
        remove.add(i)

new_lines = [line for i, line in enumerate(lines, start=1) if i not in remove]
out = "".join(new_lines)

needle = "#ifdef KFS_IMPLEMENTATION\n\n"
insert = (
    "#ifdef KFS_IMPLEMENTATION\n\n"
    '#include "kfs_api.h"\n'
    '#include "kfs_impl_fwd.h"\n'
    '#include "kfs_impl_core.h"\n\n'
)
if needle in out:
    out = out.replace(needle, insert, 1)
else:
    raise SystemExit("Could not find KFS_IMPLEMENTATION block in kfs_impl.h")

IMPL.write_text(out, encoding="utf-8")
print(f"Wrote {CORE.name}: {len((header + ''.join(extracted) + footer).splitlines())} lines")
print(f"Wrote {IMPL.name}: {len(out.splitlines())} lines")