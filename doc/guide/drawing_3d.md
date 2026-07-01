## 3D Rendering & Drawing

**Overview:** Situation's 3D path is **shader + mesh + camera** — you supply GLSL (compiled at runtime or from SPIR-V), upload geometry as a `SituationMesh`, set view/projection matrices, and record draws into the same command buffer as 2D. There is no hidden 3D engine: you own the vertex shader, fragment shader, and transforms. The library provides mesh creation, model loading (GLTF/OBJ/STL), pipeline binding, depth-tested render passes, camera helpers, and optional high-level `SituationDrawModel` for PBR GLTF.

**When to use this vs 2D:** Use [2D Drawing](drawing_2d.md) for UI, sprites, and retro quads (`DrawQuad`, `DrawTexture`). Use this guide when you need **perspective**, **depth sorting**, **lit surfaces**, **loaded assets**, or **custom GLSL**.

**Canonical examples (start here):**

| Example | What it teaches |
|---------|-----------------|
| **`examples/20_load_and_draw_model/`** | **Primary model path** — OBJ/STL load, `SituationCameraDesc`, orbit cam, manual sub-mesh draw |
| `examples/other/shader_lab_torus.c` | Procedural mesh, depth, custom shading, fullscreen background, 2D HUD overlay |
| `examples/other/basic_triangle.c` | Minimal custom shader + mesh (no camera — prototyping only) |
| `examples/other/vertex_pull_triangle.c` | Vulkan vertex-pull (advanced) |
| `examples/demon_hunt/` | Production-scale: VD world + sky mesh + SPIR-V post FX |

Build (from repo root):

```powershell
& "c:\Users\User\Desktop\hobby\_kiro\situation\build\build_situation.bat" static-opengl
& "c:\Users\User\Desktop\hobby\_kiro\situation\build\build_examples.bat" static-opengl 20_load_and_draw_model
& "c:\Users\User\Desktop\hobby\_kiro\situation\build\build_examples.bat" static-opengl shader_lab_torus
```

**Related:** [2D Drawing](drawing_2d.md) · [Graphics](graphics.md) · [Hot reload](hot_reload.md) · [Advanced GPU commands](renderer_bolster.md) · [situation_command_reference.md](../situation_command_reference.md)

---

### Coming from another 3D API?

| You're used to… | Situation equivalent |
|-----------------|---------------------|
| **Three.js** `Mesh` + `ShaderMaterial` | `SituationCreateMesh` + `SituationLoadShaderFromMemory` + `SituationCmdDrawMesh` |
| **Unity** `MeshRenderer` + materials | Load model → loop sub-meshes with your shader; or GLTF + PBR shader + `SituationDrawModel` |
| **OpenGL** VAO/VBO + `glDrawElements` | `SituationCreateMesh` hides VAO; you still write GLSL |
| **Vulkan** PSO + `vkCmdDrawIndexed` | `SituationLoadShaderFromMemory` builds PSO variants; `SituationCmdDrawMesh` records draw |
| **Raylib** `LoadModel` + `DrawModel` | `SituationLoadModelFromOBJ` / `FromSTL` / `LoadModel` + your draw loop (example 20) |

**Situation-specific perks:**

| Perk | Benefit |
|------|---------|
| **Same command buffer as 2D** | Draw world in 3D, HUD with `DrawTextEx` in one pass |
| **Runtime GLSL → both backends** | One shader source path for OpenGL and Vulkan (with `SITUATION_ENABLE_SHADER_COMPILER`) |
| **Built-in GLTF/OBJ/STL loaders** | Geometry + textures uploaded to GPU automatically |
| **`SituationCameraDesc`** | Shared view/proj builders — no duplicated `glm_lookat` math |
| **Legacy + PBR vertex layouts** | Standard strides (12/20/24/32/48 bytes) map to pipeline variants automatically |
| **`SituationDrawModel`** (optional) | PBR convenience: iterates sub-meshes, binds material textures, pushes 96-byte material block |
| **Deferred uniform recording** | `SituationSetShaderUniform` during a frame records into the command buffer (OpenGL replay) |
| **Virtual Displays** | Render 3D scene at fixed resolution, composite to window — [Virtual Display guide](virtual_display.md) |

---

### Mental model — four pieces

```
  CPU                          GPU (each frame)
  ───                          ────────────────
  Vertex[]  ──CreateMesh──►    VBO + IBO
  GLSL VS/FS ──LoadShader──►   Pipeline (PSO)
  view, proj ──Uniforms───►    Vertex shader
  model      ──Uniform/Push►   Per-object transform
                               Fragment shader → color + depth
```

Every 3D frame:

1. **Acquire** command buffer
2. **Begin render pass** with **depth clear** (`SituationRenderPassInfoDefault` does this for you)
3. **Bind pipeline** (your shader)
4. **Upload camera** (UBO, uniforms, or pre-multiplied MVP)
5. **For each object:** set model matrix → bind textures if any → `SituationCmdDrawMesh`
6. **Optional 2D overlay** (text, HUD quads)
7. **End pass** → `SituationEndFrame`

---

### Two paths for loaded models

Most maintained examples use **Path A**. Use **Path B** only when you have a full PBR shader that matches the library's material contract.

| | **Path A — manual draw** (recommended) | **Path B — `SituationDrawModel`** (PBR GLTF) |
|---|----------------------------------------|-----------------------------------------------|
| **Load** | `SituationLoadModelFromOBJ`, `FromSTL`, or `LoadModel` | `SituationLoadModel` (GLTF/GLB) |
| **Shader** | Your lit/textured shader (full control) | **Must** match PBR push-constant + sampler layout |
| **Draw** | Loop `model.meshes[i].gpu_mesh` → `SituationCmdDrawMesh` | `SituationDrawModel(cmd, model, transform)` |
| **Canonical code** | `examples/20_load_and_draw_model/main.c` | Harness: `tests/harness/test_model_loader.c` (DrawModel tests) |
| **Best for** | OBJ/STL, custom lighting, learning | Textured GLTF with albedo/normal/MR maps |

---

### 2D vs 3D at a glance

| | [2D Drawing](drawing_2d.md) | 3D (this guide) |
|---|------------------------------|-----------------|
| Entry API | `SituationCmdDrawQuad`, `DrawTexture` | `SituationCmdBindPipeline`, `DrawMesh` |
| Camera | Ortho set automatically | `SituationCameraDesc` or `view` + `proj` (cglm) |
| Depth buffer | Usually off / don't care | **Required** for correct occlusion |
| Shaders | Internal (hidden) | **Your GLSL** |
| Assets | Textures, CPU images | Meshes, GLTF/OBJ/STL models |
| Best for | UI, sprites, retro 2D | Worlds, props, custom shading |

---

### Tier 1 — Custom mesh + custom shader

The workhorse path. Same pattern as `shader_lab_torus.c`.

#### 1. Define vertex data

Situation recognizes standard strides via `SituationCreateMesh` / `SituationCreateMeshEx`:

| Layout | Stride | Attributes |
|--------|--------|------------|
| `SIT_MESH_LAYOUT_POS_ONLY` | 12 | `vec3 position` |
| `SIT_MESH_LAYOUT_POS_TEX` | 20 | position + `vec2 uv` |
| `SIT_MESH_LAYOUT_POS_NRM` | 24 | position + `vec3 normal` |
| `SIT_MESH_LAYOUT_POS_NRM_TEX` | 32 | position + normal + uv (**procedural meshes**) |
| `SIT_MESH_LAYOUT_POS_NRM_TAN_TEX` | 48 | position + normal + **tangent** + uv (**GLTF loader output**) |
| `SIT_MESH_LAYOUT_PULL` | 0 | Vertex-pull path — see Advanced |

```c
typedef struct {
    float pos[3];
    float nrm[3];
    float uv[2];
} Vertex;   /* 32 bytes — torus / hand-authored meshes */

typedef struct {
    float pos[3];
    float nrm[3];
    float tan[4];   /* xyz + handedness w */
    float uv[2];
} PBRVertex;  /* 48 bytes — matches SituationLoadModel meshes */
```

Use `SituationCreateMeshEx(..., SIT_MESH_LAYOUT_POS_NRM_TAN_TEX, &mesh)` when stride is 48.

Attribute locations **must** match your struct (0=pos, 1=normal, 2=uv for 32-byte; tangent at location 2 shifts uv to 3 for 48-byte PBR).

#### 2. Load shaders

```c
static const char* vs =
    "#version 450 core\n"
    "layout(location = 0) in vec3 inPos;\n"
    "layout(location = 1) in vec3 inNormal;\n"
    "layout(location = 2) in vec2 inUV;\n"
    "uniform mat4 uModel;\n"
    "uniform mat4 uView;\n"
    "uniform mat4 uProj;\n"
    "layout(location = 0) out vec3 vNormal;\n"
    "void main() {\n"
    "    vNormal = mat3(uModel) * inNormal;\n"
    "    gl_Position = uProj * uView * uModel * vec4(inPos, 1.0);\n"
    "}\n";

SituationShader shader = {0};
SituationLoadShaderFromMemory(vs, fs, &shader);
/* Or from files (hot-reload friendly): SituationLoadShader("shaders/mesh.vert", "shaders/mesh.frag", &shader); */
```

#### 3. Create mesh

```c
SituationMesh mesh = {0};
SituationCreateMesh(vertices, vert_count, sizeof(Vertex),
                    indices, index_count, &mesh);
```

#### 4. Camera — prefer `SituationCameraDesc`

```c
SituationCameraDesc cam = {0};
cam.eye    = (Vector3){{ 0.0f, 2.0f, 5.0f }};
cam.target = (Vector3){{ 0.0f, 0.0f, 0.0f }};
cam.up     = (Vector3){{ 0.0f, 1.0f, 0.0f }};
cam.vertical_fov_deg = 45.0f;
cam.aspect = 0.0f;   /* 0 = auto from render target */
cam.z_near = 0.1f;
cam.z_far  = 100.0f;

mat4 view, proj;
SituationCameraBuildView(&cam, view);
SituationCameraBuildProj(&cam, proj);
/* Or combined: SituationCameraBuildViewProj(&cam, vp); */
```

See [Graphics — Camera & projection helpers](graphics.md#camera--projection-helpers) for orthographic, reverse-Z, and picking (`SituationCameraUnprojectPixel`).

#### 5. Render pass with depth

```c
/* Default helper already clears color + depth (depth = 1.0, store = DONT_CARE). */
SituationRenderPassInfo pass =
    SituationRenderPassInfoDefault(-1, (ColorRGBA){20, 25, 35, 255});

SituationCmdBeginRenderPass(cmd, &pass);
SituationCmdBindPipeline(cmd, shader);

mat4 model;
glm_mat4_identity(model);

SituationSetShaderUniform(shader, "uModel", model, SIT_UNIFORM_MAT4);
SituationSetShaderUniform(shader, "uView",  view,  SIT_UNIFORM_MAT4);
SituationSetShaderUniform(shader, "uProj",  proj,  SIT_UNIFORM_MAT4);

SituationCmdDrawMesh(cmd, mesh);
SituationCmdEndRenderPass(cmd);
```

**Without a depth attachment clear/load**, overlapping triangles won't occlude correctly.

#### Orbit camera recipe (shader lab style)

```c
void orbit_eye(float yaw, float pitch, float radius, vec3 target, vec3 out_eye) {
    out_eye[0] = target[0] + radius * cosf(pitch) * sinf(yaw);
    out_eye[1] = target[1] + radius * sinf(pitch);
    out_eye[2] = target[2] + radius * cosf(pitch) * cosf(yaw);
}
/* Fill cam.eye from orbit_eye(), cam.target = target, then BuildViewProj. */
```

---

### Cross-backend shaders (OpenGL vs Vulkan)

When targeting **both** backends from one C file, per-object transforms often differ:

| Backend | Typical per-draw transform |
|---------|---------------------------|
| **OpenGL** | `layout(location = 0) uniform mat4 uModel;` — uploaded via `SituationSetShaderUniform` or `SituationCmdSetPushConstantData` |
| **Vulkan** | `layout(push_constant) uniform PC { mat4 uModel; } pc;` — uploaded via `SituationCmdSetPushConstant(cmd, 0, &mvp, sizeof(mat4))` |

Example 20 (`examples/20_load_and_draw_model/main.c`) uses `#if defined(SITUATION_USE_VULKAN)` to select the correct GLSL. The harness helpers in `tests/harness/sit_graphics_test_helpers.h` follow the same pattern.

**Rule:** Vulkan fixes vertex format at pipeline creation — use standard strides or `CreateMeshEx` with an explicit layout tag. OpenGL-only `SituationCmdSetVertexAttribute` is deprecated for new code; prefer standard layouts or vertex-pull (`sit/gpu/vertex_pull.glslh`).

---

### Tier 2 — Load a file, draw with your shader (Path A)

**Start with example 20.** Run from repo root so assets resolve:

```
tests/harness/assets/utah_teapot.obj   (fallback: teapot.stl)
```

```c
SituationModel model = {0};
SituationLoadModelFromOBJ("tests/harness/assets/utah_teapot.obj", &model);
/* Or: SituationLoadModelFromSTL(path, smooth_normals, &model) */
/* Or: SituationLoadModel("assets/models/BoomBox.glb", &model) for GLTF */
```

#### Frame the model with `SituationGetMeshData`

Example 20 reads vertex data back to compute an AABB and center the camera:

```c
void* vdata = NULL;
int vcount = 0, vstride = 0;
SituationGetMeshData(model.meshes[0].gpu_mesh, &vdata, &vcount, &vstride, NULL, NULL);
/* Scan vdata for min/max → g_center, g_extent; then SIT_FREE(vdata); */
```

#### Draw loop (manual — full control)

```c
SituationCmdBindPipeline(cmd, shader);
SituationCmdSetCullMode(cmd, SIT_CULL_BACK);

mat4 mvp;
SituationCameraBuildViewProj(&cam, vp);
glm_mat4_mul(vp, model_matrix, mvp);

for (int i = 0; i < model.mesh_count; ++i) {
    SituationMesh mesh = model.meshes[i].gpu_mesh;
    if (mesh.generation == 0 && mesh.slot_index == 0) continue;

#if defined(SITUATION_USE_VULKAN)
    SituationCmdSetPushConstant(cmd, 0, mvp, sizeof(mat4));
#else
    SituationSetShaderUniform(shader, "uMVP", mvp, SIT_UNIFORM_MAT4);
#endif
    SituationCmdDrawMesh(cmd, mesh);
}
```

Each `SituationModelMesh` also carries PBR metadata (`base_color_factor`, `metallic_factor`, `roughness_factor`, texture handles) you can bind yourself — see [Graphics — SituationModelMesh](graphics.md).

---

### Tier 3 — GLTF PBR with `SituationDrawModel` (Path B)

`SituationDrawModel` is a **convenience wrapper** — it does **not** bind a pipeline. You must bind a PBR-compatible shader first. It then, per sub-mesh:

1. Pushes a **96-byte** material block (model matrix + base color + metallic/roughness)
2. Binds material textures via `SituationCmdBindTextureSet`
3. Calls `SituationCmdDrawMesh`

#### Push-constant layout (must match exactly)

```c
typedef struct {
    mat4    model;              /* 64 bytes */
    Vector4 base_color_factor;  /* 16 bytes */
    Vector4 pbr_factors;        /* x = metallic, y = roughness */
} SituationPBRDrawModelPush;    /* 96 bytes total */
```

#### Shader binding contract

| Resource | OpenGL GLSL | Vulkan GLSL | Constant |
|----------|-------------|-------------|----------|
| View + projection UBO | `layout(std140, binding = 1) uniform ViewData { mat4 view; mat4 projection; };` | `layout(std140, set = 0, binding = 1) uniform ViewData { … };` | `SIT_UBO_BINDING_VIEW_DATA` (= **1**) |
| Albedo | `layout(binding = 0) uniform sampler2D uAlbedo;` | `layout(set = 1, binding = 0) uniform sampler2D uAlbedo;` | `SIT_SAMPLER_BINDING_ALBEDO` |
| Normal map | binding 1 | set 1, binding 1 | `SIT_SAMPLER_BINDING_NORMAL` |
| Metallic-roughness | binding 2 | set 1, binding 2 | `SIT_SAMPLER_BINDING_PBR_MAP` |
| Emissive | binding 3 | set 1, binding 3 | `SIT_SAMPLER_BINDING_EMISSIVE` |

Camera UBO struct (matches [Graphics — ViewDataUBO](graphics.md)):

```c
typedef struct ViewDataUBO {
    mat4 view;
    mat4 projection;
} ViewDataUBO;
```

Upload once per frame via `SituationCreateBuffer` + `SituationCmdBindDescriptorSet(cmd, 0, ubo)` for cross-backend correctness. Prototyping on OpenGL-only may use `SituationSetShaderUniform` for `uView`/`uProj` instead.

#### Draw call

```c
SituationCmdBindPipeline(cmd, pbr_shader);
/* upload ViewDataUBO to binding SIT_UBO_BINDING_VIEW_DATA */

mat4 model_matrix;
glm_mat4_identity(model_matrix);
glm_rotate(model_matrix, angle, (vec3){0, 1, 0});

SituationDrawModel(cmd, model, model_matrix);
```

There is no maintained standalone PBR example yet — use the harness tests (`test_model_loader.c`, `test_graphics.c`) as reference implementations. For textured OBJ without full PBR, bind `model.meshes[i].base_color_texture` manually in your Path A loop (see `graphics_test_draw_model_triplanar` in the harness).

---

### Mixing 3D world + 2D HUD

Draw order within one render pass:

1. **Sky / background** (fullscreen mesh at far depth, or clear color — see shader lab background pass)
2. **Opaque 3D** (depth write on)
3. **Transparent 3D** (optional: push raster state, depth write off)
4. **2D overlay** — `SituationCmdDrawTextEx`, `SituationCmdDrawQuad` for crosshair/HUD

`shader_lab_torus.c` draws the torus, then centered help text on top. 2D commands use the internal orthographic projection already set for the frame — they do not replace your 3D shader state for subsequent draws unless you re-bind.

For a **3D world in a fixed-resolution layer**, render into a [Virtual Display](virtual_display.md) with a depth-enabled pass, then composite — pattern used in `examples/demon_hunt/` and `05_virtual_display_retro`.

---

### Procedural geometry without asset files

`shader_lab_torus.c` builds a torus on the CPU at startup:

```c
/* Fill TorusVertex[] + uint32_t indices[], then: */
SituationCreateMesh(vbuf, vcount, sizeof(TorusVertex), ibuf, icount, &mesh);
```

Same approach works for terrain chunks, debug grids, and fullscreen triangles (background pass). For a **fake 3D cube** without shaders, see `spinning_cube.c` — it uses six `SituationCmdDrawQuad` faces (2D trick, not true mesh 3D).

---

### Shader iteration

Use `SituationLoadShader` from disk and enable hot reload during development — see [Hot reload](hot_reload.md). Pair with `shader_lab_torus.c` or example 20 for fast GLSL iteration without rebuilding the app.

---

### Advanced paths

| Topic | Where |
|-------|--------|
| Manual `BindVertexBuffer` + `DrawIndexed` | [Advanced GPU commands](renderer_bolster.md) + [command reference §5](../situation_command_reference.md#5-vertex-input--manual-draw-core-path) |
| Vertex pull (Vulkan BDA) | `vertex_pull_triangle.c`, `SituationCmdBindMeshPullBuffers`, `SIT_MESH_LAYOUT_PULL`, `sit/gpu/vertex_pull.glslh` |
| Precompiled SPIR-V | `SituationLoadShaderFromSpirv` (Demon Hunt sky shader) |
| Export procedural mesh | `SituationSaveModelAsGltf` |
| Depth bias, wireframe, cull | `SituationCmdSetDepthBias`, `SetPolygonMode`, `SetCullMode` + push/pop raster state |
| Compute → render | [Compute](compute.md) + barriers, then bind graphics pipeline |

---

### Frame loop skeleton

```c
while (!SituationWindowShouldClose()) {
    SituationPollInputEvents();
    SituationUpdateTimers();
    if (SitExample_BeginFrame()) break;

    if (SituationAcquireFrameCommandBuffer() != SITUATION_SUCCESS) continue;

    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    SituationRenderPassInfo pass =
        SituationRenderPassInfoDefault(-1, (ColorRGBA){15, 18, 28, 255});

    SituationCmdBeginRenderPass(cmd, &pass);
    draw_scene_3d(cmd);
    draw_hud_2d(cmd);
    SituationCmdEndRenderPass(cmd);

    SituationEndFrame();
}
```

---

### Resource lifecycle

| Resource | Create | Destroy |
|----------|--------|---------|
| Mesh | `SituationCreateMesh` / `CreateMeshEx` | `SituationDestroyMesh` |
| Shader | `SituationLoadShaderFromMemory` / `LoadShader` | `SituationUnloadShader` |
| Model | `SituationLoadModel` / `FromOBJ` / `FromSTL` | `SituationUnloadModel` |
| Texture | `SituationCreateTexture` (usually via loader) | `SituationDestroyTexture` |
| Buffer (UBO) | `SituationCreateBuffer` | `SituationDestroyBuffer` |

Always destroy in reverse order of creation before `SituationShutdown`. Leaked GPU resources are reported at shutdown in debug builds.

---

### Troubleshooting

| Symptom | Likely cause | Fix |
|---------|--------------|-----|
| Nothing visible | Shader compile fail, wrong pipeline | Check `SituationGetLastErrorMsg` after load |
| Flat / no perspective | Missing or identity `proj` | Use `SituationCameraBuildProj` or `glm_perspective` |
| Z-fighting | Near/far plane too tight | Widen clip range; use `SetDepthBias` for shadows |
| Wrong culling (invisible) | Back faces culled | `SituationCmdSetCullMode(SIT_CULL_NONE)` or fix winding |
| Model pink/black (PBR path) | Shader not PBR-compatible with `DrawModel` | Match 96-byte push block + sampler bindings (Tier 3) |
| Model invisible | Pipeline not bound before draw | `SituationCmdBindPipeline` first |
| Vertices garbage | Stride/layout mismatch | Match 32- or 48-byte layout; use `SituationGetMeshVertexLayout` |
| GLTF mesh wrong normals | Expected 48-byte PBR layout | Shader locations 0–3: pos, nrm, tangent, uv |
| Works GL, fails VK | GL-only `uniform mat4` on Vulkan | Use push constants on VK (see Cross-backend section) |
| Works GL, fails VK | `SetVertexAttribute` on Vulkan | Vertex layout fixed at pipeline build; use standard strides |
| HUD draws wrong after 3D | State leak | Re-bind pipeline or use `PushRasterState` |
| Depth ignored | No depth attachment in pass | Use `SituationRenderPassInfoDefault` or set `depth_attachment.loadOp = CLEAR` |
| Asset not found | Wrong working directory | Run from repo root; use `SituationFileExists` before load |

---

### API quick reference

#### Mesh

```c
SituationError SituationCreateMesh(const void* verts, int vert_count, size_t stride,
    const uint32_t* indices, int index_count, SituationMesh* out);
SituationError SituationCreateMeshEx(/* … */, SituationMeshVertexLayout layout, SituationMesh* out);
SituationError SituationDestroyMesh(SituationMesh* mesh);
SituationError SituationGetMeshVertexLayout(SituationMesh mesh, SituationMeshVertexLayout* out_layout);
void SituationGetMeshData(SituationMesh mesh, void** vertex_data, int* vertex_count,
    int* vertex_stride, void** index_data, int* index_count);
```

#### Shader & draw

```c
SituationError SituationLoadShader(const char* vs_path, const char* fs_path, SituationShader* out);
SituationError SituationLoadShaderFromMemory(const char* vs, const char* fs, SituationShader* out);
SituationError SituationCmdBindPipeline(SituationCommandBuffer cmd, SituationShader shader);
SituationError SituationCmdDrawMesh(SituationCommandBuffer cmd, SituationMesh mesh);
SituationError SituationSetShaderUniform(SituationShader shader, const char* name,
    const void* data, SituationUniformType type);
SituationError SituationCmdBindDescriptorSet(SituationCommandBuffer cmd, uint32_t set, SituationBuffer buffer);
SituationError SituationCmdBindTextureSet(SituationCommandBuffer cmd, uint32_t set_index, SituationTexture texture);
SituationError SituationCmdSetPushConstant(SituationCommandBuffer cmd, uint32_t contract_id,
    const void* data, size_t size);
```

#### Camera ([Graphics](graphics.md))

```c
typedef struct SituationCameraDesc { /* eye, target, up, fov, aspect, z_near, z_far, flags */ } SituationCameraDesc;
void SituationCameraBuildView(const SituationCameraDesc* desc, mat4 out_view);
void SituationCameraBuildProj(const SituationCameraDesc* desc, mat4 out_proj);
void SituationCameraBuildViewProj(const SituationCameraDesc* desc, mat4 out_vp);
typedef struct ViewDataUBO { mat4 view; mat4 projection; } ViewDataUBO;
```

#### Models

```c
SituationError SituationLoadModel(const char* path, SituationModel* out);
SituationError SituationLoadModelFromOBJ(const char* path, SituationModel* out);
SituationError SituationLoadModelFromSTL(const char* path, bool smooth_normals, SituationModel* out);
SituationError SituationDrawModel(SituationCommandBuffer cmd, SituationModel model, mat4 transform);
SituationError SituationUnloadModel(SituationModel* model);
SituationError SituationSaveModelAsGltf(SituationModel model, const char* path);
```

#### Raster state (3D tuning)

See [Advanced GPU commands — raster state](renderer_bolster.md#raster-state-commands-reference): `SetCullMode`, `SetDepthTest`, `SetDepthWrite`, `SetDepthBias`, `SetPolygonMode`, `PushRasterState` / `PopRasterState`.

---

### Example catalog

| Example | Focus |
|---------|--------|
| **`20_load_and_draw_model`** | **Start here** — OBJ/STL, camera helper, manual sub-mesh draw, bounds framing |
| `shader_lab_torus` | Procedural mesh, depth, orbit cam, backgrounds, 2D HUD |
| `basic_triangle` | Minimal custom shader (pos+color, no 3D camera) |
| `shader_lab_raytrace2` | Fullscreen raytrace fragment shader |
| `vertex_pull_triangle` | Vulkan vertex pull |
| `spinning_cube` | Fake 3D via `DrawQuad` faces (2D path) |
| `gpu_particle_simulation` | Compute + instanced quad draw |
| `demon_hunt/` | VD + sky mesh + SPIR-V post FX |
| `other/loading_and_rendering_a_3d_model.c` | **Superseded** — kept for dev reference only |

For numbered examples 01–20, see [Examples & Tutorials](examples_faq.md).
