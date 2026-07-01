#!/usr/bin/env python3
"""Audit R2 resources extract integrity (#if balance, symbol coverage, boundaries)."""

from __future__ import annotations

import re
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SIT = ROOT / "sit"
MONO = SIT / "situation_impl_renderer.h"
RES = SIT / "situation_impl_renderer_resources.h"
CORE = SIT / "situation_impl_renderer_core.h"
SHADER = SIT / "situation_impl_renderer_shader.h"
FWD = SIT / "situation_impl_renderer_fwd.h"

STATIC_RE = re.compile(
    r"^\s*static\s+(?:inline\s+)?(?:const\s+)?(?:[\w\s\*_]+?)\s+(_\w+)\s*\(", re.M
)
STATIC_SLOT_GETTER = re.compile(
    r"^\s*static\s+(?:inline\s+)?(?:const\s+)?(?:[\w\s\*_]+?)\s+_SitGetTextureSlot\s*\(", re.M
)
CMD_BANNER = "// --- Command Buffer Implementations ---"
RES_BANNER = "// --- Resource Allocation Helpers ---"

R2_STATICS_EXPECTED = {
    "_SitGetTextureSlot",
    "_SitGetShaderSlot",
    "_SitGetMeshSlot",
    "_SitGetBufferSlot",
    "_SitGetComputePipelineSlot",
    "_SitGetModelSlot",
    "_SitAllocShaderSlot",
    "_SitFreeShaderSlot",
    "_SitAllocComputePipelineSlot",
    "_SitFreeComputePipelineSlot",
    "_SitAllocMeshSlot",
    "_SitFreeMeshSlot",
    "_SitAllocBufferSlot",
    "_SitFreeBufferSlot",
    "_SitAllocModelSlot",
    "_SitFreeModelSlot",
    "_SituationVulkanCreateAndUploadBuffer",
    "_SituationVulkanReadBackBuffer",
    "_SitGLUploadNamedBuffer",
    "_SitMeshLayoutExpectedStride",
    "_SitMeshStrideIsKnown",
    "_SitInferMeshLayoutFromStride",
    "_SituationCreateMeshInternal",
    "_SituationCleanupInternalDefaultResources",
    "_SituationCleanupDanglingResources",
    "_SituationTextureMipExtent",
    "_SituationTextureRectInBounds",
    "_SituationTextureBufferRowPitchBytes",
    "_SituationBufferRegionFits",
}


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

    for path in (MONO, CORE, SHADER, RES):
        if not path.exists():
            print(f"FAIL missing {path.name}")
            failures += 1
            continue
        unclosed, extra = pp_balance(path.read_text(encoding="utf-8"))
        if unclosed or extra:
            print(f"FAIL pp {path.name}: unclosed={len(unclosed)} extra_endif={len(extra)}")
            for item in unclosed[:5]:
                print(f"  unclosed {item[0]}: {item[1]}")
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
    res_text = RES.read_text(encoding="utf-8")

    if STATIC_SLOT_GETTER.search(mono_text):
        print("FAIL monolith still defines _SitGetTextureSlot")
        failures += 1
    else:
        print("OK monolith has no _SitGetTextureSlot definition")

    if not STATIC_SLOT_GETTER.search(res_text):
        print("FAIL resources.h missing _SitGetTextureSlot")
        failures += 1
    else:
        print("OK resources.h defines _SitGetTextureSlot")

    if RES_BANNER not in res_text:
        print("FAIL resources.h missing Resource Allocation Helpers banner")
        failures += 1
    else:
        print("OK resources.h contains Resource Allocation Helpers banner")

    if CMD_BANNER in res_text:
        print("FAIL resources.h contains Command Buffer banner (cut too far)")
        failures += 1
    else:
        print("OK resources.h stops before Command Buffer banner")

    if "SituationCreateBuffer(" in mono_text and "SITAPI SituationError SituationCreateBuffer" in mono_text:
        print("FAIL monolith still defines SituationCreateBuffer")
        failures += 1
    else:
        print("OK monolith has no SituationCreateBuffer body")

    if "SITAPI SituationError SituationCreateBuffer" not in res_text:
        print("FAIL resources.h missing SituationCreateBuffer")
        failures += 1
    else:
        print("OK resources.h defines SituationCreateBuffer")

    if "SITAPI SituationError SituationLoadTexture" in res_text:
        print("FAIL resources.h contains SituationLoadTexture (belongs in R5 monolith)")
        failures += 1
    else:
        print("OK resources.h excludes SituationLoadTexture")

    res_defs = set(STATIC_RE.findall(res_text))
    missing = sorted(R2_STATICS_EXPECTED - res_defs)
    extra = sorted(res_defs - R2_STATICS_EXPECTED)
    if missing:
        print(f"FAIL resources.h missing {len(missing)} expected static(s): {missing}")
        failures += 1
    else:
        print(f"OK resources.h has all {len(R2_STATICS_EXPECTED)} expected statics")
    if extra:
        print(f"WARN resources.h has unexpected static(s): {extra}")

    mono_defs = STATIC_RE.findall(mono_text)
    res_defs_list = STATIC_RE.findall(res_text)
    core_defs = STATIC_RE.findall(CORE.read_text(encoding="utf-8"))
    shader_defs = STATIC_RE.findall(SHADER.read_text(encoding="utf-8"))
    all_defs = mono_defs + res_defs_list + core_defs + shader_defs
    dups = {k: v for k, v in Counter(all_defs).items() if v > 1}
    cross_file = {k: v for k, v in dups.items()
                  if sum(1 for defs in (mono_defs, res_defs_list, core_defs, shader_defs) if k in defs) > 1}
    if cross_file:
        print(f"FAIL cross-file duplicate static defs: {len(cross_file)}")
        for name, count in sorted(cross_file.items())[:12]:
            print(f"  {name}: {count}")
        failures += 1
    elif dups:
        print(f"OK duplicate static defs are intra-file forward stubs only ({len(dups)})")
    else:
        print("OK no duplicate static defs across renderer files")

    fwd_names = set(STATIC_RE.findall(FWD.read_text(encoding="utf-8")))
    impl = set(all_defs)
    missing_fwd = sorted(impl - fwd_names)
    if missing_fwd:
        print(f"FAIL {len(missing_fwd)} static(s) missing from fwd.h: {missing_fwd[:8]}")
        failures += 1
    else:
        print(f"OK all {len(impl)} renderer statics declared in fwd.h")

    print(f"\nresources.h: {len(res_text.splitlines())} lines, {len(res_defs)} statics")
    print(f"monolith: {len(mono_text.splitlines())} lines, {len(mono_defs)} statics")

    return failures


if __name__ == "__main__":
    sys.exit(main())
