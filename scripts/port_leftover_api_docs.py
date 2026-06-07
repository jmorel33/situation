#!/usr/bin/env python3
"""Port remaining SITAPI docs from header into doc/situation_api.md."""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
API_MD = ROOT / "doc" / "situation_api.md"

# Each value is markdown inserted before the anchor (must be unique in file).
INSERTS: dict[str, str] = {}

INSERTS["CORE_BEFORE_WINDOW"] = r'''
---
#### `SituationGetInitState`
Returns the library initialization state as a thread-safe **`SituationInitState`** enum. Use this to detect partial init, shutdown in progress, or re-entrancy during tooling that calls **`SituationInit`** / **`SituationShutdown`** in one process.

```c
SituationInitState SituationGetInitState(void);
```

**Returns:** One of **`SITUATION_INIT_STATE_UNINITIALIZED`**, **`INITIALIZING`**, **`INITIALIZED`**, **`SHUTTING_DOWN`**, etc. (see header).

**Usage Example:**
```c
if (SituationGetInitState() != SITUATION_INIT_STATE_INITIALIZED) {
    fprintf(stderr, "Situation not ready for this call\n");
    return;
}
```

---
#### `SituationGetGraphicsCaps`
Fills **`SituationGraphicsCaps`** with backend feature flags (bindless, compute, max push constant size, SPIR-V path, etc.). Examples and the test harness use this to skip or classify driver-specific tests.

```c
void SituationGetGraphicsCaps(SituationGraphicsCaps* out_caps);
```

**Parameters:**
- `out_caps` — Output struct; must not be NULL.

**Notes:** Safe after successful **`SituationInit`**. Values reflect the **active** backend (OpenGL vs Vulkan).

---
#### `SituationShowMessageBox`
Displays a blocking native message box (Win32 **`MessageBox`**, platform equivalent elsewhere). Intended for **fatal init errors** when stderr is not visible (GUI apps without a console).

```c
void SituationShowMessageBox(const char* title, const char* message);
```

**Parameters:**
- `title` — Dialog title (UTF-8).
- `message` — Body text (UTF-8).

**Notes:** Blocks until the user dismisses the dialog. Do not call from the audio callback thread.

'''

INSERTS["WINDOW_BEFORE_INPUT"] = r'''
---
#### `SituationSetMaximizeCallback`
Registers a callback invoked when the window is **maximized** or **restored** from maximized state. Complements **`SituationSetResizeCallback`** (framebuffer size) for UI that tracks window chrome state.

```c
void SituationSetMaximizeCallback(SituationMaximizeCallback callback, void* user_data);
```

**Parameters:**
- `callback` — `void (*)(bool maximized, void* user_data)` or NULL to clear.
- `user_data` — Opaque pointer passed to the callback.

'''

INSERTS["GRAPHICS_BEFORE_INPUT"] = r'''
---
### Dynamic raster state (command buffer)

Scoped overrides for cull, depth, and blend state within a render pass. Push before temporary state changes, pop to restore.

#### `SituationCmdPushRasterState` / `SituationCmdPopRasterState`
```c
SituationError SituationCmdPushRasterState(SituationCommandBuffer cmd, uint32_t scope_id);
SituationError SituationCmdPopRasterState(SituationCommandBuffer cmd, uint32_t scope_id);
```
**Notes:** `scope_id` is a user label for debug validation — mismatched push/pop pairs are reported in debug builds.

#### `SituationCmdSetCullMode`
```c
SituationError SituationCmdSetCullMode(SituationCommandBuffer cmd, SituationCullMode mode);
```
**Parameters:** `mode` — `SITUATION_CULL_NONE`, `SITUATION_CULL_FRONT`, or `SITUATION_CULL_BACK`.

#### `SituationCmdSetDepthTest` / `SituationCmdSetDepthWrite`
```c
SituationError SituationCmdSetDepthTest(SituationCommandBuffer cmd, bool enable, SituationDepthCompareOp depth_op);
SituationError SituationCmdSetDepthWrite(SituationCommandBuffer cmd, bool enable);
```

#### `SituationCmdSetBlendEnable` / `SituationCmdSetBlendFuncSeparate`
```c
SituationError SituationCmdSetBlendEnable(SituationCommandBuffer cmd, bool enable);
SituationError SituationCmdSetBlendFuncSeparate(SituationCommandBuffer cmd,
    SituationBlendFactor src_rgb, SituationBlendFactor dst_rgb,
    SituationBlendFactor src_a, SituationBlendFactor dst_a);
```

---
### Debug groups (command buffer)

#### `SituationCmdBeginDebugGroup` / `SituationCmdEndDebugGroup`
Annotate command buffer regions for RenderDoc, Xcode GPU capture, and **`SITUATION_VERBOSE_DIAGNOSTICS`** logging.

```c
SituationError SituationCmdBeginDebugGroup(SituationCommandBuffer cmd, const char* name, ColorRGBA color);
SituationError SituationCmdEndDebugGroup(SituationCommandBuffer cmd);
```

---
#### `SituationCmdSetPushConstantData`
Upload push constant bytes for a **bound shader** at a byte offset. Use when the generic **`SituationCmdSetPushConstant`** contract id path is insufficient (SPIR-V layouts with explicit offsets).

```c
SituationError SituationCmdSetPushConstantData(SituationCommandBuffer cmd, SituationShader shader,
    uint32_t offset, const void* data, size_t size);
```

---
### Camera & projection helpers

#### `SituationCameraDesc`
Shared inputs for view/projection builders and picking:

```c
typedef struct SituationCameraDesc {
    Vector3 eye, target, up;
    float fov_y_deg, aspect, near_z, far_z;
    bool perspective;
} SituationCameraDesc;
```

#### `SituationCameraBuildView` / `SituationCameraBuildProj` / `SituationCameraBuildViewProj`
```c
void SituationCameraBuildView(const SituationCameraDesc* desc, mat4 out_view);
void SituationCameraBuildProj(const SituationCameraDesc* desc, mat4 out_proj);
void SituationCameraBuildViewProj(const SituationCameraDesc* desc, mat4 out_vp);
```

#### `SituationCameraBuildInvViewProj`
```c
void SituationCameraBuildInvViewProj(const SituationCameraDesc* desc, mat4 out_inv_vp);
```

#### `SituationCameraUnprojectPixel`
Builds a world-space ray from a framebuffer pixel (picking, gizmos, editor tools).

```c
void SituationCameraUnprojectPixel(const SituationCameraDesc* desc, const mat4 inv_vp,
    Vector2 pixel, Vector2 framebuffer_px,
    Vector3* out_ray_origin, Vector3* out_ray_dir);
```

---
### Shader binding & validation (OpenGL-focused)

#### `SituationBindUniformBlock` / `SituationBindShaderStorageBlock`
Explicit **`glUniformBlockBinding`** / **`glShaderStorageBlockBinding`** when SPIR-V reflection reports binding 0 but GLSL used **`layout(binding=N)`**.

```c
SituationError SituationBindUniformBlock(SituationShader shader, const char* block_name, uint32_t binding_point);
SituationError SituationBindShaderStorageBlock(SituationShader shader, const char* block_name, uint32_t binding_point);
```

#### `SituationSetShaderUniform1fv` / `SituationSetShaderUniform1iv` / `SituationSetShaderUniformMatrix4fv`
Array uniform uploads on OpenGL. **`SituationSetShaderUniform1iv`** records **`SIT_OP_SET_UNIFORM`** when **`sit_render.in_frame`** is true (same deferral as scalar uniforms).

```c
SituationError SituationSetShaderUniform1fv(SituationShader shader, const char* uniform_name, int count, const float* values);
SituationError SituationSetShaderUniform1iv(SituationShader shader, const char* uniform_name, int count, const int* values);
SituationError SituationSetShaderUniformMatrix4fv(SituationShader shader, const char* uniform_name, int count, const mat4* matrices);
```

#### `SituationSetShaderUniformLocation`
Set uniform by **explicit location** (SPIR-V **`layout(location=)`**). Defers during active frames like **`SituationSetShaderUniform`**.

```c
SituationError SituationSetShaderUniformLocation(SituationShader shader, int location,
    const void* data, SituationUniformType type);
```

#### `SituationValidateShaderUniforms`
Checks that expected uniforms exist with compatible types before drawing. Fills **`error_buf`** with the first mismatch.

```c
SituationError SituationValidateShaderUniforms(SituationShader shader,
    const SituationUniformExpectation* table, int table_count,
    char* error_buf, size_t error_buf_size);
```

---
### GPU readback buffers

#### `SituationCreateReadbackBuffer`
Creates a staging buffer optimized for **GPU→CPU** copy + mapped read (NRC counters, harness tests).

```c
SituationError SituationCreateReadbackBuffer(size_t size, SituationBuffer* out_buffer);
```

#### `SituationCmdCopyBuffer`
Records **`src → dst`** copy at **`offset`** for **`size`** bytes. Completion is ordered by frame fence / **`SituationEndFrame`**.

```c
void SituationCmdCopyBuffer(SituationCommandBuffer cmd, SituationBuffer src, SituationBuffer dst,
    size_t offset, size_t size);
```

#### `SituationReadBuffer`
CPU **`memcpy`** from a readback buffer's mapped memory. Call **next frame** after the copy command.

```c
void SituationReadBuffer(SituationBuffer readback_buf, void* dst, size_t size);
```

See **`doc/situation_sdk_requirements.md`** § NRC adaptive training pattern.

---
### Texture queries & readback

#### `SituationGetTextureInfo`
```c
SituationError SituationGetTextureInfo(SituationTexture texture, SituationTextureInfo* out_info);
```

#### `SituationSetTextureSamplerParams`
Updates min/mag filter and wrap modes on an existing texture.

```c
SituationError SituationSetTextureSamplerParams(SituationTexture texture,
    SituationTextureFilter min_filter, SituationTextureFilter mag_filter,
    SituationTextureWrap wrap_s, SituationTextureWrap wrap_t);
```

#### `SituationReadTexture` / `SituationReadTextureAlloc` / `SituationReadFramebuffer`
Blocking CPU readback paths for tests, screenshots, and tools.

```c
SituationError SituationReadTexture(SituationTexture texture, const SituationTextureReadbackDesc* desc,
    void* dst_pixels, size_t dst_size_bytes);
SituationError SituationReadTextureAlloc(SituationTexture texture, const SituationTextureReadbackDesc* desc,
    SituationImage* out_image);
SituationError SituationReadFramebuffer(const SituationReadPixelsDesc* desc,
    void* dst_pixels, size_t dst_size_bytes);
```

---
### Virtual display dirty tracking

#### `SituationSetVirtualDisplayDirty` / `SituationIsVirtualDisplayDirty`
Mark whether a virtual display texture needs re-compositing this frame (skip work when unchanged).

```c
void SituationSetVirtualDisplayDirty(int display_id, bool is_dirty);
bool SituationIsVirtualDisplayDirty(int display_id);
```

#### `SituationGetLastVDCompositeTimeMS`
Returns milliseconds spent in the last **`SituationRenderVirtualDisplays`** composite pass (profiling HUD).

```c
double SituationGetLastVDCompositeTimeMS(void);
```

---
### Render lists & latency

#### `SituationSubmitRenderList`
Record **`SituationRenderList`** packets either **async** (worker + job id) or **immediate** (single-threaded fallback). Overloaded by signature:

```c
SituationJobId SituationSubmitRenderList(SituationThreadPool* pool, SituationRenderList list,
    void (*func)(void*, void*), void* user_data);
void SituationSubmitRenderList(SituationRenderList list,
    void (*func)(void*, void*), void* user_data);
```

#### `SituationGetRenderLatencyStats`
Query render-thread timing averages (nanoseconds) for diagnostics overlays.

```c
void SituationGetRenderLatencyStats(uint64_t* avg_ns, uint64_t* max_ns);
```

'''

INSERTS["AUDIO_BEFORE_NODE_GRAPH"] = r'''
---
### Extended device enumeration (Phase 0)

#### `SituationEnumerateAudioDevices`
Returns a heap-allocated device list with richer metadata than **`SituationGetAudioDevices`**. **Caller must free** with **`SituationFreeDeviceList`**.

```c
SituationAudioDeviceInfo* SituationEnumerateAudioDevices(int* out_count);
```

#### `SituationFindBestDevice`
Selects a device matching preferred type (playback/capture) and minimum channel counts.

```c
SituationAudioDeviceInfo* SituationFindBestDevice(SituationAudioDeviceType preferred_type,
    uint32_t min_channels_out, uint32_t min_channels_in);
```

#### `SituationFreeDeviceList`
```c
void SituationFreeDeviceList(SituationAudioDeviceInfo* devices, int count);
```

'''

INSERTS["NODE_GRAPH_BEFORE_FILESYSTEM"] = r'''
---
### Device registry

Built-in and custom audio node types register metadata (ports, controls, categories) used by **`SituationCreateNode`** and serialization.

#### `SituationInitDeviceRegistry`
Idempotent — registers all built-in FX/source/utility devices. Called automatically on first graph use; safe to call from tests.

```c
void SituationInitDeviceRegistry(void);
```

#### `SituationRegisterDeviceType` / `SituationIsDeviceRegistered` / `SituationGetRegisteredDeviceCount`
```c
SituationError SituationRegisterDeviceType(const SituationDeviceMetadata* meta);
bool SituationIsDeviceRegistered(SituationNodeType type);
int SituationGetRegisteredDeviceCount(void);
```

#### `SituationGetDeviceMetadata` / `SituationGetCategoryName`
```c
SituationError SituationGetDeviceMetadata(SituationNodeType type, SituationDeviceMetadata* out_meta);
char* SituationGetCategoryName(SituationDeviceCategory category);
```
**Note:** **`SituationGetCategoryName`** returns allocated string — free when done.

---
### Active graph (audio callback)

#### `SituationGetActiveGraph` / `SituationSetActiveGraph`
```c
SituationAudioGraph* SituationGetActiveGraph(void);
SituationError SituationSetActiveGraph(SituationAudioGraph* graph);
```
**Behavior:** **`SituationSetActiveGraph(NULL)`** disables graph processing in the miniaudio callback. Only one graph is mixed at a time.

---
### Graph serialization

Persist and restore node graphs as JSON (session save, editor files, harness round-trips).

#### `SituationSerializeGraphToJSON` / `SituationFreeJSONString`
```c
char* SituationSerializeGraphToJSON(const SituationAudioGraph* graph);
void SituationFreeJSONString(char* json_string);
```

#### `SituationDeserializeGraphFromJSON` / `SituationSaveGraphToFile` / `SituationLoadGraphFromFile`
```c
SituationError SituationDeserializeGraphFromJSON(SituationAudioGraph* graph, const char* json_string,
    const SituationDeviceFunctions* device_funcs, int num_device_funcs);
SituationError SituationSaveGraphToFile(const SituationAudioGraph* graph, const char* filepath);
SituationError SituationLoadGraphFromFile(SituationAudioGraph* graph, const char* filepath,
    const SituationDeviceFunctions* device_funcs, int num_device_funcs);
```

#### `SituationGetSerializationVersion` / `SituationIsVersionCompatible`
```c
char* SituationGetSerializationVersion(void);
bool SituationIsVersionCompatible(const char* json_version);
```

---
### Procedural tone routing (SFX → graph)

#### `SituationSetToneRouting` / `SituationSetGraphSFXSource`
Route **`SituationPlayToneEx`** output into the active graph's sound-source node instead of the legacy voice path.

```c
SituationError SituationSetToneRouting(SituationToneHandle handle, bool route_to_graph);
SituationError SituationSetGraphSFXSource(SituationNodeHandle handle);
```

---
### MIDI control & learn

Integrated MIDI for graph nodes. Full architecture: **`doc/midi_api.md`**.

#### Device control
```c
int SituationListMidiDevices(SituationMidiDeviceInfo* devices, int max_count);
SituationError SituationEnableMidiControl(SituationAudioGraph* graph, SituationNodeHandle handle, int device_id);
SituationError SituationDisableMidiControl(SituationAudioGraph* graph, SituationNodeHandle handle);
SituationError SituationAutoConnectMidi(SituationAudioGraph* graph, SituationNodeHandle handle);
int SituationIsMidiEnabled(SituationAudioGraph* graph, SituationNodeHandle handle);
```
**Note:** **`device_id = -1`** auto-selects the first available input. **`SituationAutoConnectMidi`** is shorthand for enable + auto-select.

#### Learn lifecycle
```c
SituationError SituationEnableMidiLearn(SituationAudioGraph* graph, SituationNodeHandle handle);
SituationError SituationDisableMidiLearn(SituationAudioGraph* graph, SituationNodeHandle handle);
int SituationIsMidiLearnEnabled(SituationAudioGraph* graph, SituationNodeHandle handle);
```

#### Learning operations
```c
SituationError SituationStartMidiLearn(SituationAudioGraph* graph, SituationNodeHandle handle,
    int control_index, const char* param_name, float min_value, float max_value, int scaling);
SituationError SituationCancelMidiLearn(SituationAudioGraph* graph, SituationNodeHandle handle);
int SituationIsLearning(SituationAudioGraph* graph, SituationNodeHandle handle);
```
**Scaling:** `0=linear`, `1=log`, `2=dB`, `3=discrete`. Learn mode **times out after 5 seconds** without a CC.

#### Mapping & presets
```c
SituationError SituationClearMidiMapping(SituationAudioGraph* graph, SituationNodeHandle handle, int control_index);
SituationError SituationClearAllMidiMappings(SituationAudioGraph* graph, SituationNodeHandle handle);
SituationError SituationSaveMidiPreset(SituationAudioGraph* graph, SituationNodeHandle handle, const char* filename);
SituationError SituationLoadMidiPreset(SituationAudioGraph* graph, SituationNodeHandle handle, const char* filename);
```

'''

INSERTS["THREADING_BEFORE_MISC"] = r'''
---
#### `SituationDispatchParallel`
Fork-join parallel loop over **`count`** indices using the thread pool. Each invocation receives **`index`** and **`user_data`**.

```c
void SituationDispatchParallel(SituationThreadPool* pool, int count, int min_batch_size,
    void (*func)(int index, void* user_data), void* user_data);
```

**Parameters:**
- `pool` — Worker pool (often **`SituationGetThreadPool()`**).
- `count` — Number of iterations.
- `min_batch_size` — Granularity hint; larger values reduce scheduling overhead.
- `func` — Called as `func(i, user_data)` for each index `i`.

---
#### `SituationSetThreadAffinity`
Pins the **calling thread** to logical cores selected in **`core_mask`** (bit N = core N). Returns **`false`** if the OS rejected the affinity request.

```c
bool SituationSetThreadAffinity(uint64_t core_mask);
```

**Usage:** Call from the thread you want to pin (e.g. audio or render worker after pool creation).

---
#### `SituationGetCPUCoreCount`
Returns the number of **physical** CPU cores detected at init (not hyper-thread logical count on all platforms).

```c
uint32_t SituationGetCPUCoreCount(void);
```

'''

INSERTS["LOGGING_BEFORE_COMPUTE"] = r'''
---
#### `SituationSetLogCallback`
Redirects library log output to a custom sink. Receives **`SituationLogLevel`**, message string, and **`user`** pointer.

```c
void SituationSetLogCallback(void (*callback)(SituationLogLevel level, const char* message, void* user), void* user);
```

**Notes:** Pass **`NULL`** callback to restore default stderr logging. Callback must be thread-safe if invoked from worker or audio threads.

'''

ANCHORS = {
    "CORE_BEFORE_WINDOW": '<summary><h3>Window and Display Module</h3></summary>',
    "WINDOW_BEFORE_INPUT": '<summary><h3>Input Module</h3></summary>',
    "GRAPHICS_BEFORE_INPUT": '<summary><h3>Input Module</h3></summary>',
    "AUDIO_BEFORE_NODE_GRAPH": '<summary><h3>Audio Node Graph Module</h3></summary>',
    "NODE_GRAPH_BEFORE_FILESYSTEM": '<summary><h3>Filesystem Module</h3></summary>',
    "THREADING_BEFORE_MISC": '<summary><h3>Miscellaneous Module</h3></summary>',
    "LOGGING_BEFORE_COMPUTE": '<summary><h3>Compute Shaders</h3></summary>',
}


def already_documented(text: str, snippet: str) -> bool:
    """True if the first #### entry in snippet exists in text."""
    m = re.search(r"#### `(\w+)`", snippet)
    return bool(m and m.group(1) in text)


def main() -> None:
    text = API_MD.read_text(encoding="utf-8")
    inserted = 0
  # Graphics before Input — insert before Input anchor but after Graphics section start
    for key in [
        "CORE_BEFORE_WINDOW",
        "WINDOW_BEFORE_INPUT",
        "GRAPHICS_BEFORE_INPUT",
        "AUDIO_BEFORE_NODE_GRAPH",
        "NODE_GRAPH_BEFORE_FILESYSTEM",
        "THREADING_BEFORE_MISC",
        "LOGGING_BEFORE_COMPUTE",
    ]:
        block = INSERTS[key].strip()
        if already_documented(text, block):
            print(f"skip {key} (already present)")
            continue
        anchor = ANCHORS[key]
        if key == "GRAPHICS_BEFORE_INPUT":
            # Insert at end of Graphics module (before Input), not at first Input occurrence from TOC
            gfx_start = text.find('<summary><h3>Graphics Module</h3></summary>')
            inp = text.find(anchor, gfx_start)
            if inp == -1:
                raise SystemExit(f"anchor not found for {key}")
            text = text[:inp] + "\n" + block + "\n\n" + text[inp:]
        else:
            idx = text.find(anchor)
            if idx == -1:
                raise SystemExit(f"anchor not found for {key}")
            text = text[:idx] + block + "\n\n" + text[idx:]
        inserted += 1
        print(f"inserted {key}")

    API_MD.write_text(text, encoding="utf-8")
    print(f"Updated {API_MD.relative_to(ROOT)} ({inserted} blocks)")


if __name__ == "__main__":
    main()
