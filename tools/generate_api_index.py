#!/usr/bin/env python3
"""Generate Situation API markdown index and supplement (moved from scripts/).

Outputs:
  doc/situation_api_index.md
  doc/situation_api_generated.md

Usage:
  python tools/generate_api_index.py
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))

from situation_api_parser import (  # noqa: E402
    ROOT,
    ApiEntry,
    parse_api_header,
    read_version,
)

API_MD = ROOT / "doc" / "situation_api.md"
GUIDE_DIR = ROOT / "doc" / "guide"
CMD_REF = ROOT / "doc" / "situation_command_reference.md"


def read_api_doc_corpus() -> str:
    """Umbrella + doc/guide/*.md (P2.5 split)."""
    parts: list[str] = []
    if API_MD.exists():
        parts.append(API_MD.read_text(encoding="utf-8", errors="replace"))
    if GUIDE_DIR.is_dir():
        for path in sorted(GUIDE_DIR.glob("*.md")):
            parts.append(path.read_text(encoding="utf-8", errors="replace"))
    return "\n".join(parts)
OUT_GENERATED = ROOT / "doc" / "situation_api_generated.md"
OUT_INDEX = ROOT / "doc" / "situation_api_index.md"

CMD_REF_ANCHORS: dict[str, str] = {
    "SituationCmdBeginRenderPass": "#1-render-pass--framebuffer",
    "SituationCmdEndRenderPass": "#1-render-pass--framebuffer",
    "SituationCmdSetViewport": "#2-dynamic-viewport--scissor",
    "SituationCmdSetScissor": "#2-dynamic-viewport--scissor",
    "SituationCmdSetViewportIndexed": "#2-dynamic-viewport--scissor",
    "SituationCmdSetScissorIndexed": "#2-dynamic-viewport--scissor",
    "SituationCmdPushRasterState": "#3-raster-state-fixed-function",
    "SituationCmdPopRasterState": "#3-raster-state-fixed-function",
    "SituationCmdSetCullMode": "#3-raster-state-fixed-function",
    "SituationCmdSetFrontFace": "#3-raster-state-fixed-function",
    "SituationCmdSetDepthTest": "#3-raster-state-fixed-function",
    "SituationCmdSetDepthWrite": "#3-raster-state-fixed-function",
    "SituationCmdSetDepthBias": "#3-raster-state-fixed-function",
    "SituationCmdSetPolygonMode": "#3-raster-state-fixed-function",
    "SituationCmdSetLineWidth": "#3-raster-state-fixed-function",
    "SituationCmdSetPrimitiveTopology": "#3-raster-state-fixed-function",
    "SituationCmdSetBlendEnable": "#3-raster-state-fixed-function",
    "SituationCmdSetBlendFuncSeparate": "#3-raster-state-fixed-function",
    "SituationCmdSetColorWriteMask": "#3-raster-state-fixed-function",
    "SituationCmdSetStencilTest": "#3-raster-state-fixed-function",
    "SituationCmdSetMultisampleState": "#3-raster-state-fixed-function",
    "SituationCmdBindPipeline": "#4-graphics-pipeline--shader-data",
    "SituationCmdSetPushConstant": "#4-graphics-pipeline--shader-data",
    "SituationCmdSetPushConstantData": "#4-graphics-pipeline--shader-data",
    "SituationCmdBindDescriptorSet": "#4-graphics-pipeline--shader-data",
    "SituationCmdBindDescriptorSetDynamic": "#4-graphics-pipeline--shader-data",
    "SituationCmdBindTextureSet": "#4-graphics-pipeline--shader-data",
    "SituationCmdBindSampledTexture": "#4-graphics-pipeline--shader-data",
    "SituationCmdSetVertexAttribute": "#5-vertex-input--manual-draw-core-path",
    "SituationCmdBindVertexBuffer": "#5-vertex-input--manual-draw-core-path",
    "SituationCmdBindIndexBuffer": "#5-vertex-input--manual-draw-core-path",
    "SituationCmdBindIndexBufferEx": "#5-vertex-input--manual-draw-core-path",
    "SituationCmdDraw": "#5-vertex-input--manual-draw-core-path",
    "SituationCmdDrawIndexed": "#5-vertex-input--manual-draw-core-path",
    "SituationCmdDrawIndirect": "#5-vertex-input--manual-draw-core-path",
    "SituationCmdDrawIndexedIndirect": "#5-vertex-input--manual-draw-core-path",
    "SituationCmdDrawMesh": "#6-high-level-draw-helpers",
    "SituationCmdDrawQuad": "#6-high-level-draw-helpers",
    "SituationCmdDrawTexture": "#6-high-level-draw-helpers",
    "SituationCmdDrawTextureYpqGrade": "#6-high-level-draw-helpers",
    "SituationCmdDrawText": "#6-high-level-draw-helpers",
    "SituationCmdDrawTextEx": "#6-high-level-draw-helpers",
    "SituationCmdDrawTextBoxed": "#6-high-level-draw-helpers",
    "SituationCmdBindMeshPullBuffers": "#5-vertex-input--manual-draw-core-path",
    "SituationCmdBindComputePipeline": "#7-compute",
    "SituationCmdBindComputeTexture": "#7-compute",
    "SituationCmdDispatch": "#7-compute",
    "SituationCmdDispatchEx": "#7-compute",
    "SituationCmdDispatchIndirect": "#7-compute",
    "SituationCmdPipelineBarrier": "#7-compute",
    "SituationCmdPipelineBarrierEx": "#7-compute",
    "SituationCmdBufferBarrier": "#7-compute",
    "SituationCmdTextureBarrier": "#7-compute",
    "SituationCmdClear": "#8-transfer--presentation",
    "SituationCmdClearColor": "#8-transfer--presentation",
    "SituationCmdClearDepth": "#8-transfer--presentation",
    "SituationCmdClearDepthStencil": "#8-transfer--presentation",
    "SituationCmdClearStencil": "#8-transfer--presentation",
    "SituationCmdCopyBuffer": "#8-transfer--presentation",
    "SituationCmdCopyBufferEx": "#8-transfer--presentation",
    "SituationCmdCopyTexture": "#8-transfer--presentation",
    "SituationCmdBlitTexture": "#8-transfer--presentation",
    "SituationCmdCopyBufferToTexture": "#8-transfer--presentation",
    "SituationCmdCopyTextureToBuffer": "#8-transfer--presentation",
    "SituationCmdGPUZoneBegin": "#9-gpu-profiling-zones-p103",
    "SituationCmdGPUZoneEnd": "#9-gpu-profiling-zones-p103",
    "SituationCmdResetQueryPool": "#10-user-query-pools-p104",
    "SituationCmdWriteTimestamp": "#10-user-query-pools-p104",
    "SituationCmdBeginOcclusionQuery": "#10-user-query-pools-p104",
    "SituationCmdEndOcclusionQuery": "#10-user-query-pools-p104",
    "SituationCmdPresent": "#8-transfer--presentation",
    "SituationRendererBehaviorPolicyDefault": "#3-raster-state-fixed-function",
    "SituationCmdSetRendererBehavior": "#3-raster-state-fixed-function",
    "SituationCmdPushRendererBehavior": "#3-raster-state-fixed-function",
    "SituationCmdPopRendererBehavior": "#3-raster-state-fixed-function",
    "SituationCmdBeginDebugGroup": "#11-debug-markers",
    "SituationCmdEndDebugGroup": "#11-debug-markers",
    "SituationCmdBeginRenderToDisplay": "#11-deprecated-commands",
    "SituationCmdEndRender": "#11-deprecated-commands",
    "SituationCmdBindUniformBuffer": "#11-deprecated-commands",
    "SituationCmdBindTexture": "#11-deprecated-commands",
    "SituationCmdBindComputeBuffer": "#11-deprecated-commands",
}
DEPRECATED_NON_CMD = {"SituationMemoryBarrier": "#11-deprecated-commands"}


def render_entry(e: ApiEntry) -> str:
    body = e.comment or "_See `sit/situation_api.h` for the authoritative declaration._"
    return (
        f"#### `{e.name}`\n"
        f"{body}\n"
        f"```c\n"
        f"{e.signature}\n"
        f"```\n"
    )


def render_generated(entries: list[ApiEntry], missing: list[ApiEntry], version: str) -> str:
    by_section: dict[str, list[ApiEntry]] = {}
    for e in missing:
        by_section.setdefault(e.section, []).append(e)

    parts = [
        "# Situation API — Generated Supplement",
        "",
        f"_Auto-generated from `sit/situation_api.h` — Situation **{version}**._",
        "",
        "Regenerate:",
        "",
        "```bat",
        "python tools\\generate_api_index.py",
        "```",
        "",
        f"**Coverage:** {len(entries) - len(missing)}/{len(entries)} symbols in situation_api.md; "
        f"**{len(missing)}** below.",
        "",
        "---",
        "",
    ]

    for section in sorted(by_section.keys(), key=str.lower):
        parts.append(f"## {section}")
        parts.append("")
        for e in sorted(by_section[section], key=lambda x: x.name):
            parts.append(render_entry(e))
            parts.append("---")
            parts.append("")

    return "\n".join(parts).rstrip() + "\n"


def cmd_entries(entries: list[ApiEntry]) -> list[ApiEntry]:
    return sorted([e for e in entries if e.name.startswith("SituationCmd")], key=lambda x: x.name)


def cmd_doc_link(name: str) -> str:
    anchor = CMD_REF_ANCHORS.get(name) or DEPRECATED_NON_CMD.get(name)
    if not anchor:
        return "—"
    return f"[command ref](situation_command_reference.md{anchor})"


def parse_command_ref_index() -> set[str]:
    if not CMD_REF.exists():
        return set()
    text = CMD_REF.read_text(encoding="utf-8", errors="replace")
    return set(re.findall(r"`(SituationCmd\w+)`", text))


def sync_command_reference_version(version: str) -> bool:
    if not CMD_REF.exists():
        return False
    text = CMD_REF.read_text(encoding="utf-8", errors="replace")
    ver_short = version.split(" ", 1)[0]
    new_line = f"_Authoritative catalog of every `SituationCmd*` recording function — Situation **{ver_short}**._"
    updated, n = re.subn(
        r"^_Authoritative catalog of every `SituationCmd\*` recording function — Situation \*\*[^*]+\*\*\._\n",
        new_line + "\n",
        text,
        count=1,
    )
    if n:
        CMD_REF.write_text(updated, encoding="utf-8")
    return bool(n)


def verify_command_catalog(entries: list[ApiEntry]) -> tuple[list[str], list[str], list[str]]:
    header_cmds = {e.name for e in entries if e.name.startswith("SituationCmd")}
    ref_cmds = {n for n in parse_command_ref_index() if n.startswith("SituationCmd")}
    missing_in_ref = sorted(header_cmds - ref_cmds)
    extra_in_ref = sorted(ref_cmds - header_cmds)
    unmapped = sorted(n for n in header_cmds if n not in CMD_REF_ANCHORS)
    return missing_in_ref, extra_in_ref, unmapped


def render_command_index_table(cmds: list[ApiEntry]) -> list[str]:
    parts = [
        "## Command buffer (`SituationCmd*`)",
        "",
        "Canonical narrative: **[situation_command_reference.md](situation_command_reference.md)**.",
        "",
        "| Function | Doc | Summary |",
        "|----------|-----|---------|",
    ]
    for e in cmds:
        summary = (e.comment or "").replace("|", "\\|")
        if len(summary) > 100:
            summary = summary[:97] + "..."
        parts.append(f"| `{e.name}` | {cmd_doc_link(e.name)} | {summary or '—'} |")
    parts.append("")
    return parts


def render_index(entries: list[ApiEntry], version: str) -> str:
    by_section: dict[str, list[ApiEntry]] = {}
    for e in entries:
        by_section.setdefault(e.section, []).append(e)

    cmds = cmd_entries(entries)

    parts = [
        "# Situation Public API Index",
        "",
        f"_Auto-generated from `sit/situation_api.h` — Situation **{version}**._",
        "",
        "Regenerate: `python tools/generate_api_index.py`",
        "",
        "Bindings: `python tools/generate_odin_bindings.py` → [bindings/odin/](../bindings/odin/)",
        "",
        "- **[situation_sdk.md](situation_sdk.md)** — SDK manual",
        "- **[situation_api.md](situation_api.md)** — detailed reference",
        "- **[situation_command_reference.md](situation_command_reference.md)** — `SituationCmd*`",
        "- **[situation_api_generated.md](situation_api_generated.md)** — gaps",
        "",
        f"**Total public functions:** {len(entries)} (`SituationCmd*`: {len(cmds)})",
        "",
        "---",
        "",
    ]
    parts.extend(render_command_index_table(cmds))
    parts.append("---")
    parts.append("")

    for section in sorted(by_section.keys(), key=str.lower):
        parts.append(f"## {section}")
        parts.append("")
        if any(e.name.startswith("SituationCmd") for e in by_section[section]):
            parts.append("| Function | Doc | Summary |")
            parts.append("|----------|-----|---------|")
            for e in sorted(by_section[section], key=lambda x: x.name):
                summary = (e.comment or "").replace("|", "\\|")
                if len(summary) > 120:
                    summary = summary[:117] + "..."
                doc = cmd_doc_link(e.name) if e.name.startswith("SituationCmd") else "—"
                parts.append(f"| `{e.name}` | {doc} | {summary or '—'} |")
        else:
            parts.append("| Function | Summary |")
            parts.append("|----------|---------|")
            for e in sorted(by_section[section], key=lambda x: x.name):
                summary = (e.comment or "").replace("|", "\\|")
                if len(summary) > 120:
                    summary = summary[:117] + "..."
                parts.append(f"| `{e.name}` | {summary or '—'} |")
        parts.append("")

    parts.append("## Alphabetical (all)")
    parts.append("")
    for e in sorted(entries, key=lambda x: x.name):
        parts.append(f"- `{e.name}` — {e.section}")
    parts.append("")

    return "\n".join(parts)


def main() -> None:
    version = read_version()
    entries = parse_api_header()
    api_md = read_api_doc_corpus()

    missing = [e for e in entries if e.name not in api_md]
    present = len(entries) - len(missing)

    sync_command_reference_version(version)
    missing_in_ref, extra_in_ref, unmapped = verify_command_catalog(entries)

    OUT_GENERATED.write_text(render_generated(entries, missing, version), encoding="utf-8")
    OUT_INDEX.write_text(render_index(entries, version), encoding="utf-8")

    print(f"Situation {version}")
    print(f"Parsed {len(entries)} SITAPI functions")
    label = "api docs (umbrella+guide)" if GUIDE_DIR.is_dir() else "situation_api.md"
    print(f"{label}: {present}/{len(entries)} ({len(missing)} missing)")
    if missing_in_ref:
        print(f"WARN: {len(missing_in_ref)} SituationCmd* missing from command reference")
    if unmapped:
        print(f"WARN: {len(unmapped)} command(s) lack CMD_REF_ANCHORS")
    print(f"Wrote {OUT_GENERATED.relative_to(ROOT)}")
    print(f"Wrote {OUT_INDEX.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
