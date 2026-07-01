#!/usr/bin/env python3
"""One-shot mechanical extract: situation_impl_renderer_core.h (R3)."""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MONO = ROOT / "sit" / "situation_impl_renderer.h"
CORE = ROOT / "sit" / "situation_impl_renderer_core.h"

CORE_HEADER = """\
/***************************************************************************************************
*
*   situation_impl_renderer_core.h - Shared Renderer Infrastructure
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   Cross-cutting renderer utilities: uniform map, staging, graveyard, GL state
*   shadow, ring buffers, program cache, GL error helpers.
*
*   Do not include directly — included only from situation_impl_renderer.h.
*
***************************************************************************************************/
#ifndef SITUATION_IMPL_RENDERER_CORE_H
#define SITUATION_IMPL_RENDERER_CORE_H

"""

CORE_FOOTER = """
#endif // SITUATION_IMPL_RENDERER_CORE_H
"""


def main() -> None:
    lines = MONO.read_text(encoding="utf-8").splitlines(keepends=True)
    cut = next(
        i for i, line in enumerate(lines) if line.strip() == "#if !defined(__STDC_NO_THREADS__)"
    )

    core_body = "".join(lines[21:cut])
    CORE.write_text(CORE_HEADER + core_body + CORE_FOOTER, encoding="utf-8")

    new_mono = (
        "".join(lines[:21])
        + '#include "situation_impl_renderer_core.h"\n\n'
        + "".join(lines[cut:])
    )
    MONO.write_text(new_mono, encoding="utf-8")

    print(f"core: {len((CORE_HEADER + core_body + CORE_FOOTER).splitlines())} lines -> {CORE}")
    print(f"monolith: {len(lines)} -> {len(new_mono.splitlines())} lines -> {MONO}")


if __name__ == "__main__":
    main()
