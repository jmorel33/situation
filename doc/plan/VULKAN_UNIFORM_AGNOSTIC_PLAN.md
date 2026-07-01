# Plan: Backend-Agnostic Language Wrapper Examples

## Status: COMPLETE

---

## Problem

The `hello_situation` examples (Odin, Zig, Rust) were written with OpenGL idioms
and failed on the Vulkan build:

1. `gl_VertexID` in the vertex shader — not valid in Vulkan GLSL (must be `gl_VertexIndex`)
2. Bare `layout(location=N) uniform float/vec2` in the fragment shader — forbidden
   by Vulkan GLSL spec
3. `SituationSetShaderUniform` calls — OpenGL-only, no-op on Vulkan

## Solution

Single source file, two shader string pairs, runtime backend selection via
`SituationGetGraphicsBackend()`. No ifdefs, no drama.

- **Vertex shader**: `VERT_SRC_VK` uses `gl_VertexIndex`; `VERT_SRC_GL` uses `gl_VertexID`.
- **Fragment shader**: `FRAG_SRC_VK` uses `layout(push_constant)` block;
  `FRAG_SRC_GL` uses bare `layout(location=N) uniform`.
  The shader body (all the actual rendering logic) is shared / identical in both.
- **CPU upload**: Vulkan path uses `SituationCmdSetPushConstant` with a 16-byte
  packed struct `{float time, float _pad, float res_x, float res_y}`;
  OpenGL path uses `SituationSetShaderUniform` as before.
- Backend detected once before the shader load:
  `is_vulkan = SituationGetGraphicsBackend() == SIT_GRAPHICS_BACKEND_VULKAN`

## Files changed

- `wrappers/Odin/examples/hello_situation/hello.odin`
- `wrappers/Zig/examples/hello_situation/main.zig`
- `wrappers/Rust/examples/hello_situation.rs`

No library files touched. No API changes.

## Also fixed in this session

- **Binding generator parser bug**: `SituationPlayToneEx` (multi-line SITAPI
  declaration) was silently dropped. Fixed in `tools/situation_api_parser.py` —
  the parser now joins continuation lines before matching. Bindings regenerated:
  554 → 555 bound functions across Odin, Zig, Rust.
