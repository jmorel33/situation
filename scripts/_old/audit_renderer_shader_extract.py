#!/usr/bin/env python3
"""Audit R1 shader extract integrity (comments, #if balance, symbol coverage)."""

from __future__ import annotations

import re
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SIT = ROOT / "sit"
MONO = SIT / "situation_impl_renderer.h"
SHADER = SIT / "situation_impl_renderer_shader.h"
CORE = SIT / "situation_impl_renderer_core.h"
FWD = SIT / "situation_impl_renderer_fwd.h"

SHADER_RANGES: list[tuple[int, int]] = [
    (450, 493),
    (3351, 3826),
    (3969, 6251),
    (6976, 7167),
    (11014, 11522),
    (22521, 23648),
    (25294, 25555),
    (26944, 28595),
]

STATIC_RE = re.compile(
    r"^\s*static\s+(?:inline\s+)?(?:const\s+)?(?:[\w\s\*_]+?)\s+(_\w+)\s*\(", re.M
)


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


def combined_monolith_with_shader() -> str:
    mono = MONO.read_text(encoding="utf-8")
    shader = SHADER.read_text(encoding="utf-8")
    needle = '#include "situation_impl_renderer_shader.h"'
    if needle not in mono:
        raise SystemExit("monolith missing shader include")
    return mono.replace(needle, "\n/* --- inlined shader.h --- */\n" + shader)


def main() -> int:
    failures = 0

    for path in (MONO, CORE, SHADER):
        unclosed, extra = pp_balance(path.read_text(encoding="utf-8"))
        if unclosed or extra:
            print(f"FAIL pp {path.name}: unclosed={len(unclosed)} extra_endif={len(extra)}")
            for item in unclosed[:5]:
                print(f"  unclosed {item[0]}: {item[1]}")
            for ln in extra[:5]:
                print(f"  extra #endif at {ln}")
            failures += 1
        else:
            print(f"OK pp {path.name}")

    combined = combined_monolith_with_shader()
    unclosed, extra = pp_balance(combined)
    if unclosed or extra:
        print(f"FAIL pp combined (monolith + inlined shader): unclosed={len(unclosed)} extra={len(extra)}")
        failures += 1
    else:
        print("OK pp combined (monolith + inlined shader)")

    mono_defs = STATIC_RE.findall(MONO.read_text(encoding="utf-8"))
    shader_defs = STATIC_RE.findall(SHADER.read_text(encoding="utf-8"))
    core_defs = STATIC_RE.findall(CORE.read_text(encoding="utf-8"))
    all_defs = mono_defs + shader_defs + core_defs
    dups = {k: v for k, v in Counter(all_defs).items() if v > 1}
    cross_slice = {k: v for k, v in dups.items() if sum(1 for p, defs in [
        ("mono", mono_defs), ("shader", shader_defs), ("core", core_defs)
    ] if k in defs) > 1}
    if cross_slice:
        print(f"WARN cross-slice duplicate static defs: {len(cross_slice)}")
        for name, count in sorted(cross_slice.items())[:12]:
            locs = []
            for label, defs in [("mono", mono_defs), ("shader", shader_defs), ("core", core_defs)]:
                if name in defs:
                    locs.append(f"{label}({defs.count(name)})")
            print(f"  {name}: {count} total — {', '.join(locs)}")
    else:
        print("OK no cross-slice duplicate static defs")

    expected_lines = sum(e - s + 1 for s, e in SHADER_RANGES)
    shader_body_lines = len(SHADER.read_text(encoding="utf-8").splitlines()) - 14  # header ~13 + footer
    delta = shader_body_lines - expected_lines - (len(SHADER_RANGES) - 1)  # join newlines
    print(f"shader body lines ~{shader_body_lines} (extract sum {expected_lines}, delta {delta})")

    # fwd coverage (reuse verify logic)
    fwd_names = set(STATIC_RE.findall(FWD.read_text(encoding="utf-8")))
    impl = set(all_defs)
    missing = sorted(impl - fwd_names)
    if missing:
        print(f"FAIL {len(missing)} static(s) missing from fwd.h")
        failures += 1
    else:
        print(f"OK all {len(impl)} renderer statics declared in fwd.h")

    # Known post-extract intentional deltas
    print("\nIntentional post-extract edits (not extract script output):")
    print("  - SIT_GL_ASYNC_STAGE_* macros live in monolith (~618), not shader.h (include order)")
    print("  - Orphan _SituationVulkanCreateShaderModule doc removed from monolith (~20609)")
    print("  - Orphan #if SITUATION_ENABLE_SHADER_COMPILER removed from monolith (~8016)")
    print("  - #if SITUATION_USE_VULKAN at monolith ~20539 closed early (~20609); was swallowing lc APIs")
    print("  - Manual #if guards added around extracted VK chunks during bring-up")

    return failures


if __name__ == "__main__":
    sys.exit(main())
