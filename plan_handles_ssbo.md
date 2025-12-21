# Plan: Universal Handles & SSBO Standardization

## Overview
This document outlines the technical steps to standardize the "Situation" SDK on two AAA architecture pillars:
1.  **Universal `uint64_t` Handles:** Every resource (Texture, Mesh, Shader, Buffer) must be addressable by a single 64-bit integer. This enables "Bindless" workflows where resources are passed to shaders via push constants or root constants, rather than complex binding tables.
2.  **SSBO-First Data Access:** Moving away from fixed-function Vertex Attributes and Uniform Buffers towards "Pull-Model" vertex fetching and structured Shader Storage Buffers (SSBOs).

## 1. Universal Handle Standardization

### The Goal
Currently, `SituationTexture` is a struct `{ slot, generation, width, height }`. While safe, it cannot be passed directly to a shader as a single scalar.
We will transition to opaque `uint64_t` handles for the public API, packing metadata internally or looking it up.

### Packing Strategy (The "Handle" Format)
All handles will be 64-bit integers with the following bit layout (example):
*   **Bits 0-31 (32 bits):** Index (Slot)
*   **Bits 32-63 (32 bits):** Generation (Version/Validation)

This allows O(1) validation while fitting in a standard register.

### Tasks
1.  **Refactor Public Typedefs:**
    *   Redefine `SituationTexture`, `SituationBuffer`, `SituationShader`, `SituationMesh` as `uint64_t` (or a struct containing strictly one `uint64_t` to preserve type safety in C).
    *   *Alternative:* Keep structs for C type safety but provide `SituationGetTextureHandleID(tex)` that returns the raw `uint64_t`.
    *   *Decision:* Provide explicit `uint64_t` accessors for all resources.

2.  **Texture System Update:**
    *   Implement `uint64_t SituationGetTextureBindlessHandle(SituationTexture tex)`.
    *   **Vulkan:** Returns a Virtual Index into the global `descriptor_indexing` array.
    *   **OpenGL:** Returns the ARB_bindless_texture `uint64_t` handle.

3.  **Buffer System Update:**
    *   `SituationGetBufferDeviceAddress` (Already implemented) is the standard here.
    *   Ensure all buffers are created with `SHDER_DEVICE_ADDRESS` bit by default on supported hardware.

## 2. SSBO-First Architecture (The "Pull" Model)

### The Problem
Legacy rendering pushes data via `glVertexAttribPointer`. This requires the Input Assembler to fetch data, which is rigid and cache-inefficient for complex geometry.

### The Solution: Vertex Pulling
The Vertex Shader runs without attributes. It reads `gl_VertexIndex` and fetches data manually from a huge SSBO.

```glsl
struct Vertex { vec3 pos; float pad; vec3 norm; float pad2; vec2 uv; };
layout(buffer_reference, std430) readonly buffer VertexBuffer { Vertex v[]; };

void main() {
    Vertex vert = VertexBuffer(push_consts.vertex_buffer_ptr).v[gl_VertexIndex];
    // ...
}
```

### Tasks
1.  **Mesh Data Refactor:**
    *   Update `SituationCreateMesh` to upload data to a generic `SITUATION_BUFFER_USAGE_STORAGE_BUFFER` instead of (or in addition to) `VERTEX_BUFFER`.
    *   Ensure `SituationMesh` handle provides access to this buffer's Device Address.

2.  **Standardize SSBO Layouts:**
    *   Define strict C-struct alignment rules (STD430) for all data passed to GPU.
    *   Promote `SIT_COMPUTE_LAYOUT_BUFFER_IMAGE` and similar "One Big Buffer" layouts.

3.  **Deprecation Path:**
    *   Mark `SituationCmdSetVertexAttribute` as "Legacy/Fallback".
    *   New tutorials should strictly use SSBO pulling.

## 3. Implementation Phases

### Phase 1: API Surface (v2.4)
*   [ ] Add `SituationGetTextureID(tex)` returning `uint64_t`.
*   [ ] Add `SituationGetMeshVertexBufferAddress(mesh)` returning `uint64_t`.
*   [ ] Update `SituationShader` logic to support "Bindless" macros automatically.

### Phase 2: Backend Internals
*   [ ] **Vulkan:** Implement Global Descriptor Array (Unbounded) for textures.
*   [ ] **OpenGL:** Polyfill bindless handles where extensions allow, or emulate via slot mapping.

### Phase 3: The "Mega-Shader"
*   [ ] Release a standard PBR shader that uses exclusively Bindless resources and SSBO vertex pulling.
