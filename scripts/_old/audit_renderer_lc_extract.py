#!/usr/bin/env python3
"""Audit R4 lifecycle extract integrity (#if balance, boundaries, static parity)."""

from __future__ import annotations

import re
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SIT = ROOT / "sit"
MONO = SIT / "situation_impl_renderer.h"
LC = SIT / "situation_impl_renderer_lc.h"
CORE = SIT / "situation_impl_renderer_core.h"
SHADER = SIT / "situation_impl_renderer_shader.h"
RES = SIT / "situation_impl_renderer_resources.h"
FWD = SIT / "situation_impl_renderer_fwd.h"

STATIC_RE = re.compile(
    r"^\s*static\s+(?:inline\s+)?(?:const\s+)?(?:[\w\s\*_]+?)\s+(_\w+)\s*\(", re.M
)
ACQUIRE_RE = re.compile(
    r"^SITAPI\s+SituationError\s+SituationAcquireFrameCommandBuffer\s*\(", re.M
)
INIT_RENDERER_RE = re.compile(
    r"^\s*static\s+SituationError\s+_SituationInitRenderer\s*\(", re.M
)
HOT_RELOAD_RE = re.compile(
    r"^\s*static\s+SituationError\s+_SituationPerformHotReloadPass\s*\(", re.M
)
LIFECYCLE_BANNER = "// --- Core Lifecycle Implementation ---"


def pp_balance(text: str) -> tuple[list[tuple[int, str]], list[int]]:
    stack: list[tuple[int, str]] = []
    extra_endif: list[int] = []
    for i, line in enumerate(text.splitlines(), 1):
        s = line.strip()
        if re.match(r"#if(n?def|\s)", s):
            stack.append((i, s[:90]))
        elif re.match(r"#endif", s):
            if stack:
                stack.pop()
            else:
                extra_endif.append(i)
    return stack, extra_endif


def combined_monolith_with_slices() -> str:
    mono = MONO.read_text(encoding="utf-8")
    for path, needle in (
        (LC, '#include "situation_impl_renderer_lc.h"'),
        (SHADER, '#include "situation_impl_renderer_shader.h"'),
        (RES, '#include "situation_impl_renderer_resources.h"'),
    ):
        body = path.read_text(encoding="utf-8")
        tag = path.stem
        if needle not in mono:
            raise SystemExit(f"monolith missing include for {path.name}")
        mono = mono.replace(
            needle, f"\n/* --- inlined {tag} --- */\n{body}\n/* --- end {tag} --- */\n"
        )
    return mono


def main() -> int:
    failures = 0

    for path in (MONO, CORE, LC, SHADER, RES):
        if not path.exists():
            print(f"FAIL missing {path.name}")
            failures += 1
            continue
        unclosed, extra = pp_balance(path.read_text(encoding="utf-8"))
        if unclosed or extra:
            print(f"FAIL pp {path.name}: unclosed={len(unclosed)} extra_endif={len(extra)}")
            failures += 1
        else:
            print(f"OK pp {path.name}")

    combined = combined_monolith_with_slices()
    unclosed, extra = pp_balance(combined)
    if unclosed or extra:
        print(f"FAIL pp combined: unclosed={len(unclosed)} extra_endif={len(extra)}")
        failures += 1
    else:
        print("OK pp combined (monolith + inlined slices)")

    mono_text = MONO.read_text(encoding="utf-8")
    lc_text = LC.read_text(encoding="utf-8")

    if INIT_RENDERER_RE.search(mono_text):
        print("FAIL monolith still defines _SituationInitRenderer")
        failures += 1
    else:
        print("OK monolith has no _SituationInitRenderer definition")

    if not INIT_RENDERER_RE.search(lc_text):
        print("FAIL lc.h missing _SituationInitRenderer")
        failures += 1
    else:
        print("OK lc.h defines _SituationInitRenderer")

    if LIFECYCLE_BANNER in mono_text:
        print("FAIL monolith still contains Core Lifecycle banner")
        failures += 1
    else:
        print("OK monolith has no Core Lifecycle banner")

    if LIFECYCLE_BANNER not in lc_text:
        print("FAIL lc.h missing Core Lifecycle banner")
        failures += 1
    else:
        print("OK lc.h contains Core Lifecycle banner")

    if HOT_RELOAD_RE.search(mono_text):
        print("FAIL monolith still defines _SituationPerformHotReloadPass")
        failures += 1
    else:
        print("OK monolith has no _SituationPerformHotReloadPass definition")

    if not HOT_RELOAD_RE.search(lc_text):
        print("FAIL lc.h missing _SituationPerformHotReloadPass")
        failures += 1
    else:
        print("OK lc.h defines _SituationPerformHotReloadPass")

    if not ACQUIRE_RE.search(mono_text):
        print("FAIL monolith missing SituationAcquireFrameCommandBuffer (R5 stub)")
        failures += 1
    else:
        print("OK monolith retains SituationAcquireFrameCommandBuffer (R5)")

    if ACQUIRE_RE.search(lc_text):
        print("FAIL lc.h contains SituationAcquireFrameCommandBuffer (cut too far)")
        failures += 1
    else:
        print("OK lc.h stops before SituationAcquireFrameCommandBuffer")

    mono_defs = STATIC_RE.findall(mono_text)
    lc_defs = STATIC_RE.findall(lc_text)
    core_defs = STATIC_RE.findall(CORE.read_text(encoding="utf-8"))
    shader_defs = STATIC_RE.findall(SHADER.read_text(encoding="utf-8"))
    res_defs = STATIC_RE.findall(RES.read_text(encoding="utf-8"))
    all_defs = mono_defs + lc_defs + core_defs + shader_defs + res_defs
    dups = {k: v for k, v in Counter(all_defs).items() if v > 1}
    cross_file = {
        k: v
        for k, v in dups.items()
        if sum(1 for defs in (mono_defs, lc_defs, core_defs, shader_defs, res_defs) if k in defs)
        > 1
    }
    if cross_file:
        print(f"FAIL cross-file duplicate static defs: {len(cross_file)}")
        for name in sorted(cross_file)[:12]:
            print(f"  {name}: {cross_file[name]}")
        failures += 1
    else:
        print("OK no cross-file duplicate static defs")

    fwd_names = set(STATIC_RE.findall(FWD.read_text(encoding="utf-8")))
    impl = set(all_defs)
    missing_fwd = sorted(impl - fwd_names)
    if missing_fwd:
        print(f"FAIL {len(missing_fwd)} static(s) missing from fwd.h: {missing_fwd[:8]}")
        failures += 1
    else:
        print(f"OK all {len(impl)} renderer statics declared in fwd.h")

    print(f"\nlc.h: {len(lc_text.splitlines())} lines, {len(lc_defs)} statics")
    print(f"monolith: {len(mono_text.splitlines())} lines, {len(mono_defs)} statics")

    return failures


if __name__ == "__main__":
    sys.exit(main())
