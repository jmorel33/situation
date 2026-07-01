#!/usr/bin/env python3
"""Mechanical extract: situation_impl_renderer_lc.h (R4).

Multi-range cut from situation_impl_renderer.h:
  A — render thread init/destroy (#if !__STDC_NO_THREADS__ at file head)
  B — lifecycle banner through line before SituationAcquireFrameCommandBuffer
  C — hot-reload tail through render-thread entry (#endif before shader include)

Re-run only on a pre-R4 monolith (not after R4 landed).
"""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MONO = ROOT / "sit" / "situation_impl_renderer.h"
LC = ROOT / "sit" / "situation_impl_renderer_lc.h"

LC_HEADER = """\
/***************************************************************************************************
*
*   situation_impl_renderer_lc.h - Renderer Lifecycle (Init, Backends, Thread, Hot-Reload)
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   Renderer init/shutdown, OpenGL/Vulkan bootstrap, soft command-buffer execute,
*   internal 2D renderers, render thread, hot-reload pass (GL + VK inline).
*
*   Do not include directly — included only from situation_impl_renderer.h.
*
***************************************************************************************************/
#ifndef SITUATION_IMPL_RENDERER_LC_H
#define SITUATION_IMPL_RENDERER_LC_H

"""

LC_FOOTER = """
#endif // SITUATION_IMPL_RENDERER_LC_H
"""

INCLUDE_LINE = '#include "situation_impl_renderer_lc.h"\n'
CORE_INCLUDE = 'situation_impl_renderer_core.h'
ACQUIRE_RE = re.compile(r"^SITAPI\s+SituationError\s+SituationAcquireFrameCommandBuffer\s*\(")
HOT_RELOAD_RE = re.compile(
    r"^\s*static\s+SituationError\s+_SituationPerformHotReloadPass\s*\("
)
LIFECYCLE_BANNER = "// --- Core Lifecycle Implementation ---"
RENDER_THREAD_IF = "#if !defined(__STDC_NO_THREADS__)"
SHADER_INCLUDE = '#include "situation_impl_renderer_shader.h"'


def walk_back_doc(lines: list[str], idx: int) -> int:
    """Walk back from idx to include preceding /** doc block."""
    start = idx
    while start > 0:
        prev = lines[start - 1].strip()
        if prev.startswith("*") or prev in ("*/",) or prev.startswith("//"):
            start -= 1
            continue
        if prev == "/**":
            start -= 1
            continue
        break
    return start


def find_cut_bounds(lines: list[str]) -> list[tuple[int, int]]:
    """Return 0-based [start, end) slices for ranges A, B, C."""
    # Range A: first render-thread #if block after core include
    core_idx = next(i for i, line in enumerate(lines) if CORE_INCLUDE in line)
    a_start = next(
        i
        for i in range(core_idx + 1, len(lines))
        if lines[i].strip() == RENDER_THREAD_IF
    )
    a_end = a_start + 1
    depth = 0
    while a_end < len(lines):
        s = lines[a_end].strip()
        if re.match(r"#if(n?def|\s)", s):
            depth += 1
        elif s.startswith("#endif"):
            if depth == 0:
                a_end += 1
                break
            depth -= 1
        a_end += 1
    else:
        raise SystemExit("Range A: unclosed #if for render-thread head block")

    # Range B: lifecycle banner through line before AcquireFrameCommandBuffer
    b_start = next(i for i, line in enumerate(lines) if LIFECYCLE_BANNER in line)
    # Include separator comment lines immediately above banner if present
    while b_start > 0 and lines[b_start - 1].strip().startswith("//"):
        b_start -= 1
    acquire_idx = next(i for i, line in enumerate(lines) if ACQUIRE_RE.match(line))
    b_end = acquire_idx
    while b_end > b_start and not lines[b_end - 1].strip():
        b_end -= 1

    # Range C: hot-reload doc + tail through last #endif before shader include
    hot_idx = next(i for i, line in enumerate(lines) if HOT_RELOAD_RE.match(line))
    c_start = walk_back_doc(lines, hot_idx)
    shader_inc = next(i for i, line in enumerate(lines) if SHADER_INCLUDE in line)
    c_end = shader_inc
    while c_end > c_start and not lines[c_end - 1].strip():
        c_end -= 1

    ranges = [(a_start, a_end), (b_start, b_end), (c_start, c_end)]
    for label, (lo, hi) in zip(("A", "B", "C"), ranges):
        if lo >= hi:
            raise SystemExit(f"Range {label}: invalid {lo + 1}-{hi} (empty)")
    return ranges


def main() -> None:
    lines = MONO.read_text(encoding="utf-8").splitlines(keepends=True)
    ranges = find_cut_bounds([l.rstrip("\n\r") for l in lines])

    chunks: list[str] = []
    for lo, hi in ranges:
        chunks.append("".join(lines[lo:hi]))
    lc_body = "\n".join(chunks)
    LC.write_text(LC_HEADER + lc_body + LC_FOOTER, encoding="utf-8")

    for lo, hi in sorted(ranges, reverse=True):
        del lines[lo:hi]

    insert_at = next(i for i, line in enumerate(lines) if CORE_INCLUDE in line) + 1
    if not any(INCLUDE_LINE.strip() in ln for ln in lines):
        lines.insert(insert_at, "\n" + INCLUDE_LINE)

    MONO.write_text("".join(lines), encoding="utf-8")

    labels = ("A render-thread head", "B lifecycle", "C hot-reload tail")
    for label, (lo, hi) in zip(labels, ranges):
        print(f"cut {label}: lines {lo + 1}-{hi} ({hi - lo} lines)")
    print(f"lc: {len((LC_HEADER + lc_body + LC_FOOTER).splitlines())} lines -> {LC}")
    print(f"monolith: {len(lines)} lines -> {MONO}")


if __name__ == "__main__":
    main()
