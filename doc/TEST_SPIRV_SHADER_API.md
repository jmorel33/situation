# Testing the SPIR-V shader load API (Situation v2.4.97+)

Checklist for **`SituationBeginLoadShaderFromSpirvMemory`**, **`SituationPollShaderLoad`**, **`SituationGetLastErrorCode`**, and **`SituationErrorToString`** on **both OpenGL and Vulkan**. The API is backend-neutral; failure codes and driver detail differ per backend.

## What you are proving

| Step | API | Pass criterion |
|------|-----|----------------|
| Kickoff | `SituationBeginLoadShaderFromSpirvMemory` | Returns `SITUATION_SUCCESS`, `shader.generation != 0` |
| Progress | `SituationPollShaderLoad` each frame | Returns `SITUATION_ERROR_SHADER_LOAD_IN_PROGRESS` (-553) until done |
| Done | `SituationPollShaderLoad` | Returns `SITUATION_SUCCESS`, pipeline usable |
| Failure | Any step | `SituationGetLastErrorCode` + `SituationGetLastErrorMsg` name the SPIR-V stage |

**Not** the same as `SituationLoadShaderFromSpirvMemory` (blocking, one shot).

## Backend matrix

| Topic | OpenGL | Vulkan |
|-------|--------|--------|
| SPIR-V target | `glslc --target-env=opengl` | `glslc --target-env=vulkan` |
| Async poll | `SituationPollShaderLoad` (VS/FS specialize + program link) | `SituationPollShaderLoad` (deferred `vkCreateGraphicsPipelines`) |
| Harness tests | `build\sit_test.exe` | `build\sit_test_vulkan.exe` |
| Demon Hunt (demo) | Embedded OpenGL-target `.spv` | Embedded Vulkan-target `.spv` + `SituationBeginLoadShaderFromSpirvMemoryEx` (UBO_SSBO); library regression: `demon_hunt_sky_spirv_vk_begin_poll` |
| Link / pipeline errors | -636..-641 (`OPENGL_SPIRV_*`) | -753..-756 (`VULKAN_SPIRV_*`), -633 pipeline |
| Driver detail | `glGetProgramInfoLog` / `glGetShaderInfoLog` (v2.4.101+) | `vkCreateShaderModule` VkResult + stage label in `SituationGetLastErrorMsg` |

## 1. Rebuild (required)

### OpenGL (Demon Hunt + GL harness)

```bat
compile_demon_hunt_shaders.bat
build_situation.bat opengl
build_tests.bat opengl
build_examples.bat opengl demon_hunt
```

### Vulkan (VK harness)

```bat
compile_harness_shaders.bat
build_situation.bat vulkan
build_tests.bat vulkan
python scripts\spirv_desc_spike.py
```

Confirm timestamps on the DLL/exe you actually run (`situation_opengl.dll` vs `situation_vulkan.dll`).

## 2. Harness (automated) — run both backends

**OpenGL:**

```bat
build\sit_test.exe --module graphics --filter spirv
```

**Vulkan:**

```bat
build\sit_test_vulkan.exe --module graphics --filter spirv
```

Expected on each backend:

- `spirv_error_code_reporting`
- `spirv_memory_invalid_params`
- `spirv_memory_*` readback / bind tests (Vulkan uses `SituationLoadShaderFromSpirvMemoryEx` + layout profiles)

**OpenGL only** (Demon Hunt sky, OpenGL-target SPIR-V):

```bat
build\sit_test.exe --module graphics --filter demon_hunt_sky_spirv_begin_poll
```

**Vulkan only** (same demo shaders, Vulkan-target SPIR-V + UBO_SSBO profile):

```bat
build\sit_test_vulkan.exe --module graphics --filter demon_hunt_sky_spirv_vk
```

Requires `compile_demon_hunt_shaders.bat` first. Failures print full driver detail via `SituationGetLastErrorMsg`.

## 3. Demon Hunt demo (manual)

Build with `build_examples.bat opengl demon_hunt` or `build_examples.bat vulkan demon_hunt` (runs `compile_demon_hunt_shaders.bat` for both SPIR-V targets).

1. Delete old log: `build\examples\demon_hunt_sky.log`
2. Run `build\examples\demon_hunt.exe`
3. Stay on title screen until skydome GPU path is ready

**Expected log sequence** (v2.4.99+):

```
[demon_hunt] loading embedded SPIR-V (vs=632 fs=1304248 bytes, no runtime GLSL).
[demon_hunt] embedded SPIR-V kickoff OK (specialize/link polled per frame).
[demon_hunt] SPIR-V still loading ...
[demon_hunt] SPIR-V world shader linked; finishing GPU init.
[demon_hunt] skydome GPU path OK ...
```

## 4. Error codes (both backends)

On failure, read full detail:

```c
SituationError st = SituationPollShaderLoad(shader);
if (st != SITUATION_SUCCESS && st != SITUATION_ERROR_SHADER_LOAD_IN_PROGRESS) {
    SituationError code = SituationGetLastErrorCode();
    char* msg = NULL;
    SituationGetLastErrorMsg(&msg);
    /* OpenGL: program/shader info log; Vulkan: vkCreateShaderModule / pipeline detail */
    SituationFreeString(msg);
}
```

### OpenGL SPIR-V

| Code | Meaning |
|------|---------|
| -553 | `SHADER_LOAD_IN_PROGRESS` — keep polling |
| -636 | `OPENGL_SPIRV_UNAVAILABLE` |
| -637 | Invalid SPIR-V blob |
| -638 / -639 / -640 | VS / FS / CS specialize failed |
| -641 | Program link failed (e.g. NVIDIA `too many instructions`) |

### Vulkan SPIR-V

| Code | Meaning |
|------|---------|
| -553 | `SHADER_LOAD_IN_PROGRESS` — keep polling |
| -753 | `VULKAN_SPIRV_INVALID` |
| -754 / -755 / -756 | VS / FS / CS shader module failed |
| -633 | Pipeline creation failed (after modules OK) |

Shared: **`SituationErrorToString(code)`** for the enum label.

## 5. API contract

- **`SituationBeginLoadShaderFromSpirvMemory`**: non-blocking kickoff on **OpenGL and Vulkan** (Vulkan copies bytecode, builds pipeline on poll).
- **`SituationPollShaderLoad`**: call every frame on the main thread; advances async load for the active backend.
- **`SituationLoadShaderFromSpirvMemory`**: blocking full load (tools / tests).
- **`SituationLoadShaderFromSpirvMemoryEx`**: Vulkan only — `SituationSpirvLayoutProfile` (mesh / dual SSBO / UBO+SSBO).

## 6. Version check

`SituationGetVersion()` should report **2.4.106** after rebuilding both DLLs you test against.

## 7. Offline shader debug (both targets)

```bat
python scripts\spirv_shader_debug.py demon_hunt --devel
python scripts\spirv_shader_debug.py harness
```

See `doc/SHADER_DEBUG.md`. Demon Hunt reports **OpenGL** `.spv`; `harness` reports **OpenGL + Vulkan** harness blobs under `tests/harness/spirv_out/`.

---

See also: `doc/SHADER_DEBUG.md`, `doc/UPDATELOG.md` (v2.4.97–v2.4.102), `doc/plan/VULKAN_SPIRV_USER_DESCRIPTOR_PARITY.md`, `compile_demon_hunt_shaders.bat`, `compile_harness_shaders.bat`.
