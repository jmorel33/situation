# Shader debug tool (SPIR-V + GLSL, OpenGL + Vulkan)

Offline analysis for Situation shaders. Run after `glslc` compile — **before** a long GPU link or pipeline build.

SPIR-V bytecode is **not portable** between backends. `compile_demon_hunt_shaders.bat` emits **OpenGL-target** and **Vulkan-target** Demon Hunt `.spv`; the harness builds **both** targets under `tests/harness/spirv_out/`.

## Quick start

**Demon Hunt (OpenGL world shader):**

```bat
compile_demon_hunt_shaders.bat
python scripts\spirv_shader_debug.py demon_hunt --devel
```

**Harness (OpenGL + Vulkan regression shaders):**

```bat
compile_harness_shaders.bat
python scripts\spirv_shader_debug.py harness
python scripts\spirv_desc_spike.py
```

`compile_demon_hunt_shaders.bat` runs `demon_hunt --devel` automatically.

## Commands

| Command | Purpose |
|---------|---------|
| `python scripts\spirv_shader_debug.py demon_hunt --devel` | GLSL stats + OpenGL production/devel SPIR-V |
| `python scripts\spirv_shader_debug.py harness` | Harness SPIR-V for **OpenGL and Vulkan** targets |
| `python scripts\spirv_shader_debug.py report path.spv` | SPIR-V module report (any blob) |
| `python scripts\spirv_shader_debug.py glsl path.fs` | GLSL function line counts + recovery `#define`s |
| `python scripts\spirv_shader_debug.py compare before.spv after.spv` | Bisect: size/instruction delta per function |

## Devel map (`demon_hunt --devel`)

Production embed uses `glslc -O` with `--target-env=opengl`, which inlines the fragment shader into **one** SPIR-V function (~77k instructions). The `--devel` pass compiles to `build/examples/demon_hunt_sky.fs.devel.spv` **without** `-O` for **per-GLSL-function** SPIR-V instruction counts (`cast_prim`, `shade_sprite_opaque`, `map_dda_occluded`, …).

## What you get (SPIR-V)

- Blob size, SPIR-V version, ID bound
- Total instruction count
- Per-function instruction count (`--devel` or unoptimized compile)
- Resource bindings (set + binding)
- Warnings for large blobs / instruction counts

## What you get (GLSL)

- Recovery toggles (`DH_ENABLE_*`, `ENABLE_*`)
- Largest functions by source lines

## Backend notes

| Issue | OpenGL (Demon Hunt) | Vulkan (harness / apps) |
|-------|---------------------|-------------------------|
| Typical failure | Link `-641`, NVIDIA `too many instructions` in program log | `vkCreateShaderModule` or pipeline `-754`..`-756` / `-633` |
| Runtime detail | `SituationGetLastErrorMsg` — full `glGetProgramInfoLog` (v2.4.101+) | `SituationGetLastErrorMsg` — VkResult + stage in module create |
| Test | `sit_test.exe --filter spirv` | `sit_test_vulkan.exe --filter spirv` |
| Compile target | `compile_demon_hunt_shaders.bat` | `compile_harness_shaders.bat` |

## Bisect workflow (OpenGL link failure)

When the driver reports `-641` / `too many instructions`:

1. `python scripts\spirv_shader_debug.py demon_hunt > build\shader_baseline.txt`
2. Flip a toggle in `examples/demon_hunt_sky.fs` (see `doc/plan/DEMON_HUNT_SHADER_STRUCTURAL_REFACTOR.md`).
3. `compile_demon_hunt_shaders.bat`
4. `python scripts\spirv_shader_debug.py compare build\shader_before.spv examples\demon_hunt_sky.fs.spv`
5. GPU confirm: `build\sit_test.exe --module graphics --filter demon_hunt_sky_spirv_begin_poll`

For **Vulkan** descriptor regressions after harness GLSL edits, run `python scripts\spirv_desc_spike.py` and `sit_test_vulkan.exe --filter spirv`.

## Related

- `doc/TEST_SPIRV_SHADER_API.md` — dual-backend API test checklist
- `doc/plan/VULKAN_SPIRV_USER_DESCRIPTOR_PARITY.md` — Vulkan layout profiles + descriptor bind
- `scripts/spirv_desc_spike.py` — harness Vulkan `(set, binding)` gate
- `doc/plan/DEMON_HUNT_SHADER_STRUCTURAL_REFACTOR.md` — toggle bisect checklist
