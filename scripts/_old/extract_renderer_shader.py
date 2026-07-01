#!/usr/bin/env python3
"""Mechanical extract: situation_impl_renderer_shader.h (R1).

Removes disjoint shader regions from situation_impl_renderer.h and appends them to
situation_impl_renderer_shader.h. Re-run only on a clean monolith (not after R1).
"""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MONO = ROOT / "sit" / "situation_impl_renderer.h"
SHADER = ROOT / "sit" / "situation_impl_renderer_shader.h"

# 1-based inclusive line ranges (re-grep anchors before changing).
SHADER_RANGES: list[tuple[int, int]] = [
    (450, 493),  # _SituationReadSpirvFile
    (3351, 3826),  # GL shader helpers (lc #if block; before bindless)
    (3969, 6251),  # Core internal GPU shader loading → SituationLoadShaderFromSpirv (VK)
    (6976, 7167),  # _SituationCreateVulkanShaderModule / _SituationCreateVulkanPipeline
    (11014, 11522),  # shaderc compile + _SituationFreeSpirvBlob
    (22521, 23648),  # GL compile + compute pipeline create/destroy
    (25294, 25555),  # _SituationVulkanCreateShaderModule* + CreateGraphicsPipeline*
    (26944, 28595),  # SituationLoadShader* … SituationReloadShader
]

SHADER_HEADER = """\
/***************************************************************************************************
*
*   situation_impl_renderer_shader.h - Shader Load, Compile, and Pipeline Creation
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   Shader file loading, GLSL/SPIR-V compile, async workers, pipeline/cache paths.
*
*   Do not include directly — included only from situation_impl_renderer.h.
*
***************************************************************************************************/
#ifndef SITUATION_IMPL_RENDERER_SHADER_H
#define SITUATION_IMPL_RENDERER_SHADER_H

"""

SHADER_FOOTER = """
#endif // SITUATION_IMPL_RENDERER_SHADER_H
"""

INCLUDE_LINE = '#include "situation_impl_renderer_shader.h"\n'


def main() -> None:
    lines = MONO.read_text(encoding="utf-8").splitlines(keepends=True)

    chunks: list[str] = []
    for start, end in SHADER_RANGES:
        if start < 1 or end > len(lines) or start > end:
            raise SystemExit(f"Invalid range {start}-{end} (file has {len(lines)} lines)")
        chunks.append("".join(lines[start - 1 : end]))

    shader_body = "\n".join(chunks)
    SHADER.write_text(SHADER_HEADER + shader_body + SHADER_FOOTER, encoding="utf-8")

    for start, end in sorted(SHADER_RANGES, reverse=True):
        del lines[start - 1 : end]

    # Insert shader include after core include (or after guard if core missing).
    insert_at = None
    for i, line in enumerate(lines):
        if 'situation_impl_renderer_core.h' in line:
            insert_at = i + 1
            break
    if insert_at is None:
        for i, line in enumerate(lines):
            if line.strip() == "#define SITUATION_IMPL_RENDERER_H":
                insert_at = i + 1
                break
    if insert_at is None:
        raise SystemExit("Could not find insert point for shader include")

    if not any(INCLUDE_LINE.strip() in ln for ln in lines):
        lines.insert(insert_at, "\n" + INCLUDE_LINE)

    MONO.write_text("".join(lines), encoding="utf-8")

    print(f"shader: {len((SHADER_HEADER + shader_body + SHADER_FOOTER).splitlines())} lines -> {SHADER}")
    print(f"monolith: {len(lines)} lines -> {MONO}")
    print(f"ranges extracted: {len(SHADER_RANGES)}")


if __name__ == "__main__":
    main()
