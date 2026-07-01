#!/usr/bin/env python3
"""Mechanical extract: situation_impl_renderer_frame_cmd.h (R5).

Single contiguous cut from SituationAcquireFrameCommandBuffer through the stub body,
stopping immediately before ``#include "situation_impl_renderer_shader.h"``.

Re-run only on a pre-R5 monolith (not after R5 landed).
"""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MONO = ROOT / "sit" / "situation_impl_renderer.h"
FCMD = ROOT / "sit" / "situation_impl_renderer_frame_cmd.h"

FCMD_HEADER = """\
/***************************************************************************************************
*
*   situation_impl_renderer_frame_cmd.h - Frame Loop, Commands, and Model I/O
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   Per-frame acquire/end, SituationCmd* recording, render lists, metrics, model loaders.
*
*   Do not include directly — included only from situation_impl_renderer.h.
*
***************************************************************************************************/
#ifndef SITUATION_IMPL_RENDERER_FRAME_CMD_H
#define SITUATION_IMPL_RENDERER_FRAME_CMD_H

"""

FCMD_FOOTER = """
#endif // SITUATION_IMPL_RENDERER_FRAME_CMD_H
"""

ACQUIRE_RE = re.compile(r"^SITAPI\s+SituationError\s+SituationAcquireFrameCommandBuffer\s*\(")
SHADER_INCLUDE = '#include "situation_impl_renderer_shader.h"'
RESOURCES_INCLUDE = '#include "situation_impl_renderer_resources.h"'
LC_INCLUDE = 'situation_impl_renderer_lc.h'
CORE_INCLUDE = 'situation_impl_renderer_core.h'
FCMD_INCLUDE = '#include "situation_impl_renderer_frame_cmd.h"\n'

ORDERED_INCLUDES = [
    '#include "situation_impl_renderer_core.h"\n',
    '#include "situation_impl_renderer_lc.h"\n',
    '#include "situation_impl_renderer_shader.h"\n',
    '#include "situation_impl_renderer_resources.h"\n',
    FCMD_INCLUDE,
]


def find_cut_bounds(lines: list[str]) -> tuple[int, int]:
    """Return 0-based [start, end) for the frame_cmd body."""
    start = next(i for i, line in enumerate(lines) if ACQUIRE_RE.match(line))
    end = next(i for i, line in enumerate(lines) if SHADER_INCLUDE in line)
    while end > start and not lines[end - 1].strip():
        end -= 1
    return start, end


def main() -> None:
    lines = MONO.read_text(encoding="utf-8").splitlines(keepends=True)
    stripped = [l.rstrip("\n\r") for l in lines]
    start, end = find_cut_bounds(stripped)

    if start >= end:
        raise SystemExit(f"Invalid cut: start={start + 1} end={end + 1}")

    body = "".join(lines[start:end])
    FCMD.write_text(FCMD_HEADER + body + FCMD_FOOTER, encoding="utf-8")

    guard_end = next(i for i, line in enumerate(lines) if line.strip() == "#define SITUATION_IMPL_RENDERER_H") + 1

    new_mono = "".join(lines[:guard_end + 1]) + "\n"
    for inc in ORDERED_INCLUDES:
        new_mono += inc
    new_mono += "\n#endif // SITUATION_IMPL_RENDERER_H\n"

    MONO.write_text(new_mono, encoding="utf-8")

    fcmd_lines = len((FCMD_HEADER + body + FCMD_FOOTER).splitlines())
    print(f"cut: lines {start + 1}-{end} ({end - start} lines)")
    print(f"frame_cmd: {fcmd_lines} lines -> {FCMD}")
    print(f"monolith: {len(new_mono.splitlines())} lines -> {MONO}")


if __name__ == "__main__":
    main()
