#!/usr/bin/env python3
"""Verify situation_impl_renderer_fwd.h matches static defs in renderer slice bodies.

Scans sit/situation_impl_renderer.h (orchestrator — usually no statics) and all
sit/situation_impl_renderer_*.h slice files except *_fwd.h.

Other static helpers are declared in their own units (wdm, vd, audio) or
situation_impl_forward.h (render thread). Public API is sit/situation_api.h only.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SIT = ROOT / "sit"
RENDERER_FWD = SIT / "situation_impl_renderer_fwd.h"
OTHER_FWD = (
    SIT / "situation_impl_forward.h",
    SIT / "situation_impl_audio.h",
    SIT / "situation_impl_wdm.h",
    SIT / "situation_impl_vd.h",
)

STATIC_RE = re.compile(
    r"^\s*static\s+(?:inline\s+)?(?:const\s+)?(?:[\w\s\*_]+?)\s+(_\w+)\s*\(", re.M
)


def renderer_impl_paths() -> list[Path]:
    paths: list[Path] = []
    orchestrator = SIT / "situation_impl_renderer.h"
    if orchestrator.exists():
        paths.append(orchestrator)
    paths.extend(
        sorted(
            p
            for p in SIT.glob("situation_impl_renderer_*.h")
            if not p.name.endswith("_fwd.h")
        )
    )
    return paths


def static_names(path: Path) -> set[str]:
    text = path.read_text(encoding="utf-8", errors="replace")
    return set(STATIC_RE.findall(text))


def main() -> int:
    impl: set[str] = set()
    by_file: dict[str, set[str]] = {}
    for path in renderer_impl_paths():
        names = static_names(path)
        by_file[path.name] = names
        impl |= names

    fwd_renderer = static_names(RENDERER_FWD)
    other: set[str] = set()
    for p in OTHER_FWD:
        if p.exists():
            other |= static_names(p)

    missing = sorted(impl - fwd_renderer - other)
    extra = sorted(fwd_renderer - impl)

    if missing:
        print(f"FAIL: {len(missing)} static(s) in renderer slice(s) missing from renderer_fwd.h:")
        for n in missing:
            print(f"  + {n}")
    if extra:
        print(f"FAIL: {len(extra)} forward decl(s) in renderer_fwd.h with no renderer slice def:")
        for n in extra:
            print(f"  - {n}")

    if missing or extra:
        return 1

    detail = ", ".join(f"{name}({len(names)})" for name, names in sorted(by_file.items()))
    print(
        f"OK: renderer_fwd.h covers all {len(impl)} static functions "
        f"in renderer slices [{detail}]"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
