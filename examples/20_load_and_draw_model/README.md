# 20 — Load and Draw Model

**Tier 5 — Capstone** | OpenGL + Vulkan | Refactored from `examples/other/loading_and_rendering_a_3d_model.c`

Loads the **Utah teapot** from the test harness assets, centers it, and draws with a simple directional-lit shader. Orbit camera with mouse; auto-rotate toggle.

## Asset

Run from the **repo root** so this path resolves:

```
tests/harness/assets/utah_teapot.obj
```

Fallback: `tests/harness/assets/teapot.stl`

## Build & run

```bat
build\build_situation.bat static-opengl
build\build_examples.bat static-opengl 20_load_and_draw_model
build\examples\20_load_and_draw_model.exe
```

Short name: `load_and_draw_model`

## Keys

| Key | Action |
|-----|--------|
| LMB drag | Orbit camera |
| Mouse wheel | Zoom |
| `R` | Reset camera |
| `SPACE` | Toggle auto-rotate |
| Universal | `ESC`, `F11`, `F9`, `P`, `F12` |

## APIs demonstrated

| API | Role |
|-----|------|
| `SituationLoadModelFromOBJ` / `SituationLoadModelFromSTL` | Mesh import |
| `SituationLoadShaderFromMemory` | Runtime GLSL/SPIR-V |
| `SituationCameraDesc` + `SituationCameraBuildViewProj` | 3D camera |
| `SituationCmdDrawMesh` | Per-submesh draw |
| `SituationGetMeshData` | AABB for framing |

## vs raylib

Same arc as raylib `models_*` examples — load geometry, set up camera, draw in a loop — using Situation's mesh + camera helpers.
