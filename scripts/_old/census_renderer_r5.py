#!/usr/bin/env python3
"""Pre-R5 census of the monolith stub (situation_impl_renderer.h body before slice includes)."""

from __future__ import annotations

import re
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MONO = ROOT / "sit" / "situation_impl_renderer.h"
LC = ROOT / "sit" / "situation_impl_renderer_lc.h"
RES = ROOT / "sit" / "situation_impl_renderer_resources.h"
SHADER = ROOT / "sit" / "situation_impl_renderer_shader.h"

STATIC_RE = re.compile(
    r"^(\s*)static\s+(?:inline\s+)?(?:const\s+)?(?:[\w\s\*_]+?)\s+(_\w+)\s*([\(<])"
)
SITAPI_RE = re.compile(r"^SITAPI\s+(.+?)\s+(Situation\w+)\s*\(")
BANNER_RE = re.compile(r"^// --- (.+?) ---")


def pp_balance(text: str) -> tuple[list[tuple[int, str]], list[int]]:
    stack: list[tuple[int, str]] = []
    extra: list[int] = []
    for i, line in enumerate(text.splitlines(), 1):
        s = line.strip()
        if re.match(r"#if(n?def|\s)", s):
            stack.append((i, s[:80]))
        elif re.match(r"#endif", s):
            if stack:
                stack.pop()
            else:
                extra.append(i)
    return stack, extra


def classify_static(line: str, tail: str) -> str:
    if tail == "(" and line.rstrip().endswith(";"):
        return "fwd"
    return "def"


def main() -> int:
    lines = MONO.read_text(encoding="utf-8").splitlines()
    shader_inc = next(
        i for i, line in enumerate(lines, 1) if 'situation_impl_renderer_shader.h' in line
    )
    body_start = None
    for i, line in enumerate(lines, 1):
        if line.startswith("SITAPI ") or line.startswith("static "):
            body_start = i
            break
    if body_start is None:
        print("R5 stub body: (empty — R5 already landed; orchestrator is includes-only)")
        print(f"File total lines: {len(lines)}")
        return 0
    stub_lines = lines[body_start - 1 : shader_inc - 1]
    stub_text = "\n".join(stub_lines)

    statics: list[tuple[int, str, str]] = []
    for i, line in enumerate(stub_lines, body_start):
        m = STATIC_RE.match(line)
        if m:
            statics.append((i, m.group(2), classify_static(line, m.group(3))))

    sitapis = [
        (i, m.group(2))
        for i, line in enumerate(stub_lines, body_start)
        for m in [SITAPI_RE.match(line)]
        if m
    ]

    banners = [
        (i, m.group(1))
        for i, line in enumerate(stub_lines, body_start)
        for m in [BANNER_RE.match(line.strip())]
        if m
    ]

    unclosed, extra = pp_balance(stub_text)

    print("=" * 60)
    print("R5 MONOLITH STUB CENSUS")
    print("=" * 60)
    print(f"File total lines:           {len(lines)}")
    print(f"Orchestrator head:          L1-L{body_start - 1} ({body_start - 1} lines)")
    print(f"R5 stub body:               L{body_start}-L{shader_inc - 1} ({shader_inc - body_start} lines)")
    print(f"Tail includes:              L{shader_inc}-L{len(lines)}")
    print()
    defs = [s for s in statics if s[2] == "def"]
    fwds = [s for s in statics if s[2] == "fwd"]
    print(f"Static symbols:             {len(statics)}")
    print(f"  definitions:              {len(defs)}")
    print(f"  forward stubs:            {len(fwds)}")
    print(f"SITAPI functions:           {len(sitapis)}")
    print(f"Section banners:            {len(banners)}")
    print()

    if unclosed or extra:
        print(f"PP BALANCE: FAIL unclosed={len(unclosed)} extra_endif={len(extra)}")
        for item in unclosed[:5]:
            print(f"  unclosed L{item[0]}: {item[1]}")
    else:
        print("PP BALANCE in stub:         OK")

    # Cross-check: symbols that plan says belong elsewhere
    print()
    print("-" * 60)
    print("DOMAIN BLUR CHECK (should NOT be in R5 stub)")
    print("-" * 60)
    blur_checks = [
        ("_SituationInitRenderer", "lc", LC),
        ("_SituationPerformHotReloadPass", "lc", LC),
        ("SituationCreateBuffer", "resources", RES),
        ("SituationLoadShader", "shader", SHADER),
        ("_SitGetTextureSlot", "resources", RES),
    ]
    for sym, owner, path in blur_checks:
        in_stub = any(n == sym for _, n, k in statics if k == "def") or any(
            n == sym for _, n in sitapis
        )
        in_owner = sym in path.read_text(encoding="utf-8")
        status = "OK absent" if not in_stub else "SURPRISE present"
        print(f"  {sym:40} owner={owner:10} stub={status}")

    # Forward stubs that pair with lc (already extracted)
    print()
    print("-" * 60)
    print("FORWARD STUBS IN STUB (pair with defs in same file or lc)")
    print("-" * 60)
    lc_text = LC.read_text(encoding="utf-8")
    for ln, name, kind in fwds:
        lc_has_def = bool(
            re.search(rf"^\s*static\s+.*\s+{re.escape(name)}\s*\([^{{]", lc_text, re.M)
            and not re.search(rf"^\s*static\s+.*\s+{re.escape(name)}\s*\([^)]*\)\s*;", lc_text, re.M)
        )
        stub_has_def = name in {n for _, n, k in defs}
        note = ""
        if lc_has_def and not stub_has_def:
            note = " -> pairs with lc.h (fwd-only in stub; OK for single TU)"
        elif stub_has_def:
            note = " -> pairs with def in stub"
        print(f"  L{ln:5d}  {name}{note}")

    # Regions
    print()
    print("-" * 60)
    print("REGIONS (by banner)")
    print("-" * 60)
    regions: list[tuple[str, int, int]] = [("(pre-banner)", body_start, banners[0][0] - 1 if banners else shader_inc - 1)]
    for j, (ln, name) in enumerate(banners):
        end = banners[j + 1][0] - 1 if j + 1 < len(banners) else shader_inc - 1
        regions.append((name, ln, end))
    for name, lo, hi in regions:
        nd = sum(1 for l, n, k in defs if lo <= l <= hi)
        ns = sum(1 for l, n in sitapis if lo <= l <= hi)
        print(f"  {name}")
        print(f"    L{lo}-L{hi}  ({hi - lo + 1} lines)  statics={nd}  SITAPI={ns}")

    # SITAPI inventory by prefix
    print()
    print("-" * 60)
    print("SITAPI BY CATEGORY")
    print("-" * 60)
    cats = Counter()
    for _, name in sitapis:
        if "Acquire" in name or "EndFrame" in name or "CommandBuffer" in name:
            cats["frame loop"] += 1
        elif name.startswith("SituationCmd"):
            cats["SituationCmd*"] += 1
        elif "LoadModel" in name or "DrawModel" in name or "ReloadModel" in name:
            cats["model I/O"] += 1
        elif "LoadTexture" in name or "ReloadTexture" in name or "ReloadShader" in name or "ReloadCompute" in name:
            cats["reload APIs (tail?)"] += 1
        elif "GetDraw" in name or "GetRender" in name or "GetFrame" in name or "GetMax" in name:
            cats["metrics/stats"] += 1
        elif "GetBuffer" in name or "GetTexture" in name or "GetMesh" in name:
            cats["handle/query"] += 1
        else:
            cats["other"] += 1
    for cat, n in sorted(cats.items(), key=lambda x: -x[1]):
        print(f"  {cat:25} {n}")

    # Tail surprise: reload APIs at end of stub
    print()
    print("-" * 60)
    print("TAIL SURPRISE CHECK (last 200 lines before shader include)")
    print("-" * 60)
    tail_start = max(body_start, shader_inc - 200)
    tail_sitapi = [(l, n) for l, n in sitapis if l >= tail_start]
    tail_static = [(l, n) for l, n, k in statics if l >= tail_start and k == "def"]
    print(f"  SITAPI in tail: {len(tail_sitapi)}")
    for l, n in tail_sitapi:
        print(f"    L{l:5d}  {n}")
    print(f"  static defs in tail: {len(tail_static)}")
    for l, n in tail_static:
        print(f"    L{l:5d}  {n}")

    # Cut proposal
    print()
    print("-" * 60)
    print("PROPOSED R5 CUT")
    print("-" * 60)
    print(f"  START: L{body_start}  SituationAcquireFrameCommandBuffer")
    print(f"  STOP:  L{shader_inc - 1}  (line before shader include)")
    print(f"  LINES: {shader_inc - body_start}")
    print(f"  STATICS (defs): {len(defs)}")
    print(f"  SITAPI: {len(sitapis)}")
    print()
    print("  POST-R5 orchestrator target:")
    print("    guard + core + lc + shader + resources + frame_cmd includes (~25-40 lines)")

    # Full static list
    print()
    print("-" * 60)
    print(f"ALL {len(defs)} STATIC DEFINITIONS")
    print("-" * 60)
    for ln, name, _ in defs:
        print(f"  L{ln:5d}  {name}")

    return 0 if not (unclosed or extra) else 1


if __name__ == "__main__":
    sys.exit(main())
