## Graphics Module

**Overview:** The Graphics module forms the core of the rendering pipeline, offering a powerful, backend-agnostic API for interacting with the GPU. It is responsible for all GPU resource management (meshes, shaders, textures) and its command-buffer-centric design (`SituationCmd...`) allows you to precisely sequence rendering operations.

**Command buffer catalog (Situation 2.4.125):** See **[situation_command_reference.md](../situation_command_reference.md)** for every `SituationCmd*` function — full signatures, OpenGL/Vulkan notes, recommended order, and use cases (render pass, raster state, descriptors, manual draw, compute, deprecated aliases).

### Structs, Enums, and Handles

#### `SituationCommandBuffer`
An opaque handle representing a command buffer, which is a list of rendering commands to be executed by the GPU. Command buffers record all rendering operations and are submitted to the GPU for execution.

```c
typedef struct SituationCommandBuffer_t* SituationCommandBuffer;
```

**Description:**
Command buffers are the primary way to record rendering work in Situation. All drawing commands (`SituationCmdDraw...`), state changes (`SituationCmdBind...`), and render passes are recorded into a command buffer before being submitted to the GPU.

**Usage Pattern:**
```c
// Get the main command buffer
SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

// Begin a render pass
SituationRenderPassInfo pass_info = {
    .target_display_id = -1,  // Main window
    .clear_color = {0.1f, 0.1f, 0.1f, 1.0f}
};
SituationCmdBeginRenderPass(cmd, &pass_info);

// Record rendering commands
SituationCmdBindPipeline(cmd, shader);
SituationCmdBindDescriptorSet(cmd, 0, descriptor_set);
SituationCmdDrawMesh(cmd, mesh);

// End the render pass
SituationCmdEndRenderPass(cmd);

// Command buffer is automatically submitted at end of frame
```

**Key Concepts:**
- **Recording:** Commands are recorded into the buffer but not executed immediately
- **Submission:** The buffer is submitted to the GPU at the end of the frame
- **Reuse:** The main command buffer is reused each frame
- **Thread Safety:** Command buffers are not thread-safe; use one per thread

**Common operations** (see [situation_command_reference.md](../situation_command_reference.md) for the complete list):

```c
// Render pass
SituationCmdBeginRenderPass(cmd, &pass_info);
SituationCmdSetViewport(cmd, 0, 0, w, h);
SituationCmdEndRenderPass(cmd);

// Graphics pipeline + resources
SituationCmdBindPipeline(cmd, shader);
SituationCmdBindDescriptorSet(cmd, 0, ubo);
SituationCmdBindTextureSet(cmd, 1, albedo);

// Low-level draw (Core)
SituationCmdBindVertexBuffer(cmd, 0, vbo, 0, vertex_stride);
SituationCmdBindIndexBuffer(cmd, ibo, 0);
SituationCmdDrawIndexed(cmd, index_count, 1, 0, 0, 0);

// High-level draw
SituationCmdDrawMesh(cmd, mesh);

// Compute + sync
SituationCmdBindComputePipeline(cmd, compute);
SituationCmdDispatch(cmd, gx, gy, gz);
SituationCmdPipelineBarrier(cmd, SITUATION_BARRIER_COMPUTE_SHADER_WRITE,
                                SITUATION_BARRIER_VERTEX_SHADER_READ);
```

**Notes:**
- Opaque handle - internal structure is hidden
- Main command buffer obtained via `SituationGetMainCommandBuffer()`
- Commands execute in the order they're recorded
- Automatically submitted at `SituationEndFrame()`
- Reset automatically each frame

---
#### `SituationAttachmentInfo`
Describes a single attachment (like a color or depth buffer) for a render pass. It specifies how the attachment's contents should be handled at the beginning and end of the pass.
```c
typedef struct SituationAttachmentInfo {
    SituationAttachmentLoadOp  loadOp;
    SituationAttachmentStoreOp storeOp;
    SituationClearValue        clear;
} SituationAttachmentInfo;
```
-   `loadOp`: The action to take at the beginning of the render pass.
    -   `SIT_LOAD_OP_LOAD`: Preserve the existing contents of the attachment.
    -   `SIT_LOAD_OP_CLEAR`: Clear the attachment to the value specified in `clear`.
    -   `SIT_LOAD_OP_DONT_CARE`: The existing contents are undefined and can be discarded.
-   `storeOp`: The action to take at the end of the render pass.
    -   `SIT_STORE_OP_STORE`: Store the rendered contents to memory.
    -   `SIT_STORE_OP_DONT_CARE`: The rendered contents may be discarded.
-   `clear`: A struct containing the color or depth/stencil values to use if `loadOp` is `SIT_LOAD_OP_CLEAR`.

---
#### `SituationClearValue`
A union that specifies the clear values for color and depth/stencil attachments. It is used within the `SituationAttachmentInfo` struct to define what value an attachment should be cleared to at the start of a render pass.
```c
typedef union SituationClearValue {
    ColorRGBA color;
    struct {
        double depth;
        int32_t stencil;
    } depth_stencil;
} SituationClearValue;
```
-   `color`: The RGBA color value to clear a color attachment to.
-   `depth_stencil`: A struct containing the depth and stencil values to clear a depth/stencil attachment to.
    -   `depth`: The depth value, typically `1.0` for clearing.
    -   `stencil`: The stencil value, typically `0`.

---
#### `SituationRenderPassInfo`
Configures a rendering pass. This is a crucial struct used with `SituationCmdBeginRenderPass()` to define the render target and its initial state.
```c
typedef struct SituationRenderPassInfo {
    int                     display_id;
    SituationAttachmentInfo color_attachment;
    SituationAttachmentInfo depth_attachment;
} SituationRenderPassInfo;
```
-   `display_id`: The ID of a `SituationVirtualDisplay` to render to. Use `-1` to target the main window's backbuffer.
-   `color_attachment`: Configuration for the color buffer, including load/store operations and clear color.
-   `depth_attachment`: Configuration for the depth buffer, including load/store operations and clear value.

---
#### `ViewDataUBO`
Defines the standard memory layout for a Uniform Buffer Object (UBO) containing camera projection and view matrices. You don't typically create this struct directly; rather, you should structure your GLSL uniform blocks to match this layout to be compatible with the library's default scene data.
```c
typedef struct ViewDataUBO {
    mat4 view;
    mat4 projection;
} ViewDataUBO;
```
-   `view`: The view matrix, which transforms world-space coordinates to view-space (camera) coordinates.
-   `projection`: The projection matrix, which transforms view-space coordinates to clip-space coordinates.

---
#### Resource Handles
The following are opaque handles to GPU resources. Their internal structure is not exposed to the user. You create them with `SituationCreate...` or `SituationLoad...` functions and free them with their corresponding `SituationDestroy...` or `SituationUnload...` functions.

#### `SituationMesh`
An opaque handle to a self-contained GPU resource representing a drawable mesh. A mesh bundles a vertex buffer and an optional index buffer, representing a complete piece of geometry that can be rendered with a single command.
```c
typedef struct SituationMesh {
    uint64_t id;
    int index_count;
} SituationMesh;
```
- **Creation:** `SituationCreateMesh()`
- **Usage:** `SituationCmdDrawMesh()`
- **Destruction:** `SituationDestroyMesh()`

---
#### `SituationBuffer`
An opaque handle to a generic region of GPU memory. Buffers are highly versatile and can be used to store vertex data, index data, uniform data for shaders (UBOs), or general-purpose storage data (SSBOs). The intended usage is specified on creation using `SituationBufferUsageFlags`.
```c
typedef struct SituationBuffer {
    uint64_t id;
    size_t size_in_bytes;
} SituationBuffer;
```
- **Creation:** `SituationCreateBuffer()`
- **Usage:** `SituationUpdateBuffer()`, `SituationCmdBindVertexBuffer()`, `SituationCmdBindIndexBuffer()`, `SituationCmdBindDescriptorSet()`
- **Destruction:** `SituationDestroyBuffer()`

---
#### `SituationComputePipeline`
An opaque handle representing a compiled compute shader program. It encapsulates a single compute shader stage and its resource layout, ready to be dispatched for general-purpose GPU computation.
```c
typedef struct SituationComputePipeline {
    uint64_t id;
} SituationComputePipeline;
```
- **Creation:** `SituationCreateComputePipeline()`
- **Usage:** `SituationCmdBindComputePipeline()`, `SituationCmdDispatch()`
- **Destruction:** `SituationDestroyComputePipeline()`

---
#### `SituationShader`
An opaque handle representing a compiled graphics shader pipeline. It encapsulates a vertex shader, a fragment shader, and the state required to use them for rendering (like vertex input layout and descriptor set layouts).
```c
typedef struct SituationShader {
    uint64_t id;
} SituationShader;
```
- **Creation:** `SituationLoadShader()`, `SituationLoadShaderFromMemory()`, `SituationBeginLoadShaderFromMemory()` (+ poll), `SituationLoadShaderFromSpirv*`, `SituationBeginLoadShaderFromSpirvMemory(Ex)`
- **Usage:** `SituationCmdBindPipeline()`
- **Destruction:** `SituationUnloadShader()`

---
#### `SituationTexture`
An opaque handle to a GPU texture resource. Textures are created by uploading `SituationImage` data from the CPU. They are used by shaders for sampling colors (e.g., albedo maps) or as storage images for compute operations.
```c
typedef struct SituationTexture {
    uint64_t id;
    int width;
    int height;
    int mipmaps;
} SituationTexture;
```
-   `width`, `height`: The dimensions of the texture in pixels.
-   `mipmaps`: The number of mipmap levels in the texture.
- **Creation:** `SituationCreateTexture()`
- **Usage:** `SituationCmdBindShaderTexture()`, `SituationCmdBindComputeTexture()`
- **Destruction:** `SituationDestroyTexture()`

---
#### `SituationModelMesh`
Represents a single drawable sub-mesh within a larger `SituationModel`. It combines the raw geometry (`SituationMesh`) with a full PBR (Physically-Based Rendering) material definition, including color factors and texture maps.
```c
typedef struct SituationModelMesh {
    SituationMesh mesh;
    // Material Data
    int material_id;
    char material_name[SITUATION_MAX_NAME_LEN];
    ColorRGBA base_color_factor;
    float metallic_factor;
    float roughness_factor;
    vec3 emissive_factor;
    float alpha_cutoff;
    bool double_sided;
    // Texture Maps (if available)
    SituationTexture base_color_texture;
    SituationTexture metallic_roughness_texture;
    SituationTexture normal_texture;
    SituationTexture occlusion_texture;
    SituationTexture emissive_texture;
} SituationModelMesh;
```
-   `mesh`: The `SituationMesh` handle containing the vertex and index buffers for this part of the model.
-   `material_name`: The name of the material.
-   `base_color_factor`, `metallic_factor`, `roughness_factor`: PBR material parameters.
-   `base_color_texture`, `metallic_roughness_texture`, etc.: Handles to the GPU textures used by this material.

---
#### `SituationModel`
A handle representing a complete 3D model, loaded from a file (e.g., GLTF). It acts as a container for all the `SituationModelMesh` and `SituationTexture` resources that make up the model.
```c
typedef struct SituationModel {
    SituationModelMesh* meshes;
    SituationTexture* all_model_textures;
    int mesh_count;
    int texture_count;
} SituationModel;
```
-   `meshes`: A pointer to an array of the model's sub-meshes.
-   `all_model_textures`: A pointer to an array of all unique textures used by the model.
-   `mesh_count`, `texture_count`: The number of meshes and textures in their respective arrays.
- **Creation:** `SituationLoadModel()`
- **Usage:** `SituationDrawModel()`
- **Destruction:** `SituationUnloadModel()`

---
#### `SituationBufferUsageFlags`
Specifies how a `SituationBuffer` will be used. This helps the driver place the buffer in the most optimal memory. Combine flags using the bitwise `|` operator.
| Flag | Description |
|---|---|
| `SITUATION_BUFFER_USAGE_VERTEX_BUFFER` | The buffer will be used as a source of vertex data. |
| `SITUATION_BUFFER_USAGE_INDEX_BUFFER` | The buffer will be used as a source of index data. |
| `SITUATION_BUFFER_USAGE_UNIFORM_BUFFER` | The buffer will be used as a Uniform Buffer Object (UBO). |
| `SITUATION_BUFFER_USAGE_STORAGE_BUFFER` | The buffer will be used as a Shader Storage Buffer Object (SSBO). |
| `SITUATION_BUFFER_USAGE_INDIRECT_BUFFER`| The buffer will be used for indirect drawing commands. |
| `SITUATION_BUFFER_USAGE_TRANSFER_SRC` | The buffer can be used as a source for a copy operation. |
| `SITUATION_BUFFER_USAGE_TRANSFER_DST` | The buffer can be used as a destination for a copy operation. |
| `SITUATION_BUFFER_USAGE_DEVICE_ADDRESS` | Buffer can be accessed via a 64-bit GPU device address (bindless buffer references). |
| **Presets** | |
| `SITUATION_BUFFER_USAGE_VERTEX_AND_STORAGE` | `VERTEX_BUFFER \| STORAGE_BUFFER` — shared vertex + shader read/write. |
| `SITUATION_BUFFER_USAGE_DYNAMIC_VERTEX` | `VERTEX_BUFFER \| TRANSFER_DST` — frequently updated vertex buffer. |
| `SITUATION_BUFFER_USAGE_DYNAMIC_UNIFORM` | `UNIFORM_BUFFER \| TRANSFER_DST` — frequently updated UBO. |
| `SITUATION_BUFFER_USAGE_STORAGE_COMPUTE` | `STORAGE_BUFFER \| TRANSFER_SRC \| TRANSFER_DST \| DEVICE_ADDRESS` — full-featured compute/bindless SSBO. |

---
#### `SituationComputeLayoutType`
Defines a set of common, pre-configured layouts for compute pipelines, telling the GPU what kind of resources the shader expects.

| Type | Description |
|---|---|
| `SIT_COMPUTE_LAYOUT_ONE_SSBO` | One SSBO at binding 0 (Set 0). |
| `SIT_COMPUTE_LAYOUT_TWO_SSBOS` | Two SSBOs at bindings 0 and 1 (Set 0). |
| `SIT_COMPUTE_LAYOUT_IMAGE_AND_SSBO` | One Storage Image at binding 0, one SSBO at binding 1 (Set 0). |
| `SIT_COMPUTE_LAYOUT_PUSH_CONSTANT` | 64-byte push constant range (no descriptor sets). |
| `SIT_COMPUTE_LAYOUT_EMPTY` | No external resources. |
| `SIT_COMPUTE_LAYOUT_BUFFER_IMAGE` | One SSBO at binding 0, one Storage Image at binding 1 (Set 0). |
| **`SIT_COMPUTE_LAYOUT_GRID`** | Cell SSBO (set 0), storage image (set 1), font sampler (set 2), overlay sampler (set 3) — `sit/gpu/grid.comp`. |
| `SIT_COMPUTE_LAYOUT_TERMINAL` | **Deprecated alias** — identical to `SIT_COMPUTE_LAYOUT_GRID` (K-Term / wrapper compat). |

---
#### Resource Handles
`SituationMesh`, `SituationShader`, `SituationTexture`, `SituationBuffer`, `SituationModel`, `SituationComputePipeline`: These are opaque handles to GPU resources. Their internal structure is not exposed to the user. You create them with `SituationCreate...` or `SituationLoad...` functions and free them with their corresponding `SituationDestroy...` or `SituationUnload...` functions.

#### Functions
### Functions

#### Frame Lifecycle & Command Buffer
These functions control the overall rendering loop.

---
---
#### `SituationAcquireFrameCommandBuffer` _(v2.4.203: bool → SituationError)_
Prepares the backend for a new frame of rendering, acquiring the next available render target from the swap chain. This is the first function to call in the render phase and it must be guarded by a conditional check. It returns `SITUATION_SUCCESS` if the frame was acquired, or an error code if the frame cannot be acquired (e.g., `SITUATION_ERROR_NOT_INITIALIZED`, `SITUATION_ERROR_VULKAN_SWAPCHAIN_FAILED`, or `SITUATION_ERROR_GENERAL` when the window is minimized), in which case you should skip all rendering for that frame.
```c
SituationError SituationAcquireFrameCommandBuffer(void);
```
**Usage Example:**
```c
// At the start of the rendering phase
if (SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS) {
    // It's safe to record rendering commands now.
    // ...
    SituationEndFrame();
} else {
    // Skip rendering for this frame.
}
```

---
#### `SituationEndFrame`
Submits all recorded commands for the frame to the GPU for execution and presents the final rendered image to the screen. This is the last function to call in the render phase.
```c
SituationError SituationEndFrame(void);
```
**Usage Example:**
```c
// At the very end of the rendering phase
if (SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS) {
    // ... record all rendering commands ...

    // Finally, submit and present the frame.
    SituationEndFrame();
}
```

---
#### `SituationGetMainCommandBuffer`
Gets a handle to the main command buffer. This command buffer is used for all rendering that targets the main window or virtual displays.

```c
SituationCommandBuffer SituationGetMainCommandBuffer(void);
```

**Returns:** Handle to the main command buffer

**Usage Example:**
```c
// Basic rendering
SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

SituationRenderPassInfo pass_info = {
    .target_display_id = -1,  // Main window
    .clear_color = {0.1f, 0.1f, 0.1f, 1.0f}
};

SituationCmdBeginRenderPass(cmd, &pass_info);
SituationCmdBindPipeline(cmd, shader);
SituationCmdDrawMesh(cmd, mesh);
SituationCmdEndRenderPass(cmd);

// Multi-pass rendering
SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

// Pass 1: Render scene to virtual display
SituationRenderPassInfo vd_pass = {
    .target_display_id = 0,
    .clear_color = {0.0f, 0.0f, 0.0f, 1.0f}
};
SituationCmdBeginRenderPass(cmd, &vd_pass);
RenderScene();
SituationCmdEndRenderPass(cmd);

// Pass 2: Post-process to main window
SituationRenderPassInfo main_pass = {
    .target_display_id = -1,
    .clear_color = {0.0f, 0.0f, 0.0f, 1.0f}
};
SituationCmdBeginRenderPass(cmd, &main_pass);
ApplyBloom();
SituationCmdEndRenderPass(cmd);
```

**Notes:**
- Returns the same buffer each frame
- All rendering commands go through this buffer
- Automatically submitted at end of frame
- Must be recorded from the main thread (render thread handles execution internally)

---

### `SituationCmd*` — command buffer recording

**Canonical reference:** **[situation_command_reference.md](../situation_command_reference.md)** (Situation 2.4.125+).

All GPU recording functions use `SituationGetMainCommandBuffer()` between `SituationAcquireFrameCommandBuffer()` and `SituationEndFrame()`. Signatures, OpenGL/Vulkan matrix, ordering, and use cases are maintained in the command reference only.

| Section | Commands |
|---------|----------|
| [§1 Render pass](../situation_command_reference.md#1-render-pass--framebuffer) | `BeginRenderPass`, `EndRenderPass` |
| [§2 Viewport / scissor](../situation_command_reference.md#2-dynamic-viewport--scissor) | `SetViewport`, `SetScissor` |
| [§3 Raster state](../situation_command_reference.md#3-raster-state-fixed-function) | `PushRasterState`, `PopRasterState`, `SetCullMode`, `SetDepthTest`, `SetDepthWrite`, `SetBlendEnable`, `SetBlendFuncSeparate` |
| [§4 Pipeline & descriptors](../situation_command_reference.md#4-graphics-pipeline--shader-data) | `BindPipeline`, `SetPushConstant`, `SetPushConstantData`, `BindDescriptorSet`, `BindDescriptorSetDynamic`, `BindTextureSet`, `BindSampledTexture` |
| [§5 Vertex input & draw](../situation_command_reference.md#5-vertex-input--manual-draw-core-path) | `SetVertexAttribute` (GL), `BindVertexBuffer`, `BindIndexBuffer`, `Draw`, `DrawIndexed` |
| [§6 High-level draw](../situation_command_reference.md#6-high-level-draw-helpers) | `DrawMesh`, `DrawQuad`, `DrawTexture`, `DrawText`, `DrawTextEx` |
| [§7 Compute](../situation_command_reference.md#7-compute) | `BindComputePipeline`, `BindComputeTexture`, `Dispatch`, `PipelineBarrier` |
| [§8 Transfer](../situation_command_reference.md#8-transfer--presentation) | `CopyBuffer`, `Present` |
| [§9 Debug](../situation_command_reference.md#9-debug-markers) | `BeginDebugGroup`, `EndDebugGroup` |
| [§10 Recipe](../situation_command_reference.md#10-recommended-command-order-one-3d-object-core-path) | Full frame ordering example |
| [§11 Deprecated](../situation_command_reference.md#11-deprecated-commands) | `BeginRenderToDisplay`, `EndRender`, `BindUniformBuffer`, `BindTexture`, `BindComputeBuffer`, `MemoryBarrier` |

**Related non-command APIs in this file:** `SituationCreateMesh`, `SituationLoadShader`, `SituationUpdateBuffer`, `SituationSetShaderUniform*`, virtual displays, compute pipeline creation, readback buffers.

---
#### Resource Management
These functions create and destroy GPU resources.

---

#### `SituationShowCursor`

Shows the cursor.

```c
SITAPI void SituationShowCursor(void);
```

**Usage Example:**
```c
SituationShowCursor();
```

---
#### `SituationCreateMesh`
Creates a self-contained GPU mesh from vertex and index data. This operation uploads the provided data to video memory.
```c
SituationError SituationCreateMesh(const void* vertex_data, int vertex_count, size_t vertex_stride, const uint32_t* index_data, int index_count, SituationMesh* out_mesh);
```
**Usage Example:**
```c
// Define vertex and index data for a quad.
MyVertex vertices[] = { ... };
uint32_t indices[] = { ... };

// Create the mesh resource.
SituationMesh quad_mesh;
if (SituationCreateMesh(vertices, 4, sizeof(MyVertex), indices, 6, &quad_mesh) == SITUATION_SUCCESS) {
    // ...
}
```

---
#### `SituationDestroyMesh`
Destroys a mesh and frees its associated GPU memory. The handle is invalidated after this call.
```c
void SituationDestroyMesh(SituationMesh* mesh);
```
**Usage Example:**
```c
// Assume quad_mesh is a valid handle created with SituationCreateMesh.
// At application shutdown or when the mesh is no longer needed:
SituationDestroyMesh(&quad_mesh);
// The quad_mesh handle is now invalid.
```

---
#### `SituationGetMeshData`
Retrieves the vertex and index data from a mesh for CPU-side processing.
```c
void SituationGetMeshData(
    SituationMesh mesh,
    float** out_vertices,
    uint32_t** out_indices,
    uint32_t* out_vertex_count,
    uint32_t* out_index_count
);
```
**Usage Example:**
```c
// Extract mesh data for physics collision
float* vertices;
uint32_t* indices;
uint32_t vertex_count, index_count;

SituationGetMeshData(mesh, &vertices, &indices, &vertex_count, &index_count);

// Use data for collision detection
BuildCollisionMesh(vertices, vertex_count, indices, index_count);
```

---
#### `SituationLoadShader`
Loads, compiles, and links a graphics shader pipeline from GLSL vertex and fragment shader files.
```c
SituationError SituationLoadShader(const char* vs_path, const char* fs_path, SituationShader* out_shader);
```
**Usage Example:**
```c
// At application startup, load the main shader.
SituationShader main_shader;
SituationLoadShader("shaders/main.vert", "shaders/main.frag", &main_shader);
```

---
#### `SituationUnloadShader`
Unloads a shader pipeline and frees its associated GPU resources.
```c
void SituationUnloadShader(SituationShader* shader);
```
**Usage Example:**
```c
// At application shutdown, unload the main shader.
SituationUnloadShader(&main_shader);
```

---
#### `SituationCreateTexture`
Creates a GPU texture from a CPU-side `SituationImage`. This involves uploading the pixel data from RAM to VRAM.
```c
SituationError SituationCreateTexture(SituationImage image, bool generate_mipmaps, SituationTexture* out_texture);
```
**Usage Example:**
```c
// Load a CPU image from a file.
SituationImage cpu_image;
if (SituationLoadImage("textures/player_character.png", &cpu_image) == SITUATION_SUCCESS) {
    // Create a GPU texture from the image, generating mipmaps for better quality.
    SituationTexture player_texture;
    if (SituationCreateTexture(cpu_image, true, &player_texture) == SITUATION_SUCCESS) {
        // The CPU-side image can now be unloaded as the data is on the GPU.
        SituationUnloadImage(cpu_image);
    }
}
```

---
#### `SituationLoadTexture`
Loads a texture from an image file.
```c
SituationTexture SituationLoadTexture(const char* filename);
```
**Usage Example:**
```c
SituationTexture diffuse = SituationLoadTexture("textures/diffuse.png");
```

---
#### `SituationCreateTextureEx`
Creates a texture with advanced options (mipmaps, filtering, wrapping).
```c
SituationTexture SituationCreateTextureEx(
    SituationImage* image,
    SituationTextureFilter filter,
    SituationTextureWrap wrap,
    bool generate_mipmaps
);
```
**Usage Example:**
```c
// Create texture with trilinear filtering and mipmaps
SituationTexture tex = SituationCreateTextureEx(
    &image,
    SIT_FILTER_TRILINEAR,
    SIT_WRAP_REPEAT,
    true  // Generate mipmaps
);
```

---
#### `SituationDestroyTexture`
Destroys a texture and frees its associated GPU memory. The handle is invalidated after this call.
```c
void SituationDestroyTexture(SituationTexture* texture);
```
**Usage Example:**
```c
// Assume player_texture is a valid handle.
// At application shutdown or when the texture is no longer needed:
SituationDestroyTexture(&player_texture);
// The player_texture handle is now invalid.
```

---
#### `SituationUpdateTexture`
Updates a texture with new pixel data from a `SituationImage`.
```c
void SituationUpdateTexture(SituationTexture texture, SituationImage image);
```
**Usage Example:**
```c
// Create a blank texture
SituationImage blank = SituationGenImageColor(256, 256, (ColorRGBA){0,0,0,255});
SituationTexture dynamic_texture;
SituationCreateTexture(blank, false, &dynamic_texture);
SituationUnloadImage(blank);

// Later, in the update loop, generate new image data
SituationImage new_data = generate_procedural_image();
SituationUpdateTexture(dynamic_texture, new_data);
SituationUnloadImage(new_data);
```

**Pro Tip (Zero-Copy Update):**
If you already have a raw data buffer (e.g., from a procedural generation function) and want to avoid allocating a new `SituationImage` on the heap, you can wrap your raw pointer in a stack-allocated `SituationImage`.
```c
// 'my_raw_pixels' is a pointer to your RGBA data.
SituationImage wrapper = {
    .width = 256,
    .height = 256,
    .data = my_raw_pixels,
    // .format defaults to 0 (RGBA), .mipmaps to 0/1
};
SituationUpdateTexture(dynamic_texture, wrapper);
// No need to call SituationUnloadImage(wrapper) since it owns nothing.
```

---
#### `SituationGetTextureHandle`
Retrieves the bindless texture handle for a texture. This 64-bit handle can be passed to shaders to enable bindless texture access, allowing thousands of textures to be used without explicit binding.

```c
uint64_t SituationGetTextureHandle(SituationTexture texture);
```

**Parameters:**
- `texture` - The texture to get the handle for

**Returns:** 64-bit bindless texture handle, or 0 if not supported

**Usage Example:**
```c
/* GLSL Shader with bindless textures:
#version 450
#extension GL_ARB_bindless_texture : require
#extension GL_ARB_gpu_shader_int64 : require

layout(push_constant) uniform PushConstants {
    uint64_t texture_handle;
    mat4 mvp;
};

void main() {
    sampler2D tex = sampler2D(texture_handle);
    vec4 color = texture(tex, v_texcoord);
    frag_color = color;
}
*/

// Get bindless handles for multiple textures
uint64_t texture_handles[MAX_MATERIALS];
for (int i = 0; i < material_count; i++) {
    texture_handles[i] = SituationGetTextureHandle(materials[i].albedo_texture);
}

// Pass handle to shader via push constant
struct PushConstants {
    uint64_t texture_handle;
    mat4 mvp;
} pc = {
    .texture_handle = texture_handles[material_id],
    .mvp = mvp_matrix
};

SituationCmdSetPushConstant(cmd, 0, &pc, sizeof(pc));
SituationCmdDrawMesh(cmd, mesh);
```

**Notes:**
- **OpenGL:** Requires `GL_ARB_bindless_texture` extension
- **Vulkan:** Not yet implemented (returns 0)
- Enables rendering with thousands of unique textures without binding overhead
- Texture must remain valid while handle is in use
- Handles are resident (always accessible) once obtained
- Part of modern GPU-driven rendering techniques

---
#### `SituationGetTextureFormat`
Gets the internal GPU format of a texture.
```c
int SituationGetTextureFormat(SituationTexture texture);
```
**Usage Example:**
```c
int format = SituationGetTextureFormat(my_texture);
// The format will be one of the backend-specific pixel format enums (e.g., GL_RGBA8)
printf("Texture format ID: %d\n", format);
```

---
#### `SituationLoadModel`
Loads a 3D model from a file (GLTF, OBJ). This function parses the model file and uploads all associated meshes and materials to the GPU.
```c
SituationError SituationLoadModel(const char* file_path, SituationModel* out_model);
```
**Usage Example:**
```c
// At application startup, load the player model.
SituationModel player_model;
SituationLoadModel("models/player.gltf", &player_model);
```

---
#### `SituationUnloadModel`
Unloads a model and all of its associated resources (meshes, materials) from GPU memory.
```c
void SituationUnloadModel(SituationModel* model);
```
**Usage Example:**
```c
// At application shutdown, unload the player model.
SituationUnloadModel(&player_model);
```

---
#### `SituationCreateBuffer`
Creates a generic GPU buffer and optionally initializes it with data. Buffers can be used for vertices, indices, uniforms (UBOs), or storage (SSBOs).
```c
SituationError SituationCreateBuffer(size_t size, const void* initial_data, SituationBufferUsageFlags usage_flags, SituationBuffer* out_buffer);
```
**Usage Example:**
```c
// Create a uniform buffer for camera matrices
mat4 proj, view;
// ... calculate projection and view matrices ...
CameraMatrices ubo_data = { .projection = proj, .view = view };
SituationBuffer camera_ubo;
if (SituationCreateBuffer(sizeof(ubo_data), &ubo_data, SITUATION_BUFFER_USAGE_UNIFORM_BUFFER, &camera_ubo) == SITUATION_SUCCESS) {
    // ... use the buffer ...
}
```

---
#### `SituationDestroyBuffer`
Destroys a GPU buffer and frees its associated video memory. The handle is invalidated after this call.
```c
void SituationDestroyBuffer(SituationBuffer* buffer);
```
**Usage Example:**
```c
// Assume camera_ubo is a valid SituationBuffer handle created earlier.
// At application shutdown or when the buffer is no longer needed:
SituationDestroyBuffer(&camera_ubo);
// The camera_ubo handle is now invalid and should not be used.
```

---
#### `SituationGetBufferDeviceAddress`
Retrieves the GPU device address of a buffer as a 64-bit pointer. This address can be passed to shaders via push constants to enable bindless buffer access using the `buffer_reference` extension.

```c
uint64_t SituationGetBufferDeviceAddress(SituationBuffer buffer);
```

**Parameters:**
- `buffer` - The buffer to get the address of

**Returns:** 64-bit GPU device address, or 0 if not supported

**Usage Example:**
```c
/* GLSL Shader with buffer_reference:
#version 450
#extension GL_EXT_buffer_reference : require

layout(buffer_reference, std430) buffer VertexData {
    vec3 position;
    vec2 texcoord;
};

layout(push_constant) uniform PushConstants {
    uint64_t vertex_buffer_address;
    mat4 mvp;
};

void main() {
    VertexData vertex = VertexData(vertex_buffer_address);
    gl_Position = mvp * vec4(vertex.position, 1.0);
}
*/

// Create buffer and get its GPU address
SituationBuffer vertex_buffer = SituationCreateBuffer(
    vertex_count * sizeof(Vertex),
    SITUATION_BUFFER_USAGE_STORAGE);

uint64_t buffer_address = SituationGetBufferDeviceAddress(vertex_buffer);

// Pass address to shader via push constant
struct PushConstants {
    uint64_t vertex_buffer_address;
    mat4 mvp;
} pc = {
    .vertex_buffer_address = buffer_address,
    .mvp = mvp_matrix
};

SituationCmdSetPushConstant(cmd, 0, &pc, sizeof(pc));
SituationCmdDraw(cmd, vertex_count, 1, 0, 0);
```

**Notes:**
- Requires Vulkan backend with `VK_KHR_buffer_device_address` extension
- Returns 0 on OpenGL or if extension is not available
- Enables bindless rendering patterns for high-performance applications
- Buffer must remain valid while the address is in use
- Part of modern GPU-driven rendering techniques

---
#### `SituationUpdateBuffer`
Updates the contents of a GPU buffer with new data from the CPU. This is the primary way to send dynamic data (uniforms, vertex data, etc.) to the GPU each frame.

```c
SituationError SituationUpdateBuffer(SituationBuffer buffer, const void* data, size_t size);
```

**Parameters:**
- `buffer` - The buffer to update
- `data` - Pointer to the source data on CPU
- `size` - Number of bytes to copy

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Update camera uniform buffer each frame
typedef struct {
    mat4 view;
    mat4 projection;
    vec3 camera_pos;
} CameraUniforms;

CameraUniforms camera_data = {
    .view = view_matrix,
    .projection = proj_matrix,
    .camera_pos = camera_position
};

SituationUpdateBuffer(camera_ubo, &camera_data, sizeof(CameraUniforms));

// Update dynamic vertex buffer
Vertex vertices[MAX_PARTICLES];
GenerateParticleVertices(vertices, particle_count);
SituationUpdateBuffer(particle_vbo, vertices, particle_count * sizeof(Vertex));
```

**Important Rules:**
```c
// ✅ CORRECT: Update BEFORE recording draw commands
SituationUpdateBuffer(uniform_buffer, &data, sizeof(data));
SituationCmdBindDescriptorSet(cmd, 0, uniform_buffer);
SituationCmdDrawMesh(cmd, mesh);

// ❌ WRONG: Update AFTER recording draw (causes race condition)
SituationCmdDrawMesh(cmd, mesh);
SituationUpdateBuffer(uniform_buffer, &data, sizeof(data));  // Too late!
```

**Notes:**
- Must be called BEFORE recording draw commands that use the buffer
- On Vulkan, uses a ring buffer for zero-copy updates
- On OpenGL, uses persistent mapped buffers
- Size must not exceed the buffer's original size
- For dynamic buffers, use `SITUATION_BUFFER_USAGE_DYNAMIC_UNIFORM` flag

---
#### Compute Shaders

---
#### `SituationCreateComputePipeline`
Creates a compute pipeline from a GLSL compute shader file. Compute pipelines are used for GPU-accelerated parallel processing tasks like particle systems, physics simulations, and image processing.

```c
SituationError SituationCreateComputePipeline(const char* compute_shader_path, SituationComputeLayoutType layout_type, SituationComputePipeline* out_pipeline);
```

**Parameters:**
- `compute_shader_path` - Path to the GLSL compute shader file (.comp or .glsl)
- `layout_type` - Descriptor set layout type (determines binding model)
- `out_pipeline` - Pointer to receive the created pipeline handle

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Create a compute pipeline for particle simulation
SituationComputePipeline particle_pipeline;
if (SituationCreateComputePipeline(
    "shaders/particles.comp",
    SIT_COMPUTE_LAYOUT_ONE_SSBO,
    &particle_pipeline) != SITUATION_SUCCESS) {
    printf("Failed to create compute pipeline\n");
    return;
}

// Use the pipeline
SituationCmdBindComputePipeline(cmd, particle_pipeline);
SituationCmdBindComputeBuffer(cmd, 0, particle_buffer);
SituationCmdDispatch(cmd, particle_count / 256, 1, 1);

// Clean up when done
SituationDestroyComputePipeline(&particle_pipeline);
```

**Notes:**
- Shader must have a `#version 450` directive and `layout(local_size_x = N) in;`
- Layout type determines how resources are bound (standard vs bindless)
- Pipeline compilation happens at creation time
- Check for errors - shader compilation can fail

---
#### `SituationDestroyComputePipeline`
Destroys a compute pipeline and frees all associated GPU resources. The pipeline handle's ID is set to 0 after destruction.

```c
void SituationDestroyComputePipeline(SituationComputePipeline* pipeline);
```

**Parameters:**
- `pipeline` - Pointer to the compute pipeline to destroy (ID will be set to 0)

**Usage Example:**
```c
// Create pipeline
SituationComputePipeline blur_pipeline;
SituationCreateComputePipeline("shaders/blur.comp",
    SIT_COMPUTE_LAYOUT_ONE_SSBO, &blur_pipeline);

// Use pipeline...

// Clean up
SituationDestroyComputePipeline(&blur_pipeline);
// blur_pipeline.id is now 0
```

**Notes:**
- Safe to call multiple times (checks for valid ID)
- Waits for GPU to finish using the pipeline before destroying
- Always destroy pipelines before shutting down
- The pipeline pointer's ID field is set to 0 to prevent use-after-free

---
### Compute command recording

See **[situation_command_reference.md §7](../situation_command_reference.md#7-compute)** for `SituationCmdBindComputePipeline`, `BindComputeTexture`, `Dispatch`, and `PipelineBarrier`.

---
#### Virtual Displays

**User guide:** **[Virtual Display Module](virtual_display.md)** — architecture, scaling, blend modes, patterns, and troubleshooting. For **2D cell playfields** written by compute (`SituationGridStackPresent`), see **[2D Grid Module](grid.md)**. This section documents individual API symbols.

---
#### `SituationVirtualDisplay`
Public runtime state for one virtual display. GPU handles live in an **opaque** `backend[]` blob — do not interpret `backend` as a stable ABI.

**Since v2.4.316 (VD-1):** Vulkan VDs use **dynamic rendering** — there is **no** per-VD `VkRenderPass` / `VkFramebuffer` in public state. **Since v2.4.387 (VD bolster):** `composite_sampler`, `color_mip_levels`, `update_mode`, and `memory_hint` are first-class fields.

```c
typedef struct SituationVirtualDisplay {
    int      id;
    Vector2  resolution;
    Vector2  offset;
    float    opacity;
    bool     visible;
    int      z_order;

    /* Independent VD frame clock (see SituationUpdateTimers) */
    uint64_t frame_count;
    double   frame_time_multiplier;
    double   elapsed_time_seconds;
    float    cycle_animation_value;
    double   last_update_time_seconds;
    double   frame_delta_time_seconds;

    /* Content-update / idle compositor */
    double   last_content_update_time;
    uint64_t last_content_update_frame;
    double   idle_threshold_seconds;
    SituationVDFallbackMode fallback_mode;
    ColorRGBA fallback_color;
    SitVdStandbyConfig standby_pattern;

    bool                 is_dirty;
    SituationScalingMode scaling_mode;   /* layout only since v2.4.387 */
    SituationBlendMode   blend_mode;
    SituationVDFlags     flags;
    int                  texture_slot_index;  /* compute-target registry slot; -1 if N/A */

    /* VD-1 attachment */
    SituationVirtualDisplayColorFormat      color_format;
    SituationVirtualDisplayDepthStencilMode depth_stencil_mode;
    SituationVirtualDisplayAttachmentDefaults attachment_defaults;

    /* VD-3 / VD-4a */
    uint32_t                         color_mip_levels;
    SituationVirtualDisplaySamplerDesc composite_sampler;

    /* VD-5 */
    SituationVirtualDisplayUpdateMode update_mode;
    SituationVirtualDisplayMemoryHint   memory_hint;

    alignas(8) uint64_t backend[SIT_VD_BACKEND_STORAGE_U64_COUNT];  /* opaque GL/VK blob */
} SituationVirtualDisplay;
```

**Field notes:**

- `visible` — included in **`SituationRenderVirtualDisplays`** when true and `opacity > 0`.
- `is_dirty` — manual redraw hint; pairs with **`SIT_VD_UPDATE_STATIC`** (frozen frame clock).
- `scaling_mode` — compositor **layout rect only**; filtering is **`composite_sampler`**.
- `texture_slot_index` — for **`SITUATION_VD_FLAG_COMPUTE_TARGET`**; use **`SituationGetVirtualDisplayTexture`** for a bindable handle.
- `backend[]` — internal FBO/texture/sampler/descriptor storage; typed only inside the library (`SitVirtualDisplayGlBackend` / `SitVirtualDisplayVkBackend`).

Full struct definition: **`sit/situation_api_types_gpu.h`**. User guide: **[Virtual Display Module](virtual_display.md)**.

---

#### `SituationVDFlags`
Flags that modify virtual display creation behavior. Passed to `SituationCreateVirtualDisplayEx`.

```c
typedef enum {
    SITUATION_VD_FLAG_NONE           = 0,
    SITUATION_VD_FLAG_COMPUTE_TARGET = 1 << 0,
} SituationVDFlags;
```

| Flag | Description |
| :--- | :--- |
| `SITUATION_VD_FLAG_NONE` | Default behavior — creates a standard VD with color + depth attachments and a render pass. |
| `SITUATION_VD_FLAG_COMPUTE_TARGET` | The VD texture is writable by compute shaders. Adds `STORAGE` usage flags to the underlying image. Skips depth buffer and render pass creation (compute shaders write directly via image store). The VD texture can be retrieved with `SituationGetVirtualDisplayTexture()` and bound to compute pipelines. |

---

#### `SituationCreateVirtualDisplay`
Creates an off-screen render target (virtual display) that can be rendered to independently and then composited onto the main window or other displays. Virtual displays are essential for multi-pass rendering, post-processing effects, and UI layering.

```c
SituationError SituationCreateVirtualDisplay(Vector2 resolution, double frame_time_mult, int z_order, SituationScalingMode scaling_mode, SituationBlendMode blend_mode, int* out_id);
```

**Parameters:**
- `resolution` - Width and height of the virtual display in pixels
- `frame_time_mult` - Time multiplier for animations (1.0 = normal speed, 0.5 = half speed, 2.0 = double speed)
- `z_order` - Rendering order (lower values render first, higher values on top)
- `scaling_mode` - How to scale when compositing: `SITUATION_SCALING_FIT`, `SITUATION_SCALING_FILL`, `SITUATION_SCALING_STRETCH`
- `blend_mode` - Blending mode: `SITUATION_BLEND_ALPHA`, `SITUATION_BLEND_ADDITIVE`, `SITUATION_BLEND_MULTIPLY`
- `out_id` - Pointer to receive the virtual display ID

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Create a low-res virtual display for 3D scene (performance optimization)
int scene_display;
SituationCreateVirtualDisplay(
    (Vector2){1280, 720},  // Half of 4K resolution
    1.0,                    // Normal time
    0,                      // Render first (background)
    SITUATION_SCALING_FIT,  // Maintain aspect ratio
    SITUATION_BLEND_ALPHA,  // Standard alpha blending
    &scene_display
);

// Create a full-res virtual display for UI (sharp text)
int ui_display;
SituationCreateVirtualDisplay(
    (Vector2){2560, 1440},  // Full 4K resolution
    1.0,
    10,                     // Render on top
    SITUATION_SCALING_STRETCH,
    SITUATION_BLEND_ALPHA,
    &ui_display
);

// Create a slow-motion effect display
int slowmo_display;
SituationCreateVirtualDisplay(
    (Vector2){1920, 1080},
    0.25,                   // 1/4 speed for slow-motion
    5,
    SITUATION_SCALING_FIT,
    SITUATION_BLEND_ALPHA,
    &slowmo_display
);
```

**Notes:**
- Virtual displays are automatically composited in z_order during `SituationRenderVirtualDisplays()`
- Each virtual display has its own color and depth buffers
- Use lower resolutions for performance-critical 3D scenes
- Frame time multiplier affects time-based animations within that display
- Virtual displays can be used as textures via `SituationGetVirtualDisplayTexture()`

---
#### `SituationCreateVirtualDisplayEx`
Extended version of `SituationCreateVirtualDisplay` that accepts additional creation flags. Use this when you need a virtual display with special capabilities, such as compute shader writability.

```c
SituationError SituationCreateVirtualDisplayEx(Vector2 resolution, double frame_time_mult, int z_order, SituationScalingMode scaling_mode, SituationBlendMode blend_mode, SituationVDFlags flags, int* out_id);
```

**Parameters:**
- `resolution` - Width and height of the virtual display in pixels
- `frame_time_mult` - Time multiplier for animations (1.0 = normal speed, 0.5 = half speed, 2.0 = double speed)
- `z_order` - Rendering order (lower values render first, higher values on top)
- `scaling_mode` - How to scale when compositing: `SITUATION_SCALING_FIT`, `SITUATION_SCALING_FILL`, `SITUATION_SCALING_STRETCH`
- `blend_mode` - Blending mode: `SITUATION_BLEND_ALPHA`, `SITUATION_BLEND_ADDITIVE`, `SITUATION_BLEND_MULTIPLY`
- `flags` - Bitfield of `SituationVDFlags` controlling creation behavior
- `out_id` - Pointer to receive the virtual display ID

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Create a compute-target VD for GPU-driven effects (e.g., particle simulation output)
int compute_vd;
SituationCreateVirtualDisplayEx(
    (Vector2){1920, 1080},
    1.0,
    0,
    SITUATION_SCALING_FIT,
    SITUATION_BLEND_ALPHA,
    SITUATION_VD_FLAG_COMPUTE_TARGET,
    &compute_vd
);

// Get the texture handle to bind in compute shaders
SituationTexture vd_tex;
SituationGetVirtualDisplayTexture(compute_vd, &vd_tex);

// Bind to compute pipeline and dispatch
SituationCmdBindComputeTexture(cmd, 0, vd_tex);
SituationCmdDispatch(cmd, 1920 / 16, 1080 / 16, 1);
```

**Notes:**
- `SITUATION_VD_FLAG_COMPUTE_TARGET` skips depth buffer and render pass creation — the VD cannot be used as a render pass target (no `SituationCmdBeginRenderPass` with its ID). Write to it exclusively via compute shaders using `imageStore`.
- Compute-target VDs still participate in compositing via `SituationRenderVirtualDisplays()` if visible.
- Passing `SITUATION_VD_FLAG_NONE` is equivalent to calling `SituationCreateVirtualDisplay`.

---
#### `SituationDestroyVirtualDisplay`
Destroys a virtual display and frees all associated GPU resources (framebuffers, textures, depth buffers). The display ID is set to -1 after destruction.

```c
SituationError SituationDestroyVirtualDisplay(int* display_id);
```

**Parameters:**
- `display_id` - Pointer to the virtual display ID to destroy (will be set to -1)

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Create temporary virtual display for screenshot
int screenshot_display;
SituationCreateVirtualDisplay((Vector2){1920, 1080}, 1.0, 0,
    SITUATION_SCALING_FIT, SITUATION_BLEND_ALPHA, &screenshot_display);

// Render scene to it
SituationRenderPassInfo pass = { .display_id = screenshot_display };
SituationCmdBeginRenderPass(cmd, &pass);
RenderScene();
SituationCmdEndRenderPass(cmd);

// Save screenshot
SituationTexture screenshot_tex = SituationGetVirtualDisplayTexture(screenshot_display);
SituationImage img = SituationGetImageFromTexture(screenshot_tex);
SituationSaveImage(img, "screenshot.png");
SituationUnloadImage(img);

// Clean up
SituationDestroyVirtualDisplay(&screenshot_display);
// screenshot_display is now -1
```

**Notes:**
- Always destroy virtual displays when no longer needed to free GPU memory
- The display ID pointer is set to -1 to prevent use-after-free
- Destroying a display invalidates any texture handles obtained from it
- Safe to call multiple times (checks for valid ID)

---
#### `SituationSetVirtualDisplayVisible`
Controls whether a virtual display should be composited onto the render target during `SituationRenderVirtualDisplays()`. Hidden displays are still rendered to but not displayed.

```c
void SituationSetVirtualDisplayVisible(int display_id, bool visible);
```

**Parameters:**
- `display_id` - The virtual display ID
- `visible` - `true` to show during compositing, `false` to hide

**Usage Example:**
```c
// Create displays for different game states
int gameplay_display, menu_display, pause_display;
SituationCreateVirtualDisplay((Vector2){1920, 1080}, 1.0, 0,
    SITUATION_SCALING_FIT, SITUATION_BLEND_ALPHA, &gameplay_display);
SituationCreateVirtualDisplay((Vector2){1920, 1080}, 1.0, 5,
    SITUATION_SCALING_FIT, SITUATION_BLEND_ALPHA, &menu_display);
SituationCreateVirtualDisplay((Vector2){1920, 1080}, 1.0, 10,
    SITUATION_SCALING_FIT, SITUATION_BLEND_ALPHA, &pause_display);

// Game state management
switch (game_state) {
    case STATE_GAMEPLAY:
        SituationSetVirtualDisplayVisible(gameplay_display, true);
        SituationSetVirtualDisplayVisible(menu_display, false);
        SituationSetVirtualDisplayVisible(pause_display, false);
        break;
    case STATE_PAUSED:
        SituationSetVirtualDisplayVisible(gameplay_display, true);  // Show dimmed
        SituationSetVirtualDisplayVisible(menu_display, false);
        SituationSetVirtualDisplayVisible(pause_display, true);     // Show pause menu
        break;
    case STATE_MENU:
        SituationSetVirtualDisplayVisible(gameplay_display, false);
        SituationSetVirtualDisplayVisible(menu_display, true);
        SituationSetVirtualDisplayVisible(pause_display, false);
        break;
}
```

**Notes:**
- Hidden displays are still rendered to (useful for keeping state)
- Use this for UI layer management and game state transitions
- More efficient than destroying/recreating displays
- Visibility only affects compositing, not rendering to the display

---
#### `SituationGetVirtualDisplayTexture`
Gets a handle to the underlying color buffer texture of a virtual display. This allows you to use the rendered output as an input texture for post-processing, UI elements, or other rendering passes.

```c
SituationTexture SituationGetVirtualDisplayTexture(int display_id);
```

**Parameters:**
- `display_id` - The virtual display ID

**Returns:** Texture handle for the virtual display's color buffer

**Usage Example:**
```c
// Create a virtual display for 3D scene
int scene_display;
SituationCreateVirtualDisplay((Vector2){1920, 1080}, 1.0, 0,
    SITUATION_SCALING_FIT, SITUATION_BLEND_ALPHA, &scene_display);

// Render 3D scene to virtual display
SituationRenderPassInfo scene_pass = { .display_id = scene_display };
SituationCmdBeginRenderPass(cmd, &scene_pass);
Render3DScene();
SituationCmdEndRenderPass(cmd);

// Get the rendered scene as a texture
SituationTexture scene_texture = SituationGetVirtualDisplayTexture(scene_display);

// Apply post-processing effects
SituationRenderPassInfo post_pass = { .display_id = -1 };
SituationCmdBeginRenderPass(cmd, &post_pass);
SituationCmdBindPipeline(cmd, blur_shader);
SituationCmdBindShaderTexture(cmd, 0, scene_texture);
SituationCmdDrawQuad(cmd);
SituationCmdEndRenderPass(cmd);

// Or use as a texture on a 3D object (security camera feed, portal, etc.)
SituationCmdBindShaderTexture(cmd, 0, scene_texture);
SituationCmdDrawMesh(cmd, tv_screen_mesh);
```

**Notes:**
- The texture is valid as long as the virtual display exists
- Texture is automatically updated each frame when you render to the display
- Useful for post-processing chains, mirrors, portals, and security camera effects
- The texture handle becomes invalid after `SituationDestroyVirtualDisplay()`

---
#### `SituationRenderVirtualDisplays`
Composites all visible virtual displays onto the current render target.
```c
SituationError SituationRenderVirtualDisplays(SituationCommandBuffer cmd);
```
**Usage Example:**
```c
// At init: Create a display for the 3D scene
int scene_vd;
SituationCreateVirtualDisplay((Vector2){640, 360}, 1.0, 0, SITUATION_SCALING_FIT, SITUATION_BLEND_ALPHA, &scene_vd);

// In render loop:
// 1. Render scene to the virtual display
SituationRenderPassInfo scene_pass = { .display_id = scene_vd };
SituationCmdBeginRenderPass(cmd, &scene_pass);
// ... draw 3D models ...
SituationCmdEndRenderPass(cmd);

// 2. Render to the main window
SituationRenderPassInfo final_pass = { .display_id = -1 };
SituationCmdBeginRenderPass(cmd, &final_pass);
// This composites the 3D scene from its virtual display onto the main window
SituationRenderVirtualDisplays(cmd);
// ... draw UI on top ...
SituationCmdEndRenderPass(cmd);
```

---
#### Legacy Shader Uniforms
---
#### `SituationGetShaderLocation`
Gets the location of a uniform variable in a shader by name.
```c
int SituationGetShaderLocation(SituationShader shader, const char* uniformName);
```
**Usage Example:**
```c
// Get the location of the "u_time" uniform in the shader.
int time_uniform_loc = SituationGetShaderLocation(my_shader, "u_time");
// This location can then be used with SituationSetShaderValue.
```

---
#### `SituationGetShaderLocationAttrib`
Gets the location of a vertex attribute in a shader by name.
```c
int SituationGetShaderLocationAttrib(SituationShader shader, const char* attribName);
```
**Usage Example:**
```c
// Get the location of the "a_color" vertex attribute.
int color_attrib_loc = SituationGetShaderLocationAttrib(my_shader, "a_color");
// This is useful for advanced, custom vertex buffer layouts.
```

---
#### `SituationSetShaderValue`
Sets a uniform value in a shader.
```c
void SituationSetShaderValue(SituationShader shader, int locIndex, const void* value, int uniformType);
```
**Usage Example:**
```c
int time_loc = SituationGetShaderLocation(my_shader, "u_time");
float current_time = (float)SituationGetTime();
// Note: This is a legacy way to set uniforms. Using UBOs with SituationCmdBindShaderBuffer is preferred.
SituationSetShaderValue(my_shader, time_loc, &current_time, SIT_UNIFORM_FLOAT);
```

---
#### `SituationSetShaderValueMatrix`
Sets a matrix uniform value in a shader.
```c
void SituationSetShaderValueMatrix(SituationShader shader, int locIndex, mat4 mat);
```
**Usage Example:**
```c
int mvp_loc = SituationGetShaderLocation(my_shader, "u_mvp");
mat4 mvp_matrix = /* ... calculate matrix ... */;
SituationSetShaderValueMatrix(my_shader, mvp_loc, mvp_matrix);
```

---
#### `SituationSetShaderValueTexture`
Sets a texture uniform value in a shader.
```c
void SituationSetShaderValueTexture(SituationShader shader, int locIndex, SituationTexture texture);
```
**Usage Example:**
```c
int albedo_loc = SituationGetShaderLocation(my_shader, "u_albedo_texture");
// This tells the shader to use my_texture for the texture sampler at `albedo_loc`.
SituationSetShaderValueTexture(my_shader, albedo_loc, my_texture);
```

---
#### Miscellaneous

---
#### `SituationLoadImageFromScreen`
Captures the current contents of the main window's backbuffer into a CPU-side image.
```c
SituationError SituationLoadImageFromScreen(SituationImage* out_image);
```
**Usage Example:**
```c
if (SituationIsKeyPressed(SIT_KEY_F12)) {
    SituationImage screenshot = {0};
    if (SituationLoadImageFromScreen(&screenshot) == SITUATION_SUCCESS) {
        SituationExportImage(screenshot, "screenshot.png");
        SituationUnloadImage(screenshot);
    }
}
```

---
### Deprecated `SituationCmd*` (migration only)

See **[situation_command_reference.md §11](../situation_command_reference.md#11-deprecated-commands)**. Prefer `SituationCmdBeginRenderPass` / `EndRenderPass`, `BindDescriptorSet`, `BindTextureSet`, `SituationCmdPipelineBarrier` over legacy names.

---
#### `SituationLoadShaderFromMemory`
Creates a graphics shader pipeline from in-memory GLSL source code. This is useful for procedurally generated shaders, embedded shaders, or runtime shader compilation.

```c
SituationError SituationLoadShaderFromMemory(const char* vs_code, const char* fs_code, SituationShader* out_shader);
```

**Parameters:**
- `vs_code` - Null-terminated string containing vertex shader GLSL source
- `fs_code` - Null-terminated string containing fragment shader GLSL source
- `out_shader` - Pointer to receive the created shader handle

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Embed shaders directly in code
const char* vertex_shader = 
    "#version 450\n"
    "layout(location = 0) in vec3 a_position;\n"
    "layout(location = 1) in vec2 a_texcoord;\n"
    "layout(location = 0) out vec2 v_texcoord;\n"
    "layout(push_constant) uniform PC { mat4 mvp; };\n"
    "void main() {\n"
    "    gl_Position = mvp * vec4(a_position, 1.0);\n"
    "    v_texcoord = a_texcoord;\n"
    "}\n";

const char* fragment_shader =
    "#version 450\n"
    "layout(location = 0) in vec2 v_texcoord;\n"
    "layout(location = 0) out vec4 frag_color;\n"
    "layout(binding = 0) uniform sampler2D u_texture;\n"
    "void main() {\n"
    "    frag_color = texture(u_texture, v_texcoord);\n"
    "}\n";

SituationShader shader;
if (SituationLoadShaderFromMemory(vertex_shader, fragment_shader, &shader) != SITUATION_SUCCESS) {
    printf("Failed to compile shader\n");
    return;
}

// Use the shader
SituationCmdBindPipeline(cmd, shader);
SituationCmdDrawMesh(cmd, mesh);

// Clean up
SituationUnloadShader(shader);
```

**Advanced Example (Procedural Shader Generation):**
```c
// Generate shader with configurable parameters
char fs_code[4096];
snprintf(fs_code, sizeof(fs_code),
    "#version 450\n"
    "layout(location = 0) in vec2 v_texcoord;\n"
    "layout(location = 0) out vec4 frag_color;\n"
    "layout(binding = 0) uniform sampler2D u_texture;\n"
    "void main() {\n"
    "    vec4 color = texture(u_texture, v_texcoord);\n"
    "    color.rgb *= vec3(%.2f, %.2f, %.2f);\n"  // Tint color
    "    frag_color = color;\n"
    "}\n",
    tint_r, tint_g, tint_b);

SituationShader tinted_shader;
SituationLoadShaderFromMemory(standard_vs, fs_code, &tinted_shader);
```

**Notes:**
- Shader source must be complete, valid GLSL
- Compilation errors are logged to the console
- Useful for shader hot-reloading during development
- Can embed shaders to avoid external file dependencies
- Performance is identical to file-based shaders after compilation

---
#### `SituationBeginLoadShaderFromMemory`
Starts a **non-blocking** graphics shader load from in-memory GLSL. The output handle is valid immediately, but the pipeline is not ready until **`SituationPollShaderLoad`** returns **`SITUATION_SUCCESS`**.

**Backend behavior:**

| Backend | Work performed asynchronously |
|---------|------------------------------|
| **OpenGL** | Async compile/link (`KHR_parallel_shader_compile` where available) |
| **Vulkan** | GLSL→SPIR-V via shaderc on the **high-priority worker queue** (v2.4.105+), then pipeline build across subsequent frames |

```c
SituationError SituationBeginLoadShaderFromMemory(const char* vs_code, const char* fs_code, SituationShader* out_shader);
```

**Parameters:** Same as **`SituationLoadShaderFromMemory`**.

**Returns:**
- **`SITUATION_SUCCESS`** — Load started; poll until ready.
- Immediate errors (bad source, out of memory, not initialized) — do not poll.

**Usage Example:**
```c
SituationShader shader = {0};
SituationError err = SituationBeginLoadShaderFromMemory(vs_src, fs_src, &shader);
if (err != SITUATION_SUCCESS && err != SITUATION_ERROR_SHADER_LOAD_IN_PROGRESS) {
    fprintf(stderr, "Begin load failed: %s\n", SituationErrorToString(err));
    return;
}

while (SituationWindowShouldClose() == false) {
    SituationPollInputEvents();
    SituationUpdateTimers();

    err = SituationPollShaderLoad(shader);
    if (err == SITUATION_ERROR_SHADER_LOAD_IN_PROGRESS) {
        DrawLoadingSpinner(); /* optional progress UI */
    } else if (err == SITUATION_SUCCESS) {
        break;
    } else {
        char* detail = NULL;
        SituationGetLastErrorMsg(&detail);
        fprintf(stderr, "Shader failed: %s\n", detail ? detail : SituationErrorToString(err));
        if (detail) SituationFreeString(detail);
        return;
    }

  /* ... rest of frame ... */
}

SituationCmdBindPipeline(cmd, shader);
```

**Notes:**
- Call **`SituationPollShaderLoad`** once per frame on the **main thread** (same thread as **`SituationPollInputEvents`**).
- Do not **`SituationUnloadShader`** while poll returns **`IN_PROGRESS`** — unload waits for the worker (Vulkan).
- Blocking equivalent: **`SituationLoadShaderFromMemory`**.

---
#### `SituationPollShaderLoad`
Advances an async shader load started by **`SituationBeginLoadShaderFromMemory`** or **`SituationBeginLoadShaderFromSpirvMemory(Ex)`**.

```c
SituationError SituationPollShaderLoad(SituationShader shader);
```

**Returns:**

| Code | Meaning |
|------|---------|
| **`SITUATION_SUCCESS`** | Pipeline ready — safe to bind and draw |
| **`SITUATION_ERROR_SHADER_LOAD_IN_PROGRESS`** | Still compiling/linking/building — call again next frame |
| Other **`SituationError`** | Hard failure — check **`SituationGetLastErrorMsg`** |

**Usage:** See **`SituationBeginLoadShaderFromMemory`** example above.

---
#### `SituationSpirvLayoutProfile` (enum)
Selects the **Vulkan** descriptor set layout when loading user SPIR-V that uses non-default UBO/SSBO bindings. Ignored on **OpenGL** (profile treated as mesh/default).

```c
typedef enum SituationSpirvLayoutProfile {
    SIT_SPIRV_LAYOUT_PROFILE_MESH = 0,          /* Set 0: dynamic UBO; set 1: combined sampler (default) */
    SIT_SPIRV_LAYOUT_PROFILE_DUAL_SSBO,         /* Set 0 + set 1: SSBO @ binding 0 each */
    SIT_SPIRV_LAYOUT_PROFILE_UBO_SSBO,          /* Set 0: UBO; set 1: SSBO @ binding 0 */
    SIT_SPIRV_LAYOUT_PROFILE_UBO_SSBO_SAMPLER,  /* Set 0: UBO; set 1: SSBO; set 2: combined image sampler (fragment) */
} SituationSpirvLayoutProfile;
```

Use **`SIT_SPIRV_LAYOUT_PROFILE_UBO_SSBO`** for large production shaders (e.g. Demon Hunt sky) that exceed default mesh layout assumptions. Use **`SIT_SPIRV_LAYOUT_PROFILE_UBO_SSBO_SAMPLER`** when the shader additionally needs a texture sampler at descriptor set 2 (e.g. frame feedback effects). Harness regression: **`sit_test_vulkan.exe --module graphics --filter spirv`**.

---
#### `SituationLoadShaderFromSpirv`
Loads a graphics pipeline from **precompiled `.spv` files** on disk. **Blocking** — returns when the pipeline is ready.

- **OpenGL:** **`GL_ARB_gl_spirv`** path when available.
- **Vulkan:** Creates shader modules and graphics pipeline directly (no shaderc).

```c
SituationError SituationLoadShaderFromSpirv(const char* vs_spv_path, const char* fs_spv_path, SituationShader* out_shader);
```

**Hot-reload:** File paths are stored; **`SituationReloadShader`** can refresh from disk where supported.

**Usage Example:**
```c
SituationShader shader;
if (SituationLoadShaderFromSpirv("shaders/mesh.vert.spv", "shaders/mesh.frag.spv", &shader) != SITUATION_SUCCESS) {
    char* msg = NULL;
    SituationGetLastErrorMsg(&msg);
    fprintf(stderr, "SPIR-V load: %s\n", msg ? msg : "unknown");
    if (msg) SituationFreeString(msg);
}
```

---
#### `SituationLoadShaderFromSpirvMemory`
Same as **`SituationLoadShaderFromSpirv`**, but vertex and fragment SPIR-V bytecode are provided as memory buffers (embedded assets, harness precompiles, tools). **Blocking.**

```c
SituationError SituationLoadShaderFromSpirvMemory(
    const void* vs_spirv, size_t vs_len,
    const void* fs_spirv, size_t fs_len,
    SituationShader* out_shader);
```

**Notes:**
- No hot-reload file paths — bytecode is copied internally.
- Default Vulkan layout: **`SIT_SPIRV_LAYOUT_PROFILE_MESH`**.
- For custom descriptor layouts, use **`SituationLoadShaderFromSpirvMemoryEx`**.

---
#### `SituationLoadShaderFromSpirvMemoryEx`
Blocking SPIR-V load with an explicit **`SituationSpirvLayoutProfile`** (**Vulkan**). **OpenGL:** profile ignored; behavior matches **`SituationLoadShaderFromSpirvMemory`**.

```c
SituationError SituationLoadShaderFromSpirvMemoryEx(
    const void* vs_spirv, size_t vs_len,
    const void* fs_spirv, size_t fs_len,
    SituationSpirvLayoutProfile layout_profile,
    SituationShader* out_shader);
```

**Usage Example:**
```c
extern const uint8_t demon_hunt_vs_spv[];
extern const uint8_t demon_hunt_fs_spv[];
extern const size_t demon_hunt_vs_spv_len, demon_hunt_fs_spv_len;

SituationShader sky;
SituationError err = SituationLoadShaderFromSpirvMemoryEx(
    demon_hunt_vs_spv, demon_hunt_vs_spv_len,
    demon_hunt_fs_spv, demon_hunt_fs_spv_len,
    SIT_SPIRV_LAYOUT_PROFILE_UBO_SSBO,
    &sky);
```

---
#### `SituationBeginLoadShaderFromSpirvMemory`
Non-blocking SPIR-V pipeline build (**Vulkan**). **OpenGL:** falls back to **blocking** load (same as sync API).

```c
SituationError SituationBeginLoadShaderFromSpirvMemory(
    const void* vs_spirv, size_t vs_len,
    const void* fs_spirv, size_t fs_len,
    SituationShader* out_shader);
```

Bytecode is **copied** at kickoff. Poll with **`SituationPollShaderLoad`**. Default layout profile: **mesh**.

---
#### `SituationBeginLoadShaderFromSpirvMemoryEx`
Non-blocking SPIR-V load with **`SituationSpirvLayoutProfile`** (**Vulkan**). **OpenGL:** profile ignored; blocking equivalent of **`SituationBeginLoadShaderFromSpirvMemory`**.

```c
SituationError SituationBeginLoadShaderFromSpirvMemoryEx(
    const void* vs_spirv, size_t vs_len,
    const void* fs_spirv, size_t fs_len,
    SituationSpirvLayoutProfile layout_profile,
    SituationShader* out_shader);
```

**Typical pattern:** Begin → loop with **`SituationPollShaderLoad`** → bind when **`SITUATION_SUCCESS`**. See **`doc/TEST_SPIRV_SHADER_API.md`** for harness and offline debug workflow.

**Notes:**
- Prefer precompiled SPIR-V in shipping builds to avoid shaderc dependency at runtime.
- Async GLSL on Vulkan requires worker thread pool (**`SituationInit`** with threading enabled — default for shared DLL builds).

---
#### `SituationSetShaderUniform`
**[OpenGL Only]** Sets a uniform variable value by name. This is a legacy API that uses a cache to avoid redundant `glGetUniformLocation` calls.

```c
SituationError SituationSetShaderUniform(SituationShader shader, const char* uniform_name, const void* data, SituationUniformType type);
```

**Parameters:**
- `shader` - The shader to set the uniform in
- `uniform_name` - Name of the uniform variable (e.g., "u_time")
- `data` - Pointer to the uniform data
- `type` - Type of the uniform (e.g., `SITUATION_UNIFORM_FLOAT`, `SITUATION_UNIFORM_VEC3`, `SITUATION_UNIFORM_MAT4`)

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Set various uniform types
float time = SituationGetTime();
SituationSetShaderUniform(shader, "u_time", &time, SITUATION_UNIFORM_FLOAT);

vec3 light_pos = {10.0f, 20.0f, 5.0f};
SituationSetShaderUniform(shader, "u_light_position", &light_pos, SITUATION_UNIFORM_VEC3);

mat4 model_matrix;
glm_mat4_identity(model_matrix);
SituationSetShaderUniform(shader, "u_model", model_matrix, SITUATION_UNIFORM_MAT4);

vec4 tint_color = {1.0f, 0.5f, 0.5f, 1.0f};
SituationSetShaderUniform(shader, "u_tint", &tint_color, SITUATION_UNIFORM_VEC4);
```

**Notes:**
- **OpenGL Only** - Not available on Vulkan backend
- For Vulkan, use uniform buffers with `SituationUpdateBuffer()` instead
- Uses an internal cache to avoid repeated `glGetUniformLocation` calls
- Less efficient than uniform buffers for frequently-changing data
- Recommended to migrate to uniform buffers for cross-platform code

---
#### `SituationCreateComputePipeline`
Creates a compute pipeline from a GLSL compute shader file. Compute pipelines are used for GPU-accelerated parallel processing.

```c
SituationError SituationCreateComputePipeline(const char* compute_shader_path, SituationComputeLayoutType layout_type, SituationComputePipeline* out_pipeline);
```

**Parameters:**
- `compute_shader_path` - Path to GLSL compute shader file (.comp)
- `layout_type` - Descriptor set layout (e.g., `SIT_COMPUTE_LAYOUT_ONE_SSBO`)
- `out_pipeline` - Pointer to receive the created pipeline handle

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Create compute pipeline for particle simulation
SituationComputePipeline particle_pipeline;
if (SituationCreateComputePipeline(
    "shaders/particles.comp",
    SIT_COMPUTE_LAYOUT_ONE_SSBO,
    &particle_pipeline) != SITUATION_SUCCESS) {
    printf("Failed to create compute pipeline\n");
    return;
}

// Use the pipeline
SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
SituationCmdBindComputePipeline(cmd, particle_pipeline);
SituationCmdBindComputeBuffer(cmd, 0, particle_buffer);
SituationCmdDispatch(cmd, particle_count / 256, 1, 1);

// Cleanup
SituationDestroyComputePipeline(particle_pipeline);

// Image processing pipeline
SituationComputePipeline blur_pipeline;
SituationCreateComputePipeline(
    "shaders/blur.comp",
    SIT_COMPUTE_LAYOUT_IMAGE_AND_SSBO,
    &blur_pipeline);

// Bind and dispatch
SituationCmdBindComputePipeline(cmd, blur_pipeline);
SituationCmdBindComputeTexture(cmd, 0, input_texture);
SituationCmdBindComputeTexture(cmd, 1, output_texture);
SituationCmdDispatch(cmd, width / 16, height / 16, 1);
```

**Notes:**
- Shader must be GLSL compute shader (#version 450)
- Layout type must match shader's descriptor set layout
- Pipeline must be destroyed with `SituationDestroyComputePipeline()`
- Use `SituationCreateComputePipelineFromMemory()` for runtime generation

---
#### `SituationCreateComputePipelineFromMemory`
Creates a compute pipeline from in-memory GLSL source code. This is useful for procedurally generated shaders, embedded shaders, or runtime shader compilation.

```c
SituationError SituationCreateComputePipelineFromMemory(const char* compute_shader_source, SituationComputeLayoutType layout_type, SituationComputePipeline* out_pipeline);
```

**Parameters:**
- `compute_shader_source` - Null-terminated string containing GLSL compute shader source
- `layout_type` - Descriptor set layout type
- `out_pipeline` - Pointer to receive the created pipeline handle

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Procedurally generate a compute shader
const char* shader_source = 
    "#version 450\n"
    "layout(local_size_x = 256) in;\n"
    "layout(binding = 0) buffer Data { float values[]; };\n"
    "void main() {\n"
    "    uint id = gl_GlobalInvocationID.x;\n"
    "    values[id] *= 2.0;\n"
    "}\n";

SituationComputePipeline pipeline;
if (SituationCreateComputePipelineFromMemory(
    shader_source,
    SIT_COMPUTE_LAYOUT_ONE_SSBO,
    &pipeline) != SITUATION_SUCCESS) {
    printf("Failed to compile compute shader\n");
    return;
}

// Use the pipeline
SituationCmdBindComputePipeline(cmd, pipeline);
SituationCmdBindComputeBuffer(cmd, 0, data_buffer);
SituationCmdDispatch(cmd, data_count / 256, 1, 1);

SituationDestroyComputePipeline(&pipeline);
```

**Advanced Example (Runtime Shader Generation):**
```c
// Generate shader with configurable work group size
char shader_code[2048];
snprintf(shader_code, sizeof(shader_code),
    "#version 450\n"
    "layout(local_size_x = %d) in;\n"
    "layout(binding = 0) buffer Particles { vec4 positions[]; };\n"
    "layout(push_constant) uniform Constants { float dt; };\n"
    "void main() {\n"
    "    uint id = gl_GlobalInvocationID.x;\n"
    "    positions[id].y -= 9.8 * dt;\n"  // Gravity
    "}\n",
    optimal_work_group_size);

SituationComputePipeline gravity_pipeline;
SituationCreateComputePipelineFromMemory(shader_code,
    SIT_COMPUTE_LAYOUT_ONE_SSBO, &gravity_pipeline);
```

**Notes:**
- Shader source must be a complete, valid GLSL compute shader
- Compilation errors are logged to the console
- Useful for shader hot-reloading during development
- Can embed shaders directly in the executable
- Performance is identical to file-based pipelines after compilation

---
#### `SituationDestroyComputePipeline`
Destroys a compute pipeline and frees all associated GPU resources. Always call this when done with a compute pipeline.

```c
void SituationDestroyComputePipeline(SituationComputePipeline* pipeline);
```

**Parameters:**
- `pipeline` - Pointer to compute pipeline to destroy

**Usage Example:**
```c
// Create and use compute pipeline
SituationComputePipeline pipeline;
SituationCreateComputePipeline("shaders/compute.comp", SIT_COMPUTE_LAYOUT_ONE_SSBO, &pipeline);

// Use the pipeline
SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
SituationCmdBindComputePipeline(cmd, pipeline);
SituationCmdDispatch(cmd, 64, 1, 1);

// Cleanup when done
SituationDestroyComputePipeline(&pipeline);

// Proper resource management
SituationComputePipeline pipelines[3];

void LoadComputeShaders() {
    SituationCreateComputePipeline("shaders/particles.comp", SIT_COMPUTE_LAYOUT_ONE_SSBO, &pipelines[0]);
    SituationCreateComputePipeline("shaders/blur.comp",      SIT_COMPUTE_LAYOUT_IMAGE_AND_SSBO, &pipelines[1]);
    SituationCreateComputePipeline("shaders/physics.comp",   SIT_COMPUTE_LAYOUT_ONE_SSBO, &pipelines[2]);
}

void UnloadComputeShaders() {
    for (int i = 0; i < 3; i++) {
        SituationDestroyComputePipeline(&pipelines[i]);
    }
}

// Safe to call multiple times
SituationComputePipeline pipeline;
SituationCreateComputePipeline("test.comp", SIT_COMPUTE_LAYOUT_ONE_SSBO, &pipeline);
SituationDestroyComputePipeline(&pipeline);
// pipeline.id is now 0
```

**Notes:**
- Frees shader modules and pipeline state
- Sets pipeline ID to 0 after destruction
- Safe to call multiple times (idempotent)
- Always call before application exit
- Don't use pipeline after destruction

---
#### `SituationGetBufferData`
Reads data back from a GPU buffer to CPU memory. This is useful for debugging, readback of compute shader results, or retrieving GPU-generated data.

```c
SituationError SituationGetBufferData(SituationBuffer buffer, size_t offset, size_t size, void* out_data);
```

**Parameters:**
- `buffer` - The buffer to read from
- `offset` - Byte offset into the buffer to start reading
- `size` - Number of bytes to read
- `out_data` - Pointer to CPU memory to receive the data

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Run compute shader to process data
SituationCmdBindComputePipeline(cmd, process_pipeline);
SituationCmdBindComputeBuffer(cmd, 0, result_buffer);
SituationCmdDispatch(cmd, work_groups, 1, 1);

// Wait for GPU to finish
SituationEndFrame();
SituationWaitForGPU();

// Read results back to CPU
float results[1024];
if (SituationGetBufferData(result_buffer, 0, sizeof(results), results) == SITUATION_SUCCESS) {
    // Process results on CPU
    for (int i = 0; i < 1024; i++) {
        printf("Result[%d] = %.2f\n", i, results[i]);
    }
}
```

**Debugging Example:**
```c
// Verify uniform buffer contents
CameraUniforms camera_data_readback;
SituationGetBufferData(camera_ubo, 0, sizeof(CameraUniforms), &camera_data_readback);

printf("Camera position: %.2f, %.2f, %.2f\n",
    camera_data_readback.camera_pos[0],
    camera_data_readback.camera_pos[1],
    camera_data_readback.camera_pos[2]);
```

**Notes:**
- **Performance Warning:** GPU-to-CPU readback is SLOW - avoid in performance-critical code
- Causes a GPU pipeline stall - use sparingly
- Primarily for debugging and one-time data retrieval
- For frequent readback, consider using staging buffers
- Ensure GPU has finished writing to the buffer before reading

---
#### `SituationConfigureVirtualDisplay`
Reconfigures an existing virtual display's properties without destroying and recreating it. This allows dynamic adjustment of display behavior at runtime.

```c
SituationError SituationConfigureVirtualDisplay(int display_id, vec2 offset, float opacity, int z_order, bool visible, double frame_time_mult, SituationBlendMode blend_mode);
```

**Parameters:**
- `display_id` - The virtual display ID to configure
- `offset` - Position offset when compositing (in pixels)
- `opacity` - Overall opacity (0.0 = fully transparent, 1.0 = fully opaque)
- `z_order` - Rendering order (lower values render first)
- `visible` - Whether to composite this display
- `frame_time_mult` - Time multiplier for animations
- `blend_mode` - Blending mode for compositing

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Create a virtual display for damage flash effect
int damage_flash_display;
SituationCreateVirtualDisplay((Vector2){1920, 1080}, 1.0, 100,
    SITUATION_SCALING_STRETCH, SITUATION_BLEND_ADDITIVE, &damage_flash_display);

// Initially invisible
SituationConfigureVirtualDisplay(damage_flash_display, 
    (vec2){0, 0}, 0.0f, 100, false, 1.0, SITUATION_BLEND_ADDITIVE);

// When player takes damage, fade in red overlay
float damage_opacity = 0.8f;
SituationConfigureVirtualDisplay(damage_flash_display,
    (vec2){0, 0}, damage_opacity, 100, true, 1.0, SITUATION_BLEND_ADDITIVE);

// Fade out over time
for (float t = 0.8f; t > 0.0f; t -= delta_time * 2.0f) {
    SituationConfigureVirtualDisplay(damage_flash_display,
        (vec2){0, 0}, t, 100, true, 1.0, SITUATION_BLEND_ADDITIVE);
}

// Hide when done
SituationConfigureVirtualDisplay(damage_flash_display,
    (vec2){0, 0}, 0.0f, 100, false, 1.0, SITUATION_BLEND_ADDITIVE);
```

**Notes:**
- More efficient than destroying and recreating displays
- Useful for animated transitions, fades, and dynamic UI layers
- Opacity affects the entire display during compositing
- Offset allows for screen shake or parallax effects

---
#### `SituationGetVirtualDisplay`
Gets a pointer to the internal state structure of a virtual display. This provides direct access to display properties for advanced use cases.

```c
SituationVirtualDisplay* SituationGetVirtualDisplay(int display_id);
```

**Parameters:**
- `display_id` - The virtual display ID

**Returns:** Pointer to the virtual display structure, or NULL if invalid

**Usage Example:**
```c
// Query virtual display properties
SituationVirtualDisplay* vd = SituationGetVirtualDisplay(my_display_id);
if (vd != NULL) {
    printf("Display resolution: %dx%d\n", vd->width, vd->height);
    printf("Z-order: %d\n", vd->z_order);
    printf("Visible: %s\n", vd->visible ? "yes" : "no");
    printf("Frame time multiplier: %.2f\n", vd->frame_time_mult);
    
    // Access framebuffer handle for advanced operations
    printf("Framebuffer ID: %u\n", vd->framebuffer_id);
}
```

**Notes:**
- Returns NULL for invalid display IDs
- Provides read access to internal state
- Modifying the structure directly is not recommended - use configuration functions instead
- Useful for debugging and advanced rendering techniques

---
#### `SituationSetVirtualDisplayScalingMode`
Changes how a virtual display is **laid out** when composited onto the render target (fit / stretch / integer scale rect). **Since v2.4.387 this does not change min/mag/mip filter** — use **`SituationSetVirtualDisplaySampler`** for filtering.

```c
SituationError SituationSetVirtualDisplayScalingMode(int display_id, SituationScalingMode scaling_mode);
```

**Parameters:**
- `display_id` - The virtual display ID
- `scaling_mode` - Layout mode:
  - `SITUATION_SCALING_FIT` - Maintain aspect ratio, letterbox if needed
  - `SITUATION_SCALING_FILL` - Fill entire area, may crop edges
  - `SITUATION_SCALING_STRETCH` - Stretch to fill, may distort
  - `SITUATION_SCALING_INTEGER` - Nearest integer scale, centered

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Layout: pixel-perfect integer scale for game layer
SituationSetVirtualDisplayScalingMode(game_display, SITUATION_SCALING_INTEGER);

// Filter: explicit nearest when upscaling (v2.4.387+)
SituationVirtualDisplaySamplerDesc sampler = SituationVirtualDisplaySamplerDescDefault();
sampler.min_filter = SIT_FILTER_NEAREST;
sampler.mag_filter = SIT_FILTER_NEAREST;
SituationSetVirtualDisplaySampler(game_display, &sampler);
```

**Notes:**
- `FIT` / `INTEGER` preserve aspect ratio (letterbox/pillarbox as needed)
- `STRETCH` distorts to fill the layout rect
- Can be changed at runtime when the window resizes
- **Filter is independent** — see **`SituationSetVirtualDisplaySampler`**, **`SituationSetVirtualDisplayMaxAnisotropy`**, **`SituationSetVirtualDisplayMipLevels`**

---
#### `SituationGetVirtualDisplaySize`
Retrieves the internal resolution (width and height) of a virtual display's render target.

```c
void SituationGetVirtualDisplaySize(int display_id, int* width, int* height);
```

**Parameters:**
- `display_id` - The virtual display ID
- `width` - Pointer to receive the width in pixels
- `height` - Pointer to receive the height in pixels

**Usage Example:**
```c
// Query display size for dynamic rendering
int vd_width, vd_height;
SituationGetVirtualDisplaySize(my_display_id, &vd_width, &vd_height);

// Calculate aspect ratio
float aspect = (float)vd_width / (float)vd_height;
printf("Virtual display: %dx%d (aspect: %.2f)\n", vd_width, vd_height, aspect);

// Set viewport to match display size
SituationCmdSetViewport(cmd, 0, 0, vd_width, vd_height);

// Create projection matrix matching display resolution
mat4 projection;
glm_perspective(FOV, aspect, NEAR_PLANE, FAR_PLANE, projection);
```

**Notes:**
- Returns the internal render target size, not the composited size
- Useful for setting viewports and calculating aspect ratios
- The size is fixed at creation time (set in `SituationCreateVirtualDisplay()`)

---
#### `SituationDrawModel`
Draws all sub-meshes of a model with a single root transformation. This is a convenience function that draws all meshes in the model hierarchy.

```c
void SituationDrawModel(SituationCommandBuffer cmd, SituationModel model, mat4 transform);
```

**Parameters:**
- `cmd` - Command buffer to record into
- `model` - Model to draw (contains multiple meshes)
- `transform` - Root transformation matrix (model-to-world)

**Usage Example:**
```c
// Load and draw a model
SituationModel character;
SituationLoadModel("models/character.gltf", &character);

// Draw with transformation
mat4 transform;
glm_mat4_identity(transform);
glm_translate(transform, (vec3){0.0f, 0.0f, -5.0f});
glm_rotate(transform, glm_rad(45.0f), (vec3){0.0f, 1.0f, 0.0f});
glm_scale(transform, (vec3){2.0f, 2.0f, 2.0f});

SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
SituationCmdBindPipeline(cmd, shader);
SituationDrawModel(cmd, character, transform);

// Draw multiple instances
for (int i = 0; i < enemy_count; i++) {
    mat4 enemy_transform;
    glm_mat4_identity(enemy_transform);
    glm_translate(enemy_transform, enemies[i].position);
    glm_rotate(enemy_transform, enemies[i].rotation, (vec3){0.0f, 1.0f, 0.0f});
    
    SituationDrawModel(cmd, enemy_model, enemy_transform);
}

// Draw with camera
mat4 mvp;
glm_mat4_mul(camera.projection, camera.view, mvp);
glm_mat4_mul(mvp, model_transform, mvp);

// Set MVP as push constant
SituationCmdSetPushConstant(cmd, 0, &mvp, sizeof(mat4));
SituationDrawModel(cmd, model, model_transform);
```

**Notes:**
- Draws all sub-meshes in the model
- Each sub-mesh may have different materials
- Transform is applied to all sub-meshes
- Requires bound pipeline and descriptor sets
- Use `SituationCmdDrawMesh()` for individual meshes

---
#### `SituationSaveModelAsGltf`
Exports a model to a human-readable .gltf and a .bin file for debugging and inspection.

```c
SituationError SituationSaveModelAsGltf(SituationModel model, const char* file_path);
```

**Parameters:**
- `model` - Model to export
- `file_path` - Output path (without extension, .gltf and .bin will be added)

**Returns:** `SITUATION_SUCCESS` on success, or an error code on failure

**Usage Example:**
```c
// Export procedurally generated model
SituationModel terrain = GenerateTerrainModel();
if (SituationSaveModelAsGltf(terrain, "exports/terrain") == SITUATION_SUCCESS) {
    printf("Model exported to exports/terrain.gltf and exports/terrain.bin\n");
}

// Debug model loading
SituationModel model;
if (SituationLoadModel("models/character.gltf", &model) == SITUATION_SUCCESS) {
    // Re-export to verify loading
    SituationSaveModelAsGltf(model, "debug/character_reexport");
}

// Export modified model
SituationModel original;
SituationLoadModel("models/original.gltf", &original);

// Modify model...
ModifyModelGeometry(&original);

// Save modified version
SituationSaveModelAsGltf(original, "models/modified");
```

**Notes:**
- Creates two files: .gltf (JSON) and .bin (binary data)
- GLTF format is human-readable for debugging
- Useful for exporting procedural geometry
- Can be re-imported into modeling tools
- Preserves mesh, material, and transform data

---
#### `SituationScreenshotFormat`

Controls the default file format used by `SituationTakeScreenshot`.

```c
typedef enum SituationScreenshotFormat {
    SIT_SCREENSHOT_BMP = 0,     // Windows Bitmap (default, fastest write)
    SIT_SCREENSHOT_PNG,         // Portable Network Graphics (lossless, compressed)
    SIT_SCREENSHOT_JPG,         // JPEG (lossy, small files, quality 90)
    SIT_SCREENSHOT_TGA,         // Targa (lossless, RLE compressed)
    SIT_SCREENSHOT_FORMAT_COUNT // Sentinel (number of supported formats)
} SituationScreenshotFormat;
```

#### `sit_screenshot_format_ext[]`

Compile-time lookup table mapping each `SituationScreenshotFormat` value to its file extension string (including the leading dot). Indexed directly by the enum value.

```c
static const char* const sit_screenshot_format_ext[] = {
    ".bmp",  // SIT_SCREENSHOT_BMP
    ".png",  // SIT_SCREENSHOT_PNG
    ".jpg",  // SIT_SCREENSHOT_JPG
    ".tga",  // SIT_SCREENSHOT_TGA
};
```

This is a header-level constant available to any translation unit that includes `situation_api.h`. Use it for format-to-extension lookups without a function call (e.g., building filenames in tight loops or compile-time dispatch).

---

#### `SituationSetScreenshotFormat`

Sets the default screenshot file format used when the filename extension does not imply a specific format.

```c
void SituationSetScreenshotFormat(SituationScreenshotFormat format);
```

**Parameters:**
- `format` - One of `SIT_SCREENSHOT_BMP`, `SIT_SCREENSHOT_PNG`, `SIT_SCREENSHOT_JPG`, `SIT_SCREENSHOT_TGA`

---

#### `SituationGetScreenshotFormat`

Returns the current default screenshot format.

```c
SituationScreenshotFormat SituationGetScreenshotFormat(void);
```

**Returns:** The currently configured `SituationScreenshotFormat`.

---

#### `SituationGetScreenshotFormatExtension`

Returns the file extension string (including the leading dot) for a given screenshot format enum value.

```c
const char* SituationGetScreenshotFormatExtension(SituationScreenshotFormat format);
```

**Parameters:**
- `format` - The screenshot format to query (e.g., `SIT_SCREENSHOT_BMP`, `SIT_SCREENSHOT_PNG`).

**Returns:** A static string such as `".bmp"`, `".png"`, `".jpg"`, or `".tga"`. Returns `".bmp"` for unknown values.

---

#### `SituationTakeScreenshot`
Takes a screenshot of the current frame and saves it to a file. The `fileName` parameter is a base name (without extension) or NULL for automatic naming. The file extension is appended automatically based on the format set via `SituationSetScreenshotFormat` (default: BMP).

```c
SituationError SituationTakeScreenshot(const char *fileName);
```

**Parameters:**
- `fileName` - Base file name (no extension), or NULL for auto-generated name. The appropriate extension (`.bmp`, `.png`, etc.) is appended based on the current screenshot format setting.

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Set default format to PNG for all future screenshots
SituationSetScreenshotFormat(SIT_SCREENSHOT_PNG);

// Take screenshot with F12 — saves as "screenshots/screenshot_<timestamp>.png"
if (SituationIsKeyPressed(SIT_KEY_F12)) {
    time_t now = time(NULL);
    char filename[256];
    snprintf(filename, sizeof(filename), "screenshots/screenshot_%ld", now);
    
    if (SituationTakeScreenshot(filename) == SITUATION_SUCCESS) {
        printf("Screenshot saved!\n");
    } else {
        printf("Failed to save screenshot\n");
    }
}

// Sequential screenshots — extension added from format setting
static int screenshot_count = 0;
if (SituationIsKeyPressed(SIT_KEY_F12)) {
    char filename[256];
    snprintf(filename, sizeof(filename), "screenshot_%04d", screenshot_count++);
    SituationTakeScreenshot(filename);
}

// NULL for fully auto-generated filename
SituationTakeScreenshot(NULL);

// Change format, then take screenshot — extension follows
SituationSetScreenshotFormat(SIT_SCREENSHOT_BMP);
SituationTakeScreenshot("capture");   // → "capture.bmp"

SituationSetScreenshotFormat(SIT_SCREENSHOT_PNG);
SituationTakeScreenshot("capture");   // → "capture.png"

// Create screenshots directory if needed
#ifdef _WIN32
    _mkdir("screenshots");
#else
    mkdir("screenshots", 0755);
#endif
SituationTakeScreenshot("screenshots/capture");
```

**Notes:**
- Captures the current frame buffer
- `fileName` is a base name — the extension is added automatically from `SituationSetScreenshotFormat`
- Pass NULL to auto-generate the filename
- Use `SituationSetScreenshotFormat` to control the output format (BMP, PNG, etc.)
- BMP is fastest (no compression overhead), PNG recommended for distribution
- JPEG uses quality 90 (good balance of size and fidelity)
- Creates parent directories if they don't exist
- Overwrites existing files without warning

---
### Deprecated `SituationCmd*` (migration only)

See **[situation_command_reference.md §11](../situation_command_reference.md#11-deprecated-commands)**. Prefer `SituationCmdBeginRenderPass` / `EndRenderPass`, `BindDescriptorSet`, `BindTextureSet`, `SituationCmdPipelineBarrier` over legacy names.

---
### Dynamic raster & debug commands

See **[situation_command_reference.md §3](../situation_command_reference.md#3-raster-state-fixed-function)** and **[§9](../situation_command_reference.md#9-debug-markers)**.

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

#### `SituationQueryShaderStorageBlocks` *(v2.4.258, OpenGL only)*
Enumerates all active SSBO blocks and their **post-link binding points** as assigned by the library after `_SituationBindGLProgramStorageBlocks`. Useful for diagnostics, harness reflection tests, and catching duplicate-binding regressions before drawing.

```c
typedef struct SituationShaderStorageBlockInfo {
    char name[128];         /* block name (may be empty for anonymous SPIR-V blocks) */
    uint32_t binding_point; /* binding point assigned after link                     */
    uint32_t block_index;   /* GL resource index (0-based)                          */
} SituationShaderStorageBlockInfo;

SituationError SituationQueryShaderStorageBlocks(
    SituationShader shader,
    SituationShaderStorageBlockInfo* out_blocks, /* NULL for count-only query */
    int capacity,
    int* out_count);                             /* receives actual block count */
```

- **Count-only query:** pass `out_blocks = NULL` or `capacity = 0`; `*out_count` receives the total.
- **Full query:** `out_blocks` is filled up to `capacity` entries; `*out_count` may exceed `capacity` if the program has more blocks.
- **Vulkan:** returns `SITUATION_ERROR_NOT_IMPLEMENTED`.
- **Regression guard:** catches the v2.4.81/82 failure where SPIR-V reflection reported both blocks at `GL_BUFFER_BINDING 0`; after the fix, `blocks[0].binding_point == 0`, `blocks[1].binding_point == 1`.

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

See **[situation_command_reference.md §8](../situation_command_reference.md#8-transfer--presentation)** (`SituationCmdCopyBuffer`).

---
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


---

#### `SituationCmdBindMeshPullBuffers`
[VK+GL] Push mesh vertex/index BDA block for pull shaders (SituationMeshPullPushConstants @ offset 0). Requires SIT_FEATURE_BINDLESS_BUFFERS.
```c
SituationError SituationCmdBindMeshPullBuffers(SituationCommandBuffer cmd, SituationMesh mesh);
```

---

#### `SituationCreateMeshEx`
Create a mesh with an explicit vertex layout tag (includes SIT_MESH_LAYOUT_PULL).
```c
SituationError SituationCreateMeshEx(const void* vertex_data, int vertex_count, size_t vertex_stride, const uint32_t* index_data, int index_count, SituationMeshVertexLayout layout, SituationMesh* out_mesh);
```

---

#### `SituationCreateVirtualDisplayFromDesc`
Create a virtual display from a full desc (VD-1 attachment config).
```c
SituationError SituationCreateVirtualDisplayFromDesc(const SituationVirtualDisplayDesc* desc, int* out_id);
```

---

#### `SituationGetMeshIndexBufferAddress`
Retrieves the GPU device address of the mesh index buffer. [VK] requires SIT_FEATURE_BINDLESS_BUFFERS; [GL] NVIDIA-only via NV_shader_buffer_load. Returns 0 if unsupported.
```c
uint64_t SituationGetMeshIndexBufferAddress(SituationMesh mesh);
```

---

#### `SituationGetMeshVertexBufferAddress`
GPU VA of mesh vertex buffer. [VK] SIT_FEATURE_BINDLESS_BUFFERS; pull draw: SituationCmdBindMeshPullBuffers + buffer_reference VS (see sit/gpu/vertex_pull.glslh).
```c
uint64_t SituationGetMeshVertexBufferAddress(SituationMesh mesh);
```

---

#### `SituationGetMeshVertexLayout`
Query layout tag stored at creation.
```c
SituationError SituationGetMeshVertexLayout(SituationMesh mesh, SituationMeshVertexLayout* out_layout);
```

---

#### `SituationGetVirtualDisplayUpdateInfo`
Query last VD content write (not the frame clock).
```c
SituationError SituationGetVirtualDisplayUpdateInfo(int display_id, double* out_last_content_update_time, uint64_t* out_last_content_update_frame, uint64_t* out_frames_since_update, double* out_seconds_since_update);
```

---

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

#### `SituationReadFramebufferHdr`
[Phase 8] Raw A2R10G10B10 readback when HDR10 swapchain active.
```c
SituationError SituationReadFramebufferHdr(const SituationReadPixelsDesc* desc, uint32_t* dst_pixels, size_t dst_size_bytes);
```

---

#### `SituationRenderPassInfoInherit`
Fill pass struct from VD attachment defaults (tier C helper).
```c
SituationRenderPassInfo SituationRenderPassInfoInherit(int display_id);
```

---

#### `SituationRequestScreenCapture`
_See `sit/situation_api.h` for the authoritative declaration._
```c
void SituationRequestScreenCapture(void);
```

---

#### `SituationSetVirtualDisplayAttachmentDefaults`
Tier B storage-only attachment defaults (load/store/clear). Rejects if VD is inside an active render pass.
```c
SituationError SituationSetVirtualDisplayAttachmentDefaults(int display_id, const SituationVirtualDisplayAttachmentDefaults* defaults);
```

---

#### `SituationRenderPassInfoInherit`
Tier C helper — fills a `SituationRenderPassInfo` from a VD's stored attachment defaults (including clear color).
```c
SituationRenderPassInfo SituationRenderPassInfoInherit(int display_id);
```

---

#### VD bolster configure APIs (v2.4.387)

See [Virtual Display Module — composite sampler & quality](virtual_display.md#composite-sampler--quality-v2487).

```c
SituationError SituationSetVirtualDisplayClearColor(int display_id, ColorRGBA color);
SituationError SituationSetVirtualDisplaySampler(int display_id, const SituationVirtualDisplaySamplerDesc* sampler);
SituationError SituationSetVirtualDisplayMaxAnisotropy(int display_id, float max_anisotropy);
SituationError SituationSetVirtualDisplayMipLevels(int display_id, uint32_t color_mip_levels, uint32_t sampler_max_mip_level);
SituationError SituationSetVirtualDisplayUpdateMode(int display_id, SituationVirtualDisplayUpdateMode mode);
SituationError SituationSetVirtualDisplayMemoryHint(int display_id, SituationVirtualDisplayMemoryHint hint);
```

- **`SetVirtualDisplaySampler`** — light rebuild (sampler + descriptor before next composite).
- **`SetVirtualDisplayMipLevels`** — `sampler_max_mip_level` updates immediately; changing **storage** `color_mip_levels` at runtime returns **`NOT_IMPLEMENTED`** (set on **`SituationCreateVirtualDisplayFromDesc`**).
- **`SetVirtualDisplayUpdateMode(SIT_VD_UPDATE_STATIC)`** — freezes VD frame clock in **`SituationUpdateTimers`**; pair with **`SituationSetVirtualDisplayDirty`** for manual refresh.
- All configure APIs above reject during an active VD render pass (`SITUATION_ERROR_RENDER_PASS_ACTIVE`).

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

#### `SituationCmdPresent`
Submits a command to copy a texture to the main window's swapchain (Compute-Only).
```c
SituationError SituationCmdPresent(SituationCommandBuffer cmd, SituationTexture texture);
```

