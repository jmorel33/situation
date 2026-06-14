# Situation API — Generated Supplement

_Auto-generated from `sit/situation_api.h` — Situation **2.4.265 (YPQ Phase 3: public mapping diagnostics API, test_misc cleanup)**._

Regenerate:

```bat
python tools\generate_api_index.py
```

**Coverage:** 548/559 symbols in situation_api.md; **11** below.

---

## 3D Model Utilities

#### `SituationLoadModelFromOBJ`
Wavefront OBJ: triangulated meshes, MTL/textures; missing/degenerate normals filled from face geometry, authored normals preserved.
```c
SituationError SituationLoadModelFromOBJ(const char* file_path, SituationModel* out_model);
```

---

#### `SituationLoadModelFromSTL`
Loads a 3D model from a binary or ASCII STL file. UVs are zeroed; normals are flat (per-face) by default, or smooth (averaged per shared vertex) when smooth_normals is true.
```c
SituationError SituationLoadModelFromSTL(const char* file_path, bool smooth_normals, SituationModel* out_model);
```

---

## Color Space Conversions

#### `SituationYpqAnalyzeRgbMapping`
_See `sit/situation_api.h` for the authoritative declaration._
```c
SituationError SituationYpqAnalyzeRgbMapping(SituationYpqRgbMappingStats* out);
```

---

#### `SituationYpqSliceDuplicateCount`
_See `sit/situation_api.h` for the authoritative declaration._
```c
SituationError SituationYpqSliceDuplicateCount(char axis, int value, int* out_dup);
```

---

## Graphics Resource Management

#### `SituationGetMeshIndexBufferAddress`
Retrieves the GPU device address of the mesh index buffer. [VK] requires SIT_FEATURE_BINDLESS_BUFFERS; [GL] NVIDIA-only via NV_shader_buffer_load. Returns 0 if unsupported.
```c
uint64_t SituationGetMeshIndexBufferAddress(SituationMesh mesh);
```

---

#### `SituationGetMeshVertexBufferAddress`
Retrieves the GPU device address of the mesh vertex buffer. [VK] requires SIT_FEATURE_BINDLESS_BUFFERS; [GL] NVIDIA-only via NV_shader_buffer_load. Returns 0 if unsupported.
```c
uint64_t SituationGetMeshVertexBufferAddress(SituationMesh mesh);
```

---

## Image Loading and Unloading

#### `SituationIsStbImageLoadExtension`
True for stb_image decode extensions (.jpg, .png, .bmp, .tga, .psd, .gif, .hdr, .pic, .ppm, .pgm, .pnm).
```c
bool SituationIsStbImageLoadExtension(const char* extension);
```

---

## Virtual Displays (Render Targets)

#### `SituationGetVirtualDisplayUpdateInfo`
Query last VD content write (not the frame clock).
```c
SituationError SituationGetVirtualDisplayUpdateInfo(int display_id, double* out_last_content_update_time, uint64_t* out_last_content_update_frame, uint64_t* out_frames_since_update, double* out_seconds_since_update);
```

---

#### `SituationSetVirtualDisplayFallbackColor`
SOLID idle tint (normalized by compositor).
```c
void SituationSetVirtualDisplayFallbackColor(int display_id, ColorRGBA color);
```

---

#### `SituationSetVirtualDisplayFallbackMode`
SOLID or COLORBURST when idle.
```c
void SituationSetVirtualDisplayFallbackMode(int display_id, SituationVDFallbackMode mode);
```

---

#### `SituationSetVirtualDisplayIdleThreshold`
Set idle threshold for compositor fallback (Phase 2a).
```c
void SituationSetVirtualDisplayIdleThreshold(int display_id, double threshold_seconds);
```

---
