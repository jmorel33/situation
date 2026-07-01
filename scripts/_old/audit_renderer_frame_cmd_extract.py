#!/usr/bin/env python3
"""Audit R5 frame_cmd extract integrity."""

from __future__ import annotations

import re
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SIT = ROOT / "sit"
MONO = SIT / "situation_impl_renderer.h"
FCMD = SIT / "situation_impl_renderer_frame_cmd.h"
CORE = SIT / "situation_impl_renderer_core.h"
LC = SIT / "situation_impl_renderer_lc.h"
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


def is_forward_stub(line: str) -> bool:
    return line.rstrip().endswith(";")


def main() -> int:
    failures = 0

    for path in (MONO, CORE, LC, SHADER, RES, FCMD):
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

    mono_text = MONO.read_text(encoding="utf-8")
    fcmd_text = FCMD.read_text(encoding="utf-8")

    mono_static_lines = [
        line for line in mono_text.splitlines() if STATIC_RE.match(line) and not is_forward_stub(line)
    ]
    if mono_static_lines:
        print(f"FAIL monolith still has {len(mono_static_lines)} static definition(s)")
        failures += 1
    else:
        print("OK monolith has no static definitions")

    if ACQUIRE_RE.search(mono_text):
        print("FAIL monolith still defines SituationAcquireFrameCommandBuffer")
        failures += 1
    else:
        print("OK monolith has no SituationAcquireFrameCommandBuffer body")

    if not ACQUIRE_RE.search(fcmd_text):
        print("FAIL frame_cmd.h missing SituationAcquireFrameCommandBuffer")
        failures += 1
    else:
        print("OK frame_cmd.h defines SituationAcquireFrameCommandBuffer")

    if INIT_RENDERER_RE.search(fcmd_text):
        print("FAIL frame_cmd.h contains _SituationInitRenderer (lc leak)")
        failures += 1
    else:
        print("OK frame_cmd.h excludes _SituationInitRenderer")

    expected_includes = [
        "situation_impl_renderer_core.h",
        "situation_impl_renderer_lc.h",
        "situation_impl_renderer_shader.h",
        "situation_impl_renderer_resources.h",
        "situation_impl_renderer_frame_cmd.h",
    ]
    for inc in expected_includes:
        if inc not in mono_text:
            print(f"FAIL monolith missing include {inc}")
            failures += 1
    if not failures:
        print("OK monolith include chain complete")

    mono_defs: list[str] = []
    fcmd_defs = STATIC_RE.findall(fcmd_text)
    core_defs = STATIC_RE.findall(CORE.read_text(encoding="utf-8"))
    lc_defs = STATIC_RE.findall(LC.read_text(encoding="utf-8"))
    shader_defs = STATIC_RE.findall(SHADER.read_text(encoding="utf-8"))
    res_defs = STATIC_RE.findall(RES.read_text(encoding="utf-8"))
    all_defs = mono_defs + fcmd_defs + core_defs + lc_defs + shader_defs + res_defs

    dups = {k: v for k, v in Counter(all_defs).items() if v > 1}
    cross_file = {
        k: v
        for k, v in dups.items()
        if sum(
            1 for defs in (mono_defs, fcmd_defs, core_defs, lc_defs, shader_defs, res_defs) if k in defs
        )
        > 1
    }
    if cross_file:
        print(f"FAIL cross-file duplicate static defs: {len(cross_file)}")
        for name in sorted(cross_file)[:8]:
            print(f"  {name}")
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

    print(f"\nframe_cmd.h: {len(fcmd_text.splitlines())} lines, {len(fcmd_defs)} statics")
    print(f"monolith: {len(mono_text.splitlines())} lines")

    return failures


if __name__ == "__main__":
    sys.exit(main())
