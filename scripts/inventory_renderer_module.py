#!/usr/bin/env python3
"""Post-split renderer module inventory — lines, statics, SITAPI per file.

Static counts use unique names per file (same rules as verify_renderer_fwd.py).
The impl union total matches the 347/347 fwd gate — not a naive per-file sum.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SIT = ROOT / "sit"

SLICES = [
    "situation_impl_renderer.h",
    "situation_impl_renderer_core.h",
    "situation_impl_renderer_lc.h",
    "situation_impl_renderer_shader.h",
    "situation_impl_renderer_resources.h",
    "situation_impl_renderer_frame_cmd.h",
]
FWD = "situation_impl_renderer_fwd.h"

STATIC_RE = re.compile(
    r"^\s*static\s+(?:inline\s+)?(?:const\s+)?(?:[\w\s\*_]+?)\s+(_\w+)\s*\(", re.M
)
SITAPI_RE = re.compile(r"^SITAPI\s+", re.M)

BASELINE_MONOLITH_LINES = 28_105
BASELINE_STATICS = 345


def static_names(text: str) -> set[str]:
    return set(STATIC_RE.findall(text))


def main() -> int:
    print("Renderer module inventory")
    print("=" * 78)
    print(f"{'File':<42} {'Lines':>7} {'Statics':>8} {'SITAPI':>7}")
    print("-" * 78)

    body_lines = 0
    body_sitapi = 0
    all_lines = 0
    union_statics: set[str] = set()

    for name in SLICES:
        path = SIT / name
        text = path.read_text(encoding="utf-8")
        lines = len(text.splitlines())
        statics = static_names(text)
        sitapi = len(SITAPI_RE.findall(text))
        all_lines += lines
        body_lines += lines
        body_sitapi += sitapi
        union_statics |= statics
        print(f"{name:<42} {lines:>7} {len(statics):>8} {sitapi:>7}")

    fwd_path = SIT / FWD
    fwd_text = fwd_path.read_text(encoding="utf-8")
    fwd_lines = len(fwd_text.splitlines())
    fwd_statics = static_names(fwd_text)
    all_lines += fwd_lines
    print(f"{FWD:<42} {fwd_lines:>7} {len(fwd_statics):>8} {'n/a':>7}")

    print("-" * 78)
    print(
        f"{'TOTAL impl (orchestrator + 5 slices)':<42} {body_lines:>7} "
        f"{len(union_statics):>8} {body_sitapi:>7}"
    )
    print(f"{'TOTAL incl. renderer_fwd.h':<42} {all_lines:>7}")
    print()
    print(f"Baseline monolith (pre-R0):     {BASELINE_MONOLITH_LINES:>7} lines  {BASELINE_STATICS} statics")
    print(
        f"Impl delta vs baseline:       {body_lines - BASELINE_MONOLITH_LINES:>+7} lines  "
        f"{len(union_statics) - BASELINE_STATICS:+d} statics (union)"
    )
    print("  (line delta = slice headers/guards/banners; mechanical move only)")
    print()
    print("Largest slice:", max(SLICES[1:], key=lambda n: len((SIT / n).read_text().splitlines())))
    print()
    print("Fwd gate: run  python scripts/verify_renderer_fwd.py")
    return 0


if __name__ == "__main__":
    sys.exit(main())
