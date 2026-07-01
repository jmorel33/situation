#!/usr/bin/env python3
"""Verify situation_impl_forward.h covers all static defs in non-renderer impl files.

Counterpart to verify_renderer_fwd.py (which checks renderer_fwd.h vs renderer.h).
This script checks that every static function defined in the non-renderer impl files
has a forward declaration in situation_impl_forward.h.

Usage:
    python scripts/verify_impl_forward.py
    python scripts/verify_impl_forward.py --fix   # print suggested forward decls for missing
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SIT = ROOT / "sit"
AUD = SIT / "aud"

# The file we're verifying
FORWARD_H = SIT / "situation_impl_forward.h"

# Files whose statics should be covered by situation_impl_forward.h
# (everything except renderer.h and renderer_fwd.h, which have their own verifier,
#  and the orchestrator/deps/decl/forward files themselves which don't define functions)
EXCLUDED = {
    "situation_impl.h",             # orchestrator, no function defs
    "situation_impl_decl.h",        # struct declarations only
    "situation_impl_deps.h",        # include ordering only
    "situation_impl_forward.h",     # this IS the forward file
    "situation_impl_renderer.h",    # covered by verify_renderer_fwd.py
    "situation_impl_renderer_fwd.h",  # forward file for renderer
}

# Regex to capture static function definitions (name is the leading-underscore identifier)
# Matches: static <qualifiers> <return_type> _FunctionName(
STATIC_DEF_RE = re.compile(
    r"^\s*static\s+(?:inline\s+)?(?:const\s+)?(?:[\w\s\*_]+?)\s+(_\w+)\s*\(",
    re.MULTILINE,
)

# Regex to detect if a definition is `static inline` (these don't need forward declarations)
STATIC_INLINE_RE = re.compile(
    r"^\s*static\s+inline\s+",
    re.MULTILINE,
)

# Regex to capture forward declarations in the forward.h (same pattern but in that file)
STATIC_DECL_RE = STATIC_DEF_RE


def find_static_defs(path: Path, include_inline: bool = True) -> dict[str, tuple[str, int]]:
    """Return {name: (file_stem, line_number)} for all static function defs.
    If include_inline=False, skip `static inline` definitions."""
    results: dict[str, tuple[str, int]] = {}
    text = path.read_text(encoding="utf-8", errors="replace")
    for i, line in enumerate(text.splitlines(), 1):
        m = STATIC_DEF_RE.match(line)
        if m:
            if not include_inline and STATIC_INLINE_RE.match(line):
                continue
            name = m.group(1)
            results[name] = (path.name, i)
    return results


def find_static_decls(path: Path) -> set[str]:
    """Return set of names forward-declared in the given file."""
    text = path.read_text(encoding="utf-8", errors="replace")
    return set(STATIC_DECL_RE.findall(text))


def get_impl_files() -> list[Path]:
    """Return sorted list of non-excluded situation_impl*.h files."""
    files = sorted(SIT.glob("situation_impl*.h"))
    return [f for f in files if f.name not in EXCLUDED]


def get_full_signature(path: Path, name: str) -> str | None:
    """Extract the full function signature line for a given static function name."""
    text = path.read_text(encoding="utf-8", errors="replace")
    # Look for the definition line
    pattern = re.compile(
        r"^(\s*static\s+(?:inline\s+)?(?:const\s+)?[\w\s\*_]+?" + re.escape(name) + r"\s*\([^)]*\))",
        re.MULTILINE,
    )
    m = pattern.search(text)
    if m:
        sig = m.group(1).strip()
        # Trim to declaration form (add semicolon, remove opening brace context)
        return sig + ";"
    return None


def main() -> int:
    fix_mode = "--fix" in sys.argv

    # Gather all NON-INLINE static definitions in non-renderer impl files.
    # static inline functions don't need forward declarations — they're self-contained
    # and the compiler sees the full body before any call site in include order.
    all_defs: dict[str, tuple[str, int]] = {}
    impl_files = get_impl_files()

    for f in impl_files:
        defs = find_static_defs(f, include_inline=False)
        all_defs.update(defs)

    # Gather what's already declared in situation_impl_forward.h
    declared = find_static_decls(FORWARD_H)

    # Also check what's declared in situation_impl_renderer_fwd.h (shouldn't overlap,
    # but avoid false positives for cross-referenced helpers)
    renderer_fwd = SIT / "situation_impl_renderer_fwd.h"
    if renderer_fwd.exists():
        declared |= find_static_decls(renderer_fwd)

    # Find missing
    missing = sorted(set(all_defs.keys()) - declared)

    # Find extras (declared in forward.h but not defined in any non-renderer impl file)
    # Only check names that aren't in the renderer (those are expected to be in renderer_fwd)
    # Include inline defs when checking for extras (a forward decl of an inline is not "extra"
    # if the inline exists, just unnecessary)
    all_impl_names_including_inline: set[str] = set()
    for f in impl_files:
        all_impl_names_including_inline |= set(find_static_defs(f, include_inline=True).keys())

    forward_decls = find_static_decls(FORWARD_H)
    extra = sorted(forward_decls - all_impl_names_including_inline)

    # For extras, also exclude things defined in renderer.h (they'd be cross-refs)
    renderer_h = SIT / "situation_impl_renderer.h"
    renderer_defs: set[str] = set()
    if renderer_h.exists():
        renderer_defs = set(find_static_defs(renderer_h).keys())
    # Also exclude things defined in sit/aud/ (audio effects subdirectory)
    aud_defs: set[str] = set()
    if AUD.exists():
        for f in AUD.rglob("*.h"):
            aud_defs |= set(find_static_defs(f).keys())
    extra = [n for n in extra if n not in renderer_defs and n not in aud_defs]

    if missing:
        print(f"MISSING: {len(missing)} static function(s) lack a forward declaration in situation_impl_forward.h:")
        print()
        by_file: dict[str, list[str]] = {}
        for name in missing:
            fname, line = all_defs[name]
            by_file.setdefault(fname, []).append(name)

        for fname in sorted(by_file.keys()):
            print(f"  [{fname}]")
            for name in sorted(by_file[fname]):
                _, line = all_defs[name]
                print(f"    {name}  (line {line})")
            print()

        if fix_mode:
            print("=" * 72)
            print("SUGGESTED FORWARD DECLARATIONS:")
            print("=" * 72)
            for fname in sorted(by_file.keys()):
                fpath = SIT / fname
                print(f"\n// From {fname}:")
                for name in sorted(by_file[fname]):
                    sig = get_full_signature(fpath, name)
                    if sig:
                        print(sig)
                    else:
                        print(f"// Could not extract signature for {name}")
            print()

    if extra:
        print(f"EXTRA: {len(extra)} forward decl(s) in situation_impl_forward.h with no matching definition:")
        for n in extra:
            print(f"  - {n}")
        print()

    if not missing and not extra:
        print(
            f"OK: situation_impl_forward.h covers all {len(all_defs)} non-inline static functions "
            f"across {len(impl_files)} non-renderer impl files. "
            f"(static inline functions are excluded — they don't need forward declarations.)"
        )
        return 0

    total_issues = len(missing) + len(extra)
    return 1 if total_issues > 0 else 0


if __name__ == "__main__":
    sys.exit(main())
