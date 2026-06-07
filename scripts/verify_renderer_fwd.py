#!/usr/bin/env python3
"""Verify situation_impl_renderer_fwd.h matches static defs in situation_impl_renderer.h.

Other static helpers are declared in their own units (wdm, vd, audio) or
situation_impl_forward.h (render thread). Public API is sit/situation_api.h only.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RENDERER_H = ROOT / "sit" / "situation_impl_renderer.h"
RENDERER_FWD = ROOT / "sit" / "situation_impl_renderer_fwd.h"
OTHER_FWD = (
    ROOT / "sit" / "situation_impl_forward.h",
    ROOT / "sit" / "situation_impl_audio.h",
    ROOT / "sit" / "situation_impl_wdm.h",
    ROOT / "sit" / "situation_impl_vd.h",
)

STATIC_RE = re.compile(
    r"^\s*static\s+(?:inline\s+)?(?:const\s+)?(?:[\w\s\*_]+?)\s+(_\w+)\s*\(", re.M
)


def static_names(path: Path) -> set[str]:
    text = path.read_text(encoding="utf-8", errors="replace")
    return set(STATIC_RE.findall(text))


def main() -> int:
    impl = static_names(RENDERER_H)
    fwd_renderer = static_names(RENDERER_FWD)
    other: set[str] = set()
    for p in OTHER_FWD:
        if p.exists():
            other |= static_names(p)

    missing = sorted(impl - fwd_renderer - other)
    extra = sorted(fwd_renderer - impl)

    if missing:
        print(f"FAIL: {len(missing)} static(s) in renderer.h missing from renderer_fwd.h:")
        for n in missing:
            print(f"  + {n}")
    if extra:
        print(f"FAIL: {len(extra)} forward decl(s) in renderer_fwd.h with no renderer.h def:")
        for n in extra:
            print(f"  - {n}")

    if missing or extra:
        return 1

    print(
        f"OK: renderer_fwd.h covers all {len(impl)} static functions in situation_impl_renderer.h"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
