#!/usr/bin/env python3
"""Mechanical extract: situation_impl_renderer_resources.h (R2).

Single contiguous cut from the slot-getter region through the Resource Allocation
Helpers block, stopping immediately before ``// --- Command Buffer Implementations ---``.

Re-run only on a pre-R2 monolith (not after R2 landed).
"""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MONO = ROOT / "sit" / "situation_impl_renderer.h"
RES = ROOT / "sit" / "situation_impl_renderer_resources.h"

RES_HEADER = """\
/***************************************************************************************************
*
*   situation_impl_renderer_resources.h - Buffer, Texture, and Mesh Resource Management
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   Resource allocation, upload, readback, slot registry helpers (GL + VK inline).
*
*   Do not include directly — included only from situation_impl_renderer.h.
*
***************************************************************************************************/
#ifndef SITUATION_IMPL_RENDERER_RESOURCES_H
#define SITUATION_IMPL_RENDERER_RESOURCES_H

"""

RES_FOOTER = """
#endif // SITUATION_IMPL_RENDERER_RESOURCES_H
"""

INCLUDE_LINE = '#include "situation_impl_renderer_resources.h"\n'
SHADER_INCLUDE = '#include "situation_impl_renderer_shader.h"'

STATIC_SLOT_GETTER = re.compile(
    r"^\s*static\s+_SituationTextureSlot\*\s+_SitGetTextureSlot\s*\("
)
CMD_BANNER = "// --- Command Buffer Implementations ---"


def find_cut_bounds(lines: list[str]) -> tuple[int, int]:
    """Return 0-based [start, end) slice bounds for the resources block."""
    slot_idx = next(
        i for i, line in enumerate(lines) if STATIC_SLOT_GETTER.match(line)
    )
    end_idx = next(i for i, line in enumerate(lines) if CMD_BANNER in line)

    start_idx = slot_idx
    while start_idx > 0:
        prev = lines[start_idx - 1].strip()
        if prev.startswith("*") or prev in ("*/",) or prev.startswith("//"):
            start_idx -= 1
            continue
        if prev == "/**":
            start_idx -= 1
            continue
        break

    return start_idx, end_idx


def main() -> None:
    lines = MONO.read_text(encoding="utf-8").splitlines(keepends=True)
    start_idx, end_idx = find_cut_bounds([l.rstrip("\n\r") for l in lines])

    if start_idx >= end_idx:
        raise SystemExit(f"Invalid cut: start={start_idx + 1} end={end_idx + 1}")

    body = "".join(lines[start_idx:end_idx])
    RES.write_text(RES_HEADER + body + RES_FOOTER, encoding="utf-8")

    del lines[start_idx:end_idx]

    insert_at = None
    for i, line in enumerate(lines):
        if SHADER_INCLUDE in line:
            insert_at = i + 1
            break
    if insert_at is None:
        raise SystemExit("Could not find shader include for resources include insertion")

    if not any(INCLUDE_LINE.strip() in ln for ln in lines):
        lines.insert(insert_at, "\n" + INCLUDE_LINE)

    MONO.write_text("".join(lines), encoding="utf-8")

    res_lines = len((RES_HEADER + body + RES_FOOTER).splitlines())
    print(f"cut: lines {start_idx + 1}-{end_idx} ({end_idx - start_idx} lines)")
    print(f"resources: {res_lines} lines -> {RES}")
    print(f"monolith: {len(lines)} lines -> {MONO}")


if __name__ == "__main__":
    main()
