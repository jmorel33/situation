#!/usr/bin/env python3
"""Collapse legacy SituationCmd* sections in doc/situation_api.md to link table."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
API_MD = ROOT / "doc" / "situation_api.md"

LINK_TABLE = '''### `SituationCmd*` — command buffer recording

**Canonical reference:** **[situation_command_reference.md](situation_command_reference.md)** (Situation 2.4.125+).

All GPU recording functions use `SituationGetMainCommandBuffer()` between `SituationAcquireFrameCommandBuffer()` and `SituationEndFrame()`. Signatures, OpenGL/Vulkan matrix, ordering, and use cases are maintained in the command reference only.

| Section | Commands |
|---------|----------|
| [§1 Render pass](situation_command_reference.md#1-render-pass--framebuffer) | `BeginRenderPass`, `EndRenderPass` |
| [§2 Viewport / scissor](situation_command_reference.md#2-dynamic-viewport--scissor) | `SetViewport`, `SetScissor` |
| [§3 Raster state](situation_command_reference.md#3-raster-state-fixed-function) | `PushRasterState`, `PopRasterState`, `SetCullMode`, `SetDepthTest`, `SetDepthWrite`, `SetBlendEnable`, `SetBlendFuncSeparate` |
| [§4 Pipeline & descriptors](situation_command_reference.md#4-graphics-pipeline--shader-data) | `BindPipeline`, `SetPushConstant`, `SetPushConstantData`, `BindDescriptorSet`, `BindDescriptorSetDynamic`, `BindTextureSet`, `BindSampledTexture` |
| [§5 Vertex input & draw](situation_command_reference.md#5-vertex-input--manual-draw-core-path) | `SetVertexAttribute` (GL), `BindVertexBuffer`, `BindIndexBuffer`, `Draw`, `DrawIndexed` |
| [§6 High-level draw](situation_command_reference.md#6-high-level-draw-helpers) | `DrawMesh`, `DrawQuad`, `DrawTexture`, `DrawText`, `DrawTextEx` |
| [§7 Compute](situation_command_reference.md#7-compute) | `BindComputePipeline`, `BindComputeTexture`, `Dispatch`, `PipelineBarrier` |
| [§8 Transfer](situation_command_reference.md#8-transfer--presentation) | `CopyBuffer`, `Present` |
| [§9 Debug](situation_command_reference.md#9-debug-markers) | `BeginDebugGroup`, `EndDebugGroup` |
| [§10 Recipe](situation_command_reference.md#10-recommended-command-order-one-3d-object-core-path) | Full frame ordering example |
| [§11 Deprecated](situation_command_reference.md#11-deprecated-commands) | `BeginRenderToDisplay`, `EndRender`, `BindUniformBuffer`, `BindTexture`, `BindComputeBuffer`, `MemoryBarrier` |

**Related non-command APIs in this file:** `SituationCreateMesh`, `SituationLoadShader`, `SituationUpdateBuffer`, `SituationSetShaderUniform*`, virtual displays, compute pipeline creation, readback buffers.

---
'''

COMPUTE_POINTER = '''### Compute command recording

See **[situation_command_reference.md §7](situation_command_reference.md#7-compute)** for `SituationCmdBindComputePipeline`, `BindComputeTexture`, `Dispatch`, and `PipelineBarrier`.

---
'''

DEPRECATED_POINTER = '''### Deprecated `SituationCmd*` (migration only)

See **[situation_command_reference.md §11](situation_command_reference.md#11-deprecated-commands)**. Prefer `SituationCmdBeginRenderPass` / `EndRenderPass`, `BindDescriptorSet`, `BindTextureSet`, `SituationCmdPipelineBarrier` over legacy names.

---
'''

RASTER_POINTER = '''### Dynamic raster & debug commands

See **[situation_command_reference.md §3](situation_command_reference.md#3-raster-state-fixed-function)** and **[§9](situation_command_reference.md#9-debug-markers)**.

---
'''

COPYBUFFER_POINTER = '''#### `SituationCmdCopyBuffer`

Documented in **[situation_command_reference.md §8](situation_command_reference.md#8-transfer--presentation)**.

---
'''


def find_line(lines: list[str], needle: str, start: int = 0) -> int:
    for i in range(start, len(lines)):
        if needle in lines[i]:
            return i
    raise ValueError(f"not found: {needle!r}")


def main() -> None:
    text = API_MD.read_text(encoding="utf-8")
    lines = text.splitlines(keepends=True)

    # 1) Main graphics block: legacy header through BindDescriptorSetDynamic stub
    i0 = find_line(lines, "### `SituationCmd*` functions (canonical vs legacy stubs)")
    i1 = find_line(lines, "#### Resource Management")
    lines[i0:i1] = [LINK_TABLE]

    # 2) Remove phantom SituationCmdCopyTexture (not in API header)
    try:
        c0 = find_line(lines, "#### `SituationCmdCopyTexture`")
        c1 = find_line(lines, "#### `SituationGetTextureFormat`")
        lines[c0:c1] = []
    except ValueError:
        pass

    # 3) Compute cmd stubs (keep DestroyComputePipeline before, Virtual Displays after)
    try:
        k0 = find_line(lines, "#### `SituationCmdBindComputePipeline`")
        k1 = find_line(lines, "#### Virtual Displays")
        lines[k0:k1] = [COMPUTE_POINTER]
    except ValueError:
        pass

    # 4) Scattered duplicate block: deprecated BeginRenderToDisplay through duplicate EndRenderPass
    try:
        d0 = find_line(lines, "#### `SituationCmdBeginRenderToDisplay`")
        d1 = find_line(lines, "#### `SituationLoadShaderFromMemory`")
        lines[d0:d1] = [DEPRECATED_POINTER]
    except ValueError:
        pass

    # 5) Deprecated bind stubs + raster + debug + SetPushConstantData (keep Camera section)
    try:
        e0 = find_line(lines, "#### `SituationCmdBindUniformBuffer`")
        e1 = find_line(lines, "### Camera & projection helpers")
        lines[e0:e1] = [DEPRECATED_POINTER, RASTER_POINTER]
    except ValueError:
        pass

    # 6) Short CopyBuffer stub before readback section content
    try:
        b0 = find_line(lines, "#### `SituationCmdCopyBuffer`")
        b1 = find_line(lines, "#### `SituationReadBuffer`")
        lines[b0:b1] = [COPYBUFFER_POINTER]
    except ValueError:
        pass

    API_MD.write_text("".join(lines), encoding="utf-8")
    print(f"Updated {API_MD.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
