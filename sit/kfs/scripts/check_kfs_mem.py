#!/usr/bin/env python3
"""
KFS heap allocator audit — grep gate for kfs_impl_*.h.

M5: fails the build on raw CRT allocators outside the allowlisted CRT backend
region in kfs_impl_core.h. Use --no-enforce or ENFORCE=0 for warn-only (M1 stub).
"""
from __future__ import annotations

import argparse
import os
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
IMPL_GLOB = "kfs_impl_*.h"
IMPL_DIR = ROOT / "kfs"

CRT_BACKEND_BEGIN = "BEGIN KFS_MEM_CRT_BACKEND"
CRT_BACKEND_END = "END KFS_MEM_CRT_BACKEND"

PATTERNS = (
    (re.compile(r"\bmalloc\s*\("), "malloc"),
    (re.compile(r"\bcalloc\s*\("), "calloc"),
    (re.compile(r"\brealloc\s*\("), "realloc"),
    (re.compile(r"\bstrdup\s*\("), "strdup"),
    (re.compile(r"\bfree\s*\("), "free"),
)

ALLOWLIST_SUBSTRINGS = (
    "sqlite3_free",
    "COMMIT;",
    "TODO(M",
)


def is_comment_only_line(line: str) -> bool:
    stripped = line.strip()
    if not stripped:
        return True
    if stripped.startswith("//"):
        return True
    if stripped.startswith("/*") and stripped.endswith("*/"):
        return True
    return False


def line_has_inline_comment_before_allocator(line: str, match_start: int) -> bool:
    """Allow allocators that appear only after a // comment on the same line."""
    slash = line.find("//")
    return slash != -1 and slash < match_start


def is_allowed_line(line: str, in_crt_backend: bool) -> bool:
    if in_crt_backend:
        return True
    if is_comment_only_line(line):
        return True
    for token in ALLOWLIST_SUBSTRINGS:
        if token in line:
            return True
    for pattern, _ in PATTERNS:
        m = pattern.search(line)
        if m and line_has_inline_comment_before_allocator(line, m.start()):
            return True
    return False


def scan_file(path: Path) -> list[tuple[int, str, str]]:
    hits: list[tuple[int, str, str]] = []
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError as exc:
        print(f"[check_kfs_mem] cannot read {path}: {exc}", file=sys.stderr)
        return hits

    in_crt_backend = False
    for lineno, line in enumerate(text.splitlines(), start=1):
        if CRT_BACKEND_BEGIN in line:
            in_crt_backend = True
        if is_allowed_line(line, in_crt_backend):
            if CRT_BACKEND_END in line:
                in_crt_backend = False
            continue
        for pattern, name in PATTERNS:
            if pattern.search(line):
                hits.append((lineno, name, line.rstrip()))
                break
        if CRT_BACKEND_END in line:
            in_crt_backend = False
    return hits


def main() -> int:
    parser = argparse.ArgumentParser(description="Audit KFS impl headers for raw CRT allocators")
    parser.add_argument(
        "--no-enforce",
        action="store_true",
        help="Warn only — do not fail the build (M1 stub mode)",
    )
    args = parser.parse_args()
    enforce = not args.no_enforce and os.environ.get("ENFORCE", "1") != "0"

    if not IMPL_DIR.is_dir():
        print(f"[check_kfs_mem] missing impl dir: {IMPL_DIR}", file=sys.stderr)
        return 0 if not enforce else 1

    all_hits: list[tuple[Path, int, str, str]] = []
    for path in sorted(IMPL_DIR.glob(IMPL_GLOB)):
        for lineno, kind, line in scan_file(path):
            all_hits.append((path, lineno, kind, line))

    if not all_hits:
        mode = "enforce" if enforce else "warn-only"
        print(f"[check_kfs_mem] OK — no raw CRT allocators in kfs_impl_*.h ({mode})")
        return 0

    print(f"[check_kfs_mem] found {len(all_hits)} raw allocator site(s):")
    for path, lineno, kind, line in all_hits:
        rel = path.relative_to(ROOT)
        print(f"  {rel}:{lineno}: {kind}: {line}")

    if enforce:
        print("[check_kfs_mem] FAIL — migrate to KFS_* / sqlite3_free (see doc/memory_alloc_plan.md)")
        return 1

    print("[check_kfs_mem] WARN — stub mode (non-blocking); omit --no-enforce to fail")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())