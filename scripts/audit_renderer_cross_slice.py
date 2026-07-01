#!/usr/bin/env python3
"""Audit cross-slice symbol references in renderer modularization.

Reports static/SITAPI symbols defined in one slice file and referenced from another.
Coupling is expected in the single-TU model; this script makes it visible for docs/PRs.

Usage:
  python scripts/audit_renderer_cross_slice.py
  python scripts/audit_renderer_cross_slice.py --json
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SIT = ROOT / "sit"

SLICES: dict[str, Path] = {
    "core": SIT / "situation_impl_renderer_core.h",
    "shader": SIT / "situation_impl_renderer_shader.h",
    "resources": SIT / "situation_impl_renderer_resources.h",
    "monolith": SIT / "situation_impl_renderer.h",
}

STATIC_RE = re.compile(
    r"^\s*static\s+(?:inline\s+)?(?:const\s+)?(?:[\w\s\*_]+?)\s+(_\w+)\s*\(", re.M
)
SITAPI_RE = re.compile(
    r"^SITAPI\s+(?:[\w\s\*]+\s+)?((?:Situation|_Situation)\w*)\s*\(", re.M
)

# Known mechanical-cut domain blur (see doc/done/RENDERER_MODULARIZATION_PLAN.md)
KNOWN_BLUR = {
    ("resources", "SituationCmdBindTexture"): "frame_cmd API in resources slice (R2 cut)",
    ("resources", "SituationCmdCopyBuffer"): "frame_cmd API in resources slice (R2 cut)",
    ("resources", "SituationCmdCopyBufferEx"): "frame_cmd API in resources slice (R2 cut)",
    ("resources", "SituationCmdBlitTexture"): "frame_cmd API in resources slice (R2 cut)",
    ("resources", "SituationCmdCopyTexture"): "frame_cmd API in resources slice (R2 cut)",
    ("resources", "SituationCmdCopyBufferToTexture"): "frame_cmd API in resources slice (R2 cut)",
    ("resources", "SituationCmdCopyTextureToBuffer"): "frame_cmd API in resources slice (R2 cut)",
    ("resources", "_SituationCleanupDanglingResources"): "lc lifecycle in resources slice (R2 cut)",
    ("resources", "_SituationCleanupInternalDefaultResources"): "lc lifecycle in resources slice (R2 cut)",
}


def symbols_in(path: Path) -> set[str]:
    text = path.read_text(encoding="utf-8")
    return set(STATIC_RE.findall(text)) | set(SITAPI_RE.findall(text))


def references(text: str, symbol: str) -> bool:
    return bool(re.search(r"\b" + re.escape(symbol) + r"\s*\(", text))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--json", action="store_true", help="Emit machine-readable report")
    args = parser.parse_args()

    missing = [name for name, path in SLICES.items() if not path.exists()]
    if missing:
        print("FAIL missing slice files:", ", ".join(missing))
        return 1

    defs = {name: symbols_in(path) for name, path in SLICES.items()}
    texts = {name: path.read_text(encoding="utf-8") for name, path in SLICES.items()}

    edges: dict[str, dict[str, list[str]]] = {}
    for caller, ctext in texts.items():
        for callee, syms in defs.items():
            if caller == callee:
                continue
            hits = sorted(sym for sym in syms if references(ctext, sym))
            if hits:
                edges.setdefault(caller, {})[callee] = hits

    if args.json:
        print(json.dumps({"edges": edges, "known_blur": list(KNOWN_BLUR.values())}, indent=2))
        return 0

    print("Cross-slice coupling report (symbol call references)\n")
    for caller in ("monolith", "core", "shader", "resources"):
        if caller not in edges:
            continue
        print(f"=== {caller} ->")
        for callee, hits in sorted(edges[caller].items()):
            print(f"  {callee} ({len(hits)} symbols)")
            for sym in hits[:12]:
                note = ""
                key = (callee, sym)
                if key in KNOWN_BLUR:
                    note = f"  [known blur: {KNOWN_BLUR[key]}]"
                elif caller == "monolith" and callee in ("core", "shader", "resources"):
                    note = "  [expected until R4/R5]"
                print(f"    {sym}{note}")
            if len(hits) > 12:
                print(f"    ... +{len(hits) - 12} more")
        print()

    # Cross-slice includes are forbidden
    failures = 0
    for name, path in SLICES.items():
        if name == "monolith":
            continue
        text = path.read_text(encoding="utf-8")
        for other in SLICES:
            if other == name:
                continue
            needle = f'situation_impl_renderer_{other}.h'
            if other == "monolith":
                needle = "situation_impl_renderer.h"
            if f'#include "{needle}"' in text or f"#include '{needle}'" in text:
                print(f"FAIL cross-slice #include in {path.name}: {needle}")
                failures += 1
    if failures == 0:
        print("OK no cross-slice #include lines in slice headers")

    return failures


if __name__ == "__main__":
    sys.exit(main())
