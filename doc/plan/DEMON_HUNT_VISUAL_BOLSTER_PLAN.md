# Demon Hunt Visual Bolster Plan

## Goal

Elevate the Demon Hunt demo's visual quality on the Vulkan path by adding material-aware
shading, atmospheric effects, and sprite enhancements — all within the existing single-pass
fullscreen fragment shader architecture. No new render passes, no new textures, no new
descriptor sets. Everything is ALU + existing UBO/SSBO data.

**Baseline**: GTX 1070 + i7-4790K @ 1080p — **300+ fps, ~15% CPU usage**. GPU is lightly loaded; the bottleneck headroom is generous.

**Target**: Double the visual richness while staying above 200fps on the same hardware. The 100+ fps headroom gives significant room for new shader ALU and texture reads before the 200fps floor is threatened. CPU usage should drop slightly after Phase 1 removes the CPU sprite billboard path.

**Platform**: **Vulkan only.** The demon_hunt shader currently compiles to ~77,000 instructions.
OpenGL physically cannot run a shader of this size — the driver rejects it outright. OpenGL
is expired for this demo. `sky_draw_cpu_world_fallback()` (the flat-color column renderer)
remains in the codebase as a safety net if the Vulkan shader fails to link at runtime, but
it is not a playable path and is not tested as part of this plan. All build and validation
steps use `build_examples.bat vulkan demon_hunt` exclusively.

**Frame Feedback**: A single previous-frame texture is introduced (Phase 1.5) as shared
infrastructure. This enables real screen-space effects (bloom, CA, reflections, TAA) that
would otherwise be impossible in a single-pass architecture. Cost: one RGBA8 texture (~8MB)
plus one blit per frame (<0.1ms). The feedback texture is optional — all features degrade
gracefully to ALU-only approximations when `DH_ENABLE_FRAME_FEEDBACK 0`.

---

## Pre-Implementation Decisions (Resolved)

These three architectural questions were identified during review and are **locked in**
before coding starts. They are not open decision points during implementation.

### Decision 1: Hunter Drone → Shader Sprite Path (Phase 1)

**Choice**: Add `SHADER_SPRITE_HUNTER_DRONE` (type ID 8) to both the CPU packing and
the shader shading. Do NOT keep the CPU-only draw path for drones.

**Rationale**: The plan's goal is to retire the CPU sprite loop entirely. Leaving drones
as CPU-only would force keeping the entire `SprSort` infrastructure for a single sprite type.
The drone's visual (hull + wings + eye + glow) is structurally identical to the hellraiser
pattern and maps cleanly to the existing `shade_sprite_opaque()` architecture.

### Decision 2: Feedback Blit Source → VirtualDisplay (Phase 1.5)

**Choice**: Render the world (fullscreen shader) to a VirtualDisplay, then copy the VD's
internal texture to the feedback texture. Do NOT attempt to blit from the swapchain image.

**Rationale**: On Vulkan, the swapchain image is in `PRESENT_SRC` or `COLOR_ATTACHMENT`
layout and isn't directly blittable mid-render-pass without ending/restarting the pass.
A VirtualDisplay is a library-owned render target whose backing texture is accessible via
`SituationGetVirtualDisplayTexture()`. This gives a stable, blittable source without
swapchain layout gymnastics.

**Implementation flow**:
1. `SituationCreateVirtualDisplayEx(resolution, 1.0, 0, ...)` at startup
2. Begin render pass targeting the VD → draw fullscreen shader
3. End render pass → barrier → `SituationCmdCopyTexture(vd_tex, feedback_tex)` → barrier
4. Begin render pass targeting main window → composite VD + draw HUD/minimap

### Decision 3: Compile-Time Define Guards (All Phases)

**Choice**: Add `#error` directives for hard dependencies (`TEMPORAL` requires `FRAME_FEEDBACK`;
`FLOOR_DETAIL` requires `MATERIALS`). Soft dependencies use `#if` branching to select
enhanced vs. fallback paths — no compile error, just degraded quality.

**Rationale**: With 7 toggles and 128 possible combinations, silent runtime garbage from
invalid combos would be a debugging nightmare. Hard errors at compile time catch the two
invalid configurations immediately. Soft fallbacks keep the remaining combinations functional.

---

## Optimization Guidelines (Apply Throughout All Phases)

Every new shader code path added in this plan should follow these rules. The shader is
already ~77K instructions — new code must earn its place by being as tight as possible.

### FMA (Fused Multiply-Add)

Prefer explicit `fma(a, b, c)` over `a * b + c` in hot paths. On NVIDIA the compiler
usually fuses these automatically, but explicit calls guarantee it and communicate intent.
The most common pattern in this shader — `base * light + ambient` — should always be FMA.

```glsl
// Prefer:
vec3 lit = fma(base_col, vec3(ndotl), ambient);

// Over:
vec3 lit = base_col * ndotl + ambient;
```

### Bitfield Extraction

For material ID reads from `materialRows` and wall/ceiling bit checks from packed int rows,
use `bitfieldExtract()` — it maps to a single hardware BFE instruction on NVIDIA, and is
cleaner than manual shift + mask:

```glsl
// Prefer:
int mat_id = bitfieldExtract(scene.materialRows[mat_idx], mat_shift, 4);

// Over:
int mat_id = (scene.materialRows[mat_idx] >> mat_shift) & 0xF;

// For single-bit wall/ceiling checks:
bool is_wall = bool(bitfieldExtract(scene.wallRows[c.y], c.x, 1));

// Over:
bool is_wall = bool((scene.wallRows[c.y] >> c.x) & 1);
```

### Branchless / Inline Conditionals

Avoid `if/else` branches in fragment shaders where possible — GPU warps execute all
branches when threads diverge, paying the cost of both paths. Prefer arithmetic selects:

```glsl
// step(edge, x) = 0.0 if x < edge, 1.0 if x >= edge (free on most hardware)
float above_horizon = step(0.0, ray_dir.y);

// mix() as a select (compiles to a conditional move, not a branch):
vec3 col = mix(floor_col, wall_col, above_horizon);

// Clamp instead of if-guard:
float fog_density = clamp(base_density * height_falloff, 0.2, 1.0);

// max/min instead of conditional assignment:
float ao_strength = max(0.0, 1.0 - ao_dist * 2.0);  // not: if (ao_dist < 0.5) ao_strength = ...
```

For material branches (`shade_material` switch), a `if/else if` chain is unavoidable, but
keep each branch short and math-only. Do not call other functions from inside material branches.

### Reciprocal and Inverse Square Root

```glsl
// Prefer rcp (single instruction) over divide where exact precision isn't required:
float inv_dist = inversesqrt(dot(d, d));   // rsqrt — faster than 1.0/sqrt()
vec3 norm_d = d * inv_dist;               // multiply by reciprocal, not divide

// Avoid: normalize(d)  — prefer manual rsqrt when you also need the length
// float len = dot(d, d); float inv = inversesqrt(len); vec3 n = d * inv;
```

### Smooth Operations Over Hard Branches

Prefer `smoothstep` + `mix` over hard threshold comparisons for visual effects. Besides
looking better, `smoothstep` is a polynomial multiply chain that the compiler can FMA-fuse:

```glsl
// AO falloff:
float ao = mix(0.65, 1.0, smoothstep(0.0, 0.5, ao_dist));

// Bloom threshold (only bright pixels contribute):
float bright = smoothstep(0.5, 1.0, dot(col, vec3(0.2126, 0.7152, 0.0722)));
```

### Avoid Dependent Texture Reads in Hot Paths

The feedback texture (`uPrevFrame`) is sampled at screen-space UVs that are computed from
`gl_FragCoord` — these are non-dependent reads (UV known at compile time structure-wise)
and cache well. Avoid sampling the feedback texture inside loops or inside material branches
where the UV depends on a previous texture read.

### Short-Circuit Evaluation via `step` and `max`

Instead of guarding entire blocks with a branch, multiply the contribution by a step:

```glsl
// Instead of:
float volumetric = 0.0;
if (sun_dot > 0.3 && frame.uSunDir.y > 0.1) { /* expensive loop */ }

// Prefer computing the gate as a float weight and multiplying in:
float sun_gate = step(0.3, sun_dot) * step(0.1, frame.uSunDir.y);
// ... compute volumetric unconditionally or guard the loop with the float gate:
volumetric *= sun_gate;  // zero cost if gate is 0
```
*(For the volumetric loop specifically, keeping the `if` is fine since early-exit avoids
the 4 DDA steps entirely — use judgment: loops warrant branches, scalar math doesn't.)*

---

## Phase 1 — Retire CPU Sprite Billboard Path

### Rationale

The demo currently has a CPU-side sprite billboard draw path in `render_world()` (lines ~4748–5210
in `demon_hunt.c`) that draws demons, hellraisers, drones, portals, ammo, particles, and shots
as axis-aligned colored rectangles via `draw_rect_px()`. This path runs in parallel with the
shader sprite resolver (`ENABLE_SPRITE_RESOLVER 1`) and is gated behind `shader_sprite_runtime_enabled()`.

Now that Phase 3 shader sprites are shipping (`SHADER_SPRITE_PHASE3_AVAILABLE 1`) and the shader
handles all sprite types (demon, hellraiser, portal, exit pillar, player shot, ammo, particle),
the CPU billboard path is dead weight:

- ~500 lines of `draw_rect_px()` calls in `render_world()` for sprite types already handled by shader
- Duplicate visibility/LOS checks, light accumulation, perspective projection
- The F9 toggle (`g_shader_sprites_enabled`) currently disables shader sprites and reveals the CPU path
- The exit pillar still has a conditional CPU path outside the sprite loop (`!shader_sprite_phase3_runtime_enabled()`)

**Note**: The `sky_draw_cpu_world_fallback()` (flat-color column renderer, ~60 lines) is separate
and remains as the shader-link-failure fallback. It does NOT draw sprites — it only renders
flat walls, sky, and floor bands. This stays.

### What to remove

- The F9 toggle flag (`g_shader_sprites_enabled`) and its key handler at line ~5641
- All sprite billboard draw cases in `render_world()`: SPR_DEMON, SPR_HELLRAISER, SPR_HUNTER_DRONE,
  SPR_AMMO, SPR_PARTICLE, SPR_PLAYER_SHOT, SPR_DRONE_SHOT, SPR_PORTAL
- The CPU exit pillar draw block (gated behind `!shader_sprite_phase3_runtime_enabled()`)
- The `SprSort` array, `sprite_sort_compare_desc()` sort, and the entire back-to-front draw loop
- The `drone_explosions_draw()` CPU call (explosions are already particle-based in shader)
- The CPU muzzle flash rect (can be moved to a UBO flag for shader bloom)
- Helper functions used only by CPU sprites: `add_teleporter_light()`, `add_hellraiser_light()`,
  `add_player_shot_light()`, `add_drone_shot_light()` (dynamic lights are already in the SSBO for shader)

### What stays

- `sky_draw_fullscreen()` — the fullscreen shader pass (UBO + SSBO + mesh)
- `sky_draw_cpu_world_fallback()` — flat-color fallback when shader link fails
- `pack_shader_sprites()` — CPU→GPU sprite data packing (feeds SSBO)
- `upload_scene_ssbo()`, `upload_sky_frame_ubo()` — GPU data upload
- `render_world()` function itself (will just call `sky_draw_fullscreen` + HUD/minimap)
- HUD draws: crosshair, minimap, score, health bar, ammo counter, text
- `shader_sprite_runtime_enabled()` — simplify to always return true when `g_sky_ok && g_scene_ssbo_ok`
- CPU game logic: collision DDA, `cast_ray()`, demon AI, `los_to_point()`, `map_solid()`

### Actionable steps

- [x] **1.1** Catalog every `draw_rect_px()` call in `render_world()` that draws world sprites
  - Verify each sprite type has a matching shader path in `shade_sprite_opaque()` / `shade_sprite_alpha()`
  - **DONE**: All sprite types verified; hunter drone + drone shot added to shader path.
- [x] **1.2** Add `SHADER_SPRITE_HUNTER_DRONE` to the shader sprite system
  - **Decision (pre-resolved)**: Hunter drones MUST be added to the shader sprite path.
  - **DONE**: Implemented as `SHADER_SPRITE_HUNTER_DRONE = 9` (plus `SHADER_SPRITE_DRONE_SHOT`).
    Packing in `pack_shader_sprites()`, `shade_sprite_opaque()` case with hull/eye/wings/glow,
    coverage functions `hunter_drone_opaque_coverage()` / `hunter_drone_alpha_coverage()`.
- [x] **1.3** Remove the F9 key handler and `g_shader_sprites_enabled` variable
  - ✅ Variable declaration removed, F9 handler deleted, HUD text simplified.
  - `shader_sprite_runtime_enabled()` already returns `g_sky_ok && g_scene_ssbo_ok`.
- [x] **1.4** Remove the CPU exit pillar draw block *(removed with sprite loop)*
- [x] **1.5** Remove the `SprSort` array declaration and back-to-front sprite draw loop
  - ✅ All dead code removed. Orphaned block (~260 lines) deleted. Duplicate `float tt` fixed.
- [x] **1.5b** *(critical fix)* Delete orphaned dead code block — **DONE**
- [x] **1.6** Remove or inline `drone_explosions_draw()` — already gone (was in deleted block)
- [x] **1.7** Remove CPU muzzle flash rect — already gone (was in deleted block)
- [x] **1.8** Remove dead helper functions that only served CPU sprite lighting:
  - `sprite_light_mul()` — removed
  - `add_teleporter_light()`, `add_hellraiser_light()`, `add_player_shot_light()`, `add_drone_shot_light()` — already gone
- [x] **1.9** Remove `SprSort` typedef and `sprite_sort_compare_desc()` comparator *(already done)*
- [x] **1.10** Build and run: `build_examples.bat vulkan demon_hunt`
  - ✅ Build succeeds (`[SUCCESS] build\examples\demon_hunt.exe`, 20MB, 2026-06-06)
  - TODO: Visual verification (run demo, check all sprite types render)
- [ ] **1.11** Measure LOC reduction (target: 400–600 lines removed from `demon_hunt.c`)

---

## Phase 1.5 — Previous-Frame Feedback Texture

### Rationale

The single-pass fullscreen shader cannot read its own output or neighboring pixels — this
makes real bloom, chromatic aberration, screen-space reflections, and temporal accumulation
impossible. A feedback texture solves this: at end-of-frame, blit the current framebuffer
into a texture; next frame, bind that texture as a `sampler2D` in the shader.

This is a standard technique (used in every modern engine for TAA, SSR, motion blur) with
near-zero cost: one blit/copy command per frame (~0.05ms on GTX 1070) and one texture bind.

### Architecture

```
Frame N:
  1. Bind previous_frame_texture as sampler2D (set 2, binding 0)
  2. Render fullscreen quad (existing shader + feedback reads)
  3. After render pass ends: copy framebuffer → previous_frame_texture
  4. Texture is now "Frame N" for next frame's consumption
```

**API path** (validated against `situation_api.h`):
- `SituationCreateTextureEx()` with flags `SAMPLED | TRANSFER_DST` — the feedback texture
- `SituationCmdBindTextureSet(cmd, 2, feedback_tex)` — bind as set 2 in shader
- `SituationCmdCopyTexture()` or `SituationCmdBlitTexture()` — end-of-frame copy from swapchain/VD
- `SituationCmdTextureBarrier()` — transition layout between copy-dst and shader-read

**Shader side** (Vulkan):
```glsl
#if DH_ENABLE_FRAME_FEEDBACK
layout(set = 2, binding = 0) uniform sampler2D uPrevFrame;
#endif
```

**Graceful fallback**: When `DH_ENABLE_FRAME_FEEDBACK 0`, all features that read the feedback
texture fall back to their ALU-only approximations (color tint instead of CA, world-space
proximity bloom instead of screen-space bloom, DDA ray instead of SSR, etc.).

### What this unlocks for later phases

| Phase | Without feedback | With feedback |
|-------|-----------------|---------------|
| 3D (CA) | Color tint shift (fake) | Real subpixel channel offset from previous frame |
| 5D (floor reflections) | Expensive DDA reflection ray (8 steps) | Single texture read at reflected UV (free) |
| 6A (bloom) | World-space proximity check | Screen-space Kawase/Gaussian from bright prev-frame pixels |
| 6 (new: TAA) | Hard shadow edges, aliasing | Temporal blend smooths dither + edges |
| 6 (new: motion blur) | No motion blur | Blend with previous frame on fast camera turn |

### Actionable steps

- [x] **1.5.1** Create feedback texture at startup (in `init_sky_gpu()` after SSBO creation):
  ```c
  SituationImage feedback_img = {0};
  feedback_img.width = sw;  // window width
  feedback_img.height = sh; // window height
  feedback_img.channels = 4;
  feedback_img.data = NULL; // no initial data
  SituationTextureUsageFlags fb_flags = SITUATION_TEXTURE_USAGE_SAMPLED | SITUATION_TEXTURE_USAGE_TRANSFER_DST;
  SituationCreateTextureEx(feedback_img, false, fb_flags, &g_feedback_tex);
  ```
  - Handle resize: destroy + recreate if window size changes
  - Add `static SituationTexture g_feedback_tex;` and `static int g_feedback_tex_ok;`
- [x] **1.5.2** Add end-of-frame copy in `sky_draw_fullscreen()` (after `SituationCmdDrawMesh`):
  ```c
  // After the shader draws, copy result to feedback texture for next frame
  if (g_feedback_tex_ok) {
      SituationTextureBarrierDesc barrier_to_dst = {
          .old_layout = SITUATION_TEXTURE_LAYOUT_SHADER_READ,
          .new_layout = SITUATION_TEXTURE_LAYOUT_TRANSFER_DST,
          .src_access = SITUATION_ACCESS_SHADER_READ,
          .dst_access = SITUATION_ACCESS_TRANSFER_WRITE,
          .src_stage = SITUATION_PIPELINE_STAGE_FRAGMENT_SHADER,
          .dst_stage = SITUATION_PIPELINE_STAGE_TRANSFER
      };
      SituationCmdTextureBarrier(cmd, g_feedback_tex, &barrier_to_dst);
      // Copy from current render target to feedback
      SituationTextureCopyRegion region = {0, 0, sw, sh};
      SituationCmdBlitTexture(cmd, /* current framebuffer */, g_feedback_tex, &region);
      SituationTextureBarrierDesc barrier_to_read = {
          .old_layout = SITUATION_TEXTURE_LAYOUT_TRANSFER_DST,
          .new_layout = SITUATION_TEXTURE_LAYOUT_SHADER_READ,
          .src_access = SITUATION_ACCESS_TRANSFER_WRITE,
          .dst_access = SITUATION_ACCESS_SHADER_READ,
          .src_stage = SITUATION_PIPELINE_STAGE_TRANSFER,
          .dst_stage = SITUATION_PIPELINE_STAGE_FRAGMENT_SHADER
      };
      SituationCmdTextureBarrier(cmd, g_feedback_tex, &barrier_to_read);
  }
  ```
  - **NOTE (pre-resolved)**: The blit source is the VirtualDisplay approach. On Vulkan the
    swapchain image isn't directly blittable mid-render-pass, and ending/restarting a render
    pass just for a copy adds complexity. Instead:
    1. Create a VirtualDisplay as the world render target (demon hunt already renders to main window;
       switch to a VD with `SituationCreateVirtualDisplayEx` at the same resolution)
    2. Render the fullscreen shader to the VD (same draw, different target)
    3. Get the VD texture via `SituationGetVirtualDisplayTexture(vd_id, &vd_tex)`
    4. Copy VD texture → feedback texture (both are owned textures, no swapchain issues)
    5. Composite the VD to the main window (the HUD draws go directly to the main window after)
    This cleanly separates "world render" from "HUD/overlay render" and gives us a stable
    blittable source without touching the swapchain image mid-pass.
- [x] **1.5.3** Bind feedback texture before shader draw:
  ```c
  if (g_feedback_tex_ok) {
      SituationCmdBindTextureSet(cmd, 2, g_feedback_tex);
  }
  ```
- [x] **1.5.4** Add shader declaration:
  ```glsl
  #define DH_ENABLE_FRAME_FEEDBACK 0  // master toggle

  #if DH_ENABLE_FRAME_FEEDBACK
  #if defined(VULKAN)
  layout(set = 2, binding = 0) uniform sampler2D uPrevFrame;
  #else
  layout(binding = 3) uniform sampler2D uPrevFrame;  // OpenGL binding point
  #endif
  #endif
  ```
- [x] **1.5.5** Add helper to sample previous frame:
  ```glsl
  #if DH_ENABLE_FRAME_FEEDBACK
  vec3 sample_prev_frame(vec2 screen_uv) {
      return texture(uPrevFrame, screen_uv).rgb;
  }
  vec3 sample_prev_frame_offset(vec2 screen_uv, vec2 pixel_offset) {
      return texture(uPrevFrame, screen_uv + pixel_offset / frame.uResolution).rgb;
  }
  #endif
  ```
- [x] **1.5.6** Handle first frame (feedback texture is black/undefined):
  - Frame 0: shader detects `uPrevFrame` is all zeros → skip feedback reads (use fallback path)
  - Or: clear feedback texture to `FOG_COLOR` on creation so first frame has sane data
- [x] **1.5.7** Handle window resize:
  - Detect size change in `render_world()` or `sky_draw_fullscreen()`
  - Destroy old `g_feedback_tex`, recreate at new size
  - Clear to fog color to avoid garbage on resize frame
- [x] **1.5.8** Cleanup in `shutdown_sky_gpu()`: destroy feedback texture
- [x] **1.5.9** Compile and test with `DH_ENABLE_FRAME_FEEDBACK 1`:
  - Verify no GPU validation errors (Vulkan validation layers)
  - Verify the texture bind doesn't break existing set 0 (UBO) / set 1 (SSBO) binds
  - Verify first frame renders correctly (no garbage)
  - Verify no visual regression when feedback is bound but not yet used by any effect
- [x] **1.5.10** Performance baseline: measure fps with feedback infrastructure active but no reads
  - Expected cost: <0.5fps (one blit command)

---

## Phase 1.5bis — Library: SPIR-V Layout Profile with Sampler (Situation Core)

### Rationale

Phase 1.5 creates the feedback texture and attempts to bind it at descriptor set 2 via
`SituationCmdBindTextureSet(cmd, 2, g_feedback_tex)`. This crashes on Vulkan because
`SIT_SPIRV_LAYOUT_PROFILE_UBO_SSBO` only allocates sets 0 (UBO) and 1 (SSBO) — the pipeline
layout has no set 2. The library needs a new profile that includes a sampler descriptor set.

### What this adds to the library

A new enum value `SIT_SPIRV_LAYOUT_PROFILE_UBO_SSBO_SAMPLER` that creates a pipeline layout with:
- Set 0: Uniform buffer (std140 UBO)
- Set 1: Storage buffer (std430 SSBO)
- Set 2: Combined image sampler (texture, fragment stage)

Plus push constants (128 bytes, all graphics stages) for compatibility with `SituationDrawModel`
and future push-constant users.

### Actionable steps

- [x] **1.5b.1** Add enum value in `sit/situation_api.h`:
  ```c
  SIT_SPIRV_LAYOUT_PROFILE_UBO_SSBO_SAMPLER,  /* set 0 UBO, set 1 SSBO, set 2 combined image sampler */
  ```
- [x] **1.5b.2** Add `VkPipelineLayout graphics_spirv_layout_ubo_ssbo_sampler` field to the Vulkan render state struct in `situation_impl_decl.h`.
- [x] **1.5b.3** Create the 3-set pipeline layout in `_SituationVulkanInitGraphicsSpirvLayouts()`:
  - Sets: `ubo_layout`, `ssbo_layout`, `text_sampler_layout` (reuse existing — binding 0, fragment, combined image sampler)
  - Push constant range: 128 bytes, `VK_SHADER_STAGE_ALL_GRAPHICS`
- [x] **1.5b.4** Wire up pipeline creation: when `layout_profile == SIT_SPIRV_LAYOUT_PROFILE_UBO_SSBO_SAMPLER`, use `graphics_spirv_layout_ubo_ssbo_sampler` as the pipeline layout.
- [x] **1.5b.5** Wire up descriptor binding validation: add a `case SIT_SPIRV_LAYOUT_PROFILE_UBO_SSBO_SAMPLER:` that allows set 0 (UBO), set 1 (SSBO), set 2 (texture/sampler).
- [x] **1.5b.6** Handle texture binding at set 2 in `SituationCmdBindTextureSet`: when the bound shader profile is UBO_SSBO_SAMPLER and set_index == 2, allocate/update a combined image sampler descriptor and bind it via `vkCmdBindDescriptorSets`.
- [x] **1.5b.7** Destroy the new pipeline layout in `_SituationCleanupVulkan()`.
- [x] **1.5b.8** Update range checks (`if (layout_profile > SIT_SPIRV_LAYOUT_PROFILE_UBO_SSBO)`) to accept the new enum value.
- [x] **1.5b.9** Switch demon_hunt shader load to `SIT_SPIRV_LAYOUT_PROFILE_UBO_SSBO_SAMPLER`.
- [x] **1.5b.10** Uncomment the `SituationCmdBindTextureSet(cmd, 2, g_feedback_tex)` call.
- [x] **1.5b.11** Flip `DH_ENABLE_FRAME_FEEDBACK` to 1.
- [x] **1.5b.12** Build `situation_vulkan.dll`, build demon_hunt, verify no crash entering level.
- [x] **1.5b.13** Run existing Vulkan test harness — no regressions.

### Notes

- OpenGL is unaffected (SPIR-V layout profiles are ignored on the OpenGL path).
- `text_sampler_layout` is reused because it's already a single combined image sampler at binding 0 in the fragment stage — exactly what `layout(set = 2, binding = 0) uniform sampler2D uPrevFrame` needs.
- The push constant range is added for future-proofing; demon_hunt doesn't currently use push constants on the sky shader, but other shaders using this profile might.

---

## Phase 2 — Material System (SSBO Extension)

### Architecture

The map is 32×32 cells max (`MAP_MAX_W=32`, `MAP_MAX_H=32`). Currently `wallRows[32]` packs
1 bit per cell into one `int` per row (32 bits = 32 cells). For 4-bit material IDs, we need
4 bits × 32 cells = 128 bits = 4 ints per row. Total: `materialRows[128]` (or `materialRows[32][4]`
conceptually, but GLSL SSBO prefers a flat array).

**Packing layout**: `materialRows[row * 4 + (cell / 8)]` contains 8 cells × 4 bits.
Extract: `(materialRows[z * 4 + x / 8] >> ((x % 8) * 4)) & 0xF`

**Material enum (4 bits = 16 types):**

| ID | Material | Visual Character |
|----|----------|-----------------|
| 0  | Stone (default) | Rough diffuse, subtle hash-based normal perturbation |
| 1  | Metal | Specular N·H highlight, darkened diffuse, Fresnel edge glow |
| 2  | Flesh/Organic | Wrap lighting (SSS approx), warm color shift toward red |
| 3  | Emissive | Bypass shadow, additive glow, pulsing intensity |
| 4  | Wood | Warm orange-brown tint, anisotropic grain highlight along Y |
| 5  | Water/Liquid | Floor-only: animated UV distortion, tinted reflective |
| 6  | Bone | Off-white diffuse, hard specular, darkened crevices |
| 7  | Rusted Metal | Rough orange-brown specular, patchy tint variation |
| 8–15 | Reserved | Future (crystal, lava, ice, etc.) |

### Actionable steps

- [x] **2.1** Add `materialRows[128]` to `SkySceneSsboHeader` in `demon_hunt.c` (after `arch_ew_rows[32]`, before `align_pad`)
  - std430 alignment: `int[128]` after `int[32]` is trivially aligned (4-byte boundary, no padding needed)
  - `SKY_SCENE_SSBO_HEADER_BYTES` updates automatically via `sizeof(SkySceneSsboHeader)`
  - **Verification step**: Add a compile-time size check after the struct definition:
    ```c
    _Static_assert(sizeof(SkySceneSsboHeader) == /* expected bytes */,
                   "SkySceneSsboHeader size changed — update shader SSBO layout");
    ```
    Calculate expected: current `sizeof` + 128 * 4 = current + 512 bytes
  - Check that no other code uses `SKY_SCENE_SSBO_HEADER_BYTES` or hardcoded size assumptions
    (grep for the constant name and any raw byte counts matching the old size)
- [x] **2.2** Add matching `int materialRows[128]` to shader SSBO `ShaderScenePack` in `demon_hunt_sky.fs`
  - Place after `archEwRows[32]`, before `_alignPad`
  - Verify std430 alignment (ints are naturally aligned, no padding needed)
- [x] **2.3** Add `static uint8_t g_material_map[MAP_MAX_H][MAP_MAX_W]` in `demon_hunt.c`
- [x] **2.4** Add `sky_pack_material_rows()` function (parallel to `sky_pack_wall_rows`):
  ```c
  static void sky_pack_material_rows(int mat_rows[128]) {
      memset(mat_rows, 0, 128 * sizeof(int));
      for (int z = 0; z < g_map_h && z < MAP_MAX_H; z++) {
          for (int x = 0; x < g_map_w && x < 32; x++) {
              int idx = z * 4 + x / 8;
              int shift = (x % 8) * 4;
              mat_rows[idx] |= ((int)g_material_map[z][x] & 0xF) << shift;
          }
      }
  }
  ```
- [x] **2.5** Call `sky_pack_material_rows()` in `upload_scene_ssbo()` and copy into SSBO header
- [x] **2.6** Assign materials in `map_generate()` after room/corridor carving:
  - Outer border walls (x==0 || z==0 || x==map_w-1 || z==map_h-1) → Stone (0)
  - Arch cells → Stone (0) — arches are always stone
  - Random interior walls → weighted random: 50% Stone, 20% Wood, 15% Metal, 10% Rusted Metal, 5% Bone
  - Exit cell neighbors → Emissive (3)
  - Arena levels → bias toward Metal + Rusted Metal
  - Corridor floors → Stone or Wood (for floor material, if Phase 5 uses it)
- [x] **2.7** Add `#define DH_ENABLE_MATERIALS 0` toggle in shader (next to existing `DH_ENABLE_*` block)
- [x] **2.8** Add `shade_material()` function in shader, called from wall shading in `main()`:
  ```glsl
  #if DH_ENABLE_MATERIALS
  vec3 shade_material(int mat_id, vec3 base_col, float ndotl, float perp, vec3 view_dir, vec3 normal) {
      // material-specific shading modifications
  }
  #endif
  ```
- [x] **2.9** Extract material ID after DDA wall hit (in `main()` wall shading block, line ~1610):
  ```glsl
  #if DH_ENABLE_MATERIALS
  ivec2 hit_cell = ivec2(floor(wp));
  int mat_idx = hit_cell.y * 4 + hit_cell.x / 8;
  int mat_shift = (hit_cell.x % 8) * 4;
  int mat_id = (scene.materialRows[mat_idx] >> mat_shift) & 0xF;
  wcol = shade_material(mat_id, wcol, ndotl, perp, view_rd, vec3(nx, 0, nz));
  #endif
  ```
- [x] **2.10** Implement per-material shading:
  - [x] Stone: `hash12(wp * 4.0)` normal perturbation (±0.15 on ndotl)
  - [x] Metal: `pow(max(dot(reflect(-sunDir, N), V), 0.0), 32.0) * 0.6` specular + darken diffuse to 60%
  - [x] Flesh: wrap lighting `max(0, (ndotl + 0.4) / 1.4)` + red color shift
  - [x] Emissive: return bright emissive color, ignore shadow (`sl = 1.0`)
  - [x] Wood: warm tint `vec3(0.65, 0.35, 0.15)`, anisotropic `abs(sin(wp.y * 12.0)) * 0.15` added
  - [x] Bone: off-white base `vec3(0.85, 0.80, 0.72)`, hard specular power 64
  - [x] Rusted Metal: orange tint variation `hash12(floor(wp)) * 0.2`, rough specular power 8
- [x] **2.11** Compile shader with `DH_ENABLE_MATERIALS 1`: `compile_demon_hunt_shaders.bat`
  - Verify Vulkan SPIR-V compiles cleanly (OpenGL target not checked — shader is ~77K instructions, well beyond OGL limits)
- [x] **2.12** Build and run: `build_examples.bat vulkan demon_hunt`
  - Verify materials are visually distinct per wall type
  - Verify no regression on existing wall shading when `DH_ENABLE_MATERIALS 0`
- [ ] **2.13** Performance check: verify fps stays above 270 (budget: 5–8% cost)

---

## Phase 3 — Atmospheric Enhancements

### 3A — Height-gradient fog

Current fog: `exp(-dist * k)` where k is 0.06 (walls), 0.09 (floor), ~0.065 (sprites).
This is purely distance-based. Replace with height-aware fog that hugs the floor:

```glsl
float height_fog(float dist, vec3 ray_dir) {
    float base_density = 0.07;
    float height_falloff = max(1.0 - ray_dir.y * 2.5, 0.2);
    return exp(-dist * base_density * height_falloff);
}
```

Floor-looking rays (negative Y) get full fog. Upward-looking rays get reduced fog.
Sky rays (above horizon) get no world fog.

### 3B — Edge ambient occlusion (capped 2-step DDA)

After primary DDA wall hit, cast a perpendicular ray (rotated 90° from hit normal)
with a **hard cap of 2 DDA steps**. If it hits a wall within 0.5 units, darken by 20–35%.

Key constraint: the secondary ray uses the same `cell_wall()` check (reads `wallRows`)
but does NOT check arches (skip `arch_hit_interval` — too expensive for AO). Max 2 steps
means at most 2 `cell_wall()` checks = 2 int reads + 2 bit extractions. Bounded cost.

### 3C — Enhanced god rays (volumetric scatter along view ray)

Current `godray_mask()` computes a sun-cone contribution using clouds.
Enhancement: in corridors where the player can see the sun (sky visible), accumulate
a scatter term by sampling 4 points along the primary DDA ray and checking if each
point has sky visibility (no wall above). This brightens sun-aligned corridors.

Implementation: in `main()`, after `cast_prim()`, if the sun is above horizon:
```glsl
float volumetric = 0.0;
float sun_dot = max(dot(normalize(view_rd), normalize(frame.uSunDir)), 0.0);
if (sun_dot > 0.3 && frame.uSunDir.y > 0.1) {
    for (int i = 1; i <= 4; i++) {
        float sample_raw = raw * float(i) / 5.0;
        vec2 sample_pos = g_col.ro.xz + g_col.rdxz * sample_raw;
        ivec2 sc = ivec2(floor(sample_pos));
        if (sc.x >= 0 && sc.y >= 0 && sc.x < int(frame.uMapSize.x) && sc.y < int(frame.uMapSize.y)) {
            if (!cell_wall(sc)) volumetric += 0.25;
        }
    }
    volumetric *= pow(sun_dot, 3.0) * 0.12;
}
```

### 3D — Chromatic aberration on damage

When `frame.uThreatPulse > 0.0`, shift the final R and B channels by ±1–2 pixels.

**With feedback texture** (`DH_ENABLE_FRAME_FEEDBACK 1`):
Real chromatic aberration — read offset pixels from previous frame:
```glsl
if (frame.uThreatPulse > 0.0) {
    vec2 screen_uv = gl_FragCoord.xy / frame.uResolution;
    vec2 center = vec2(0.5);
    vec2 dir = normalize(screen_uv - center) * frame.uThreatPulse;
    float offset = 2.0 / frame.uResolution.x;  // 2 pixel offset
    fragColor.r = sample_prev_frame_offset(screen_uv, dir * offset).r;
    fragColor.b = sample_prev_frame_offset(screen_uv, -dir * offset).b;
}
```
This gives true radial CA emanating from screen center — exactly what damage effects look like.

**Without feedback** (`DH_ENABLE_FRAME_FEEDBACK 0`):
Color tint shift (approximation — current frame only):
```glsl
if (frame.uThreatPulse > 0.0) {
    fragColor.r *= 1.0 + frame.uThreatPulse * 0.15;
    fragColor.b *= 1.0 - frame.uThreatPulse * 0.10;
}
```

### Actionable steps

- [x] **3.1** Add `#define DH_ENABLE_ATMOSPHERE 0` toggle in shader
- [x] **3.2** Implement `height_fog()` function, replacing `exp(-dist * k)` calls:
  - Wall fog (line ~1626): `height_fog(perp, g_col.view_rd)`
  - Floor fog (line ~1493): keep distance-only (floor is already "low" by definition)
  - Sprite fog: `height_fog(hit.perp, normalize(hit.world_pos - g_col.ro))`
- [x] **3.3** Implement edge AO:
  - After wall hit in `main()` (line ~1608), before final color composite
  - Perpendicular direction logic (clarified):
    - `side_hit == 0` means the ray crossed an X boundary → wall normal is in X → cast AO in **Z direction**
    - `side_hit == 1` means the ray crossed a Z boundary → wall normal is in Z → cast AO in **X direction**
    - AO ray direction sign: cast toward the wall interior. If `side_hit == 0` and `rdxz.x < 0`
      (ray was going left, hit right side of cell), AO casts in +Z and -Z (both sides of the corner).
      Simplification: cast in BOTH perpendicular directions (±Z or ±X), take the minimum AO distance.
  - 2-step DDA: for each direction, step through at most 2 cells checking `cell_wall()`
  - If hit within 0.5 units: `wcol *= mix(1.0, 0.65, ao_strength)`
  - AO strength based on proximity: `ao_strength = 1.0 - (ao_dist / 0.5)`
  - Implementation:
    ```glsl
    float ao = 1.0;
    ivec2 ao_cell = ivec2(floor(wp));
    ivec2 ao_dir = (side_hit == 0) ? ivec2(0, 1) : ivec2(1, 0);
    for (int sign = -1; sign <= 1; sign += 2) {
        for (int step = 1; step <= 2; step++) {
            ivec2 test = ao_cell + ao_dir * (step * sign);
            if (cell_wall(test)) {
                float d = float(step) * 0.5;  // approximate distance
                ao = min(ao, mix(0.65, 1.0, d));
                break;
            }
        }
    }
    wcol *= ao;
    ```
- [x] **3.4** Implement volumetric god ray enhancement:
  - In `main()`, after `cast_prim()` returns, before branching into wall/floor/sky shading
  - 4 sample points along the primary ray
  - Add volumetric term to `shot_bloom` vec3 (reuse the existing additive path)
  - Color: warm sun tint `vec3(1.0, 0.75, 0.35) * volumetric`
- [x] **3.5** Implement damage color shift:
  - At end of `main()`, after all `fragColor` assignments
  - Gate behind `frame.uThreatPulse > 0.0`
  - Push red, suppress blue proportionally to threat_pulse
  - With frame feedback: real radial chromatic aberration from previous frame
  - Without frame feedback: fallback color tint shift
- [x] **3.6** Compile shader: `compile_demon_hunt_shaders.bat`
  - Verify Vulkan SPIR-V compiles cleanly
- [x] **3.7** Build and run: `build_examples.bat vulkan demon_hunt`
  - Verify fog looks correct (floor hugs, sky clears)
  - Verify AO darkening at wall corners
  - Verify god ray enhancement in sun-facing corridors
  - Verify damage tint when taking hits
- [ ] **3.8** Performance check: verify fps stays above 245 (budget: +8–12% over Phase 2)
  - If AO is too expensive, reduce to 1-step DDA check (single `cell_wall` read)

---

## Phase 4 — Sprite Material Enhancements

### Architecture

Sprite data is already packed into 3 vec4 rows per sprite in the SSBO (`spriteData[]`):
- `sprite_row0`: position (x, y, z, alive)
- `sprite_row1`: dimensions (half_width, height, base_y, type)
- `sprite_row2`: parameters (p0, p1, p2, p3) — type-specific data

The `p3` field (row2.w) is currently used by:
- Demon: `cos(time)` for hand bob animation
- Hellraiser: `threat_pulse` (0 or 1)
- Others: 0.0 or unused

**Strategy**: Rather than adding a material nibble to the sprite pack (which would require
changing the SSBO layout), use the existing `type` field to drive material-aware shading
directly in `shade_sprite_opaque()` / `shade_sprite_alpha()`. Each sprite type already has
its own shading branch — we enhance those branches with material-quality effects.

### Enhancements per sprite type

| Type | Enhancement | Implementation |
|------|-------------|----------------|
| Demon | Rim light from sun direction | `pow(1.0 - max(dot(N, V), 0.0), 3.0) * sunColor` on body |
| Demon | Emissive eye intensify | Scale eye emission by `1.5 + 0.5 * sin(time)` |
| Hellraiser | Full-body emissive pulse | Multiply aura by `1.0 + threat_pulse * 0.8` |
| Hellraiser | Heat distortion shimmer | UV wobble on body: `uv.x += sin(uv.y * 20 + time * 8) * 0.01` |
| Player Shot | Stronger bloom core | Increase core multiplier from 0.95 to 1.4 |
| Ammo | Pulsing emissive glow | `col *= 0.7 + 0.3 * sin(time * 3.0 + index * 1.7)` |
| Portal | Ring animation | Concentric ring pattern using `fract(length(uv - 0.5) * 4.0 - time)` |
| Exit Pillar | Vertical light shaft | Add upward gradient emissive that extends above the sprite bounds |

### Actionable steps

- [ ] **4.1** Add `#define DH_ENABLE_SPRITE_MATERIALS 0` toggle in shader
- [ ] **4.2** Demon rim light:
  - In `shade_sprite_opaque()` → `SHADER_SPRITE_DEMON` branch (line ~657)
  - Approximate view direction: `normalize(g_col.ro - hit.world_pos)`
  - Approximate sprite normal: face toward camera `vec3(sin(g_col.ray_ang), 0, cos(g_col.ray_ang))`
  - Rim term: `pow(1.0 - max(dot(N, V), 0.0), 3.0) * vec3(1.0, 0.6, 0.3) * sun_lum * 0.4`
  - Add to body/arm regions (where body or arms mask > 0)
- [ ] **4.3** Demon emissive eye boost:
  - In existing eye color computation: multiply `eye_col` by `1.2 + 0.4 * sin(frame.uTime * 5.0 + params.z)`
  - Add slight bloom contribution: `col += eye_col * eyes * 0.15`
- [ ] **4.4** Hellraiser enhanced emissive:
  - In `SHADER_SPRITE_HELLRAISER` branch
  - Scale `aura_col` by `1.0 + params.w * 0.6` (params.w = threat_pulse)
  - Add UV shimmer: offset body `uv.x` by `sin(uv.y * 16.0 + frame.uTime * 10.0) * 0.008 * params.w`
- [ ] **4.5** Player shot brighter bloom:
  - In `SHADER_SPRITE_PLAYER_SHOT` branch: increase multiplier on `gain_a * gain_b`
  - In `player_shot_bloom()`: widen the halo by reducing `smoothstep` *inner* from 0.16 to 0.12
    (widens the core-to-edge gradient, making the bloom halo larger while keeping the outer cutoff)
  - **Note**: changing the *outer* radius would make the edge softer/less distinct; we want *wider*, not *softer*
- [ ] **4.6** Ammo pickup pulsing:
  - In `SHADER_SPRITE_AMMO` branch: after final `col` computation:
  - `col *= 0.75 + 0.25 * sin(frame.uTime * 3.0 + float(hit.index) * 1.7)`
- [ ] **4.7** Portal animated rings:
  - In `SHADER_SPRITE_PORTAL` branch: replace flat `energy` region with ring pattern:
  - `float ring = smoothstep(0.02, 0.0, abs(fract(length(uv - vec2(0.5)) * 3.0 - frame.uTime * 0.8) - 0.5))`
  - Mix ring pattern into energy_col region
- [ ] **4.8** Exit pillar vertical shaft:
  - In `SHADER_SPRITE_EXIT_PILLAR` branch: add upward gradient
  - `float shaft = smoothstep(0.3, 0.9, uv.y) * 0.3 * pulse`
  - `col += pillar * shaft` (bright top, dark bottom)
- [ ] **4.9** Compile and test: `compile_demon_hunt_shaders.bat` + `build_examples.bat vulkan demon_hunt`
  - Verify each sprite type looks enhanced
  - Verify no sprites disappear or break
- [ ] **4.10** Performance check: verify fps stays above 235 (budget: +3–5%)

---

## Phase 5 — Floor & Ceiling Detail

### 5A — Floor material shading

The floor currently uses a single brown color `vec3(0.18, 0.11, 0.07)` in `shade_floor_at()`.
With the material system from Phase 2, floor cells can use the same `materialRows` data
to vary floor appearance per cell.

Floor material lookup is identical to wall material lookup but uses the floor hit cell
coordinates (already computed as `fxz` in `shade_floor_at()`).

### 5B — Water floor animation

Water-material floor cells get animated UV distortion before any shading:
```glsl
if (floor_mat == MAT_WATER) {
    fxz += vec2(sin(fxz.y * 8.0 + frame.uTime * 2.0),
                cos(fxz.x * 6.0 + frame.uTime * 1.5)) * 0.015;
    base_col = vec3(0.05, 0.12, 0.18); // dark water tint
}
```

### 5C — Ceiling for enclosed rooms

Currently no ceiling is rendered (sky shows above walls). For cells that are enclosed
(surrounded by walls on all 4 sides), render a dark ceiling when the ray points above
wall height within that cell.

**CPU detection**: In `map_generate()`, after carving, scan each empty cell and count
wall neighbors (N/S/E/W). Mark as "has ceiling" if **all 4 neighbors are walls** (not
3+ as originally proposed — the 3+ heuristic would incorrectly ceiling dead-end corridors
that are open on one side, and could mark map-border cells that happen to have 3 wall
neighbors including the boundary).

**Refined rule**: `has_ceiling = (all 4 cardinal neighbors are CELL_WALL)`. This gives
ceilings ONLY to cells fully enclosed by walls on all sides — tight 1-wide alcoves and
dead-end nooks. Open corridors (which have at least one open neighbor) stay sky-visible.

**Edge case handling**: Border cells (x==0, z==0, x==map_w-1, z==map_h-1) are always walls
and never empty, so they won't be scanned. Internal empty cells adjacent to the border
will correctly detect the border wall as a neighbor.

**Shader**: When `g_col.pct < g_col.wtop` and the cell directly above the camera (or
the cell the sky ray first enters) is marked as ceiling, render ceiling color instead
of sky.

### 5D — Floor reflections (feedback-accelerated)

For metal/water floor cells: show a reflection of the scene above.

**With feedback texture** (`DH_ENABLE_FRAME_FEEDBACK 1`):
Screen-space reflection — sample previous frame at the reflected screen coordinate:
```glsl
if (floor_mat == MAT_METAL || floor_mat == MAT_WATER) {
    // Floor hit is below horizon; reflection point is mirrored above horizon
    vec2 screen_uv = gl_FragCoord.xy / frame.uResolution;
    vec2 reflected_uv = vec2(screen_uv.x, 1.0 - screen_uv.y);  // vertical flip around horizon
    // Adjust for actual horizon position (not always center):
    float horizon_uv = g_col.mid / frame.uResolution.y;
    float dist_below = screen_uv.y - horizon_uv;
    reflected_uv.y = horizon_uv - dist_below * 0.8;  // slightly compressed reflection
    if (reflected_uv.y > 0.0 && reflected_uv.y < 1.0) {
        vec3 reflected = sample_prev_frame(reflected_uv);
        float fresnel = pow(1.0 - abs(g_col.view_rd.y), 3.0) * 0.25;
        col = mix(col, reflected, fresnel);
    }
}
```
Cost: 1 texture read per reflective floor fragment. No DDA ray. ~0.01ms total.

**Without feedback** (`DH_ENABLE_FRAME_FEEDBACK 0`):
DDA reflection ray (original approach, more expensive):
- Cast upward DDA: direction = reflect(floor_ray_dir, vec3(0,1,0)), max 8 steps
- If wall hit: blend wall color at 15% (`col = mix(col, reflected_wall_col, 0.15)`)
- Distance gate: only if `floor_dist < 6.0` (limits cost to near fragments)
- Cost: up to 8 DDA steps per reflective floor fragment

The feedback path completely eliminates the Phase 5 performance risk (the reflection DDA
was the most expensive single feature in the plan). With feedback, floor reflections are
essentially free — one texture read per fragment, same cost as a color lookup.

### Actionable steps

- [ ] **5.1** Add `#define DH_ENABLE_FLOOR_DETAIL 0` toggle in shader
- [ ] **5.2** Floor material lookup in `shade_floor_at()`:
  - Compute `ivec2 floor_cell = ivec2(floor(fxz))`
  - Extract material from `materialRows` (same formula as wall)
  - Switch base color by material: Stone=brown, Metal=dark gray, Water=dark blue, Wood=warm brown
- [ ] **5.3** Water floor UV animation:
  - If material == Water (5): apply sinusoidal UV offset to `fxz` before shadow/light computation
  - Add slight blue tint to fog color for water cells
- [ ] **5.4** Add `ceilingRows[32]` bitmask to SSBO (1 bit per cell, same packing as wallRows)
  - CPU side: scan map after generation, mark cells with **all 4 cardinal neighbors as walls**
    (NOT 3+ — see Phase 5C rationale for why the stricter rule is correct)
  - Only scan interior empty cells (skip border cells which are always walls)
  - Pack into `int ceilingRows[MAP_MAX_H]` in `SkySceneSsboHeader`
  - Add to shader SSBO layout (after `materialRows[128]`)
  - Implementation:
    ```c
    static void sky_pack_ceiling_rows(int ceiling[MAP_MAX_H]) {
        memset(ceiling, 0, MAP_MAX_H * sizeof(int));
        for (int z = 1; z < g_map_h - 1 && z < MAP_MAX_H; z++) {
            int bits = 0;
            for (int x = 1; x < g_map_w - 1 && x < 32; x++) {
                if (g_map[z][x] != CELL_WALL &&
                    g_map[z-1][x] == CELL_WALL && g_map[z+1][x] == CELL_WALL &&
                    g_map[z][x-1] == CELL_WALL && g_map[z][x+1] == CELL_WALL) {
                    bits |= (1 << x);
                }
            }
            ceiling[z] = bits;
        }
    }
    ```
- [ ] **5.5** Ceiling rendering in shader:
  - In `main()`, in the `g_col.pct < g_col.wtop` (sky region) branch
  - Check if the cell the camera is in has ceiling: `cell_ceiling(ivec2(floor(g_col.ro.xz)))`
  - If yes, render dark ceiling color `vec3(0.04, 0.035, 0.05)` with AO at edges
  - Helper: `bool cell_ceiling(ivec2 c)` reads `ceilingRows[c.y]` bit
- [ ] **5.6** Floor reflections (metal/water cells only):
  - **If `DH_ENABLE_FRAME_FEEDBACK 1`** (preferred):
    - After `shade_floor_at()` computes base color, if material is Metal(1) or Water(5):
    - Compute reflected screen UV (flip Y around horizon line)
    - `vec3 reflected = sample_prev_frame(reflected_uv)`
    - Fresnel blend: `col = mix(col, reflected, pow(1.0 - abs(view_rd.y), 3.0) * 0.25)`
    - Cost: 1 texture read per reflective floor fragment
  - **If `DH_ENABLE_FRAME_FEEDBACK 0`** (fallback):
    - Cast upward DDA: direction = reflect(floor_ray_dir, vec3(0,1,0)), max 8 steps
    - If wall hit: blend wall color at 15% (`col = mix(col, reflected_wall_col, 0.15)`)
    - Distance gate: only if `floor_dist < 6.0`
- [ ] **5.7** Compile and test: `compile_demon_hunt_shaders.bat` + `build_examples.bat vulkan demon_hunt`
  - Verify floor material variation is visible
  - Verify water floors animate
  - Verify ceiling appears in tight corridors
  - Verify reflections on metal floors
- [ ] **5.8** Performance check: verify fps stays above 215 (budget: +5–10%)
  - If reflection ray is too expensive, limit to cells within 4 units or remove entirely

---

## Phase 6 — Post-process Polish

### 6A — Screen-space bloom (feedback-powered)

**With feedback texture** (`DH_ENABLE_FRAME_FEEDBACK 1`):
Real screen-space bloom — sample previous frame at diagonal offsets (Kawase-style):
```glsl
vec3 feedback_bloom(vec2 screen_uv) {
    vec3 bloom = vec3(0.0);
    // 2-iteration Kawase: 4 diagonal samples at increasing offsets
    vec2 texel = 1.0 / frame.uResolution;
    // Iteration 1: offset 1.5 pixels
    bloom += sample_prev_frame(screen_uv + vec2(-1.5, -1.5) * texel);
    bloom += sample_prev_frame(screen_uv + vec2( 1.5, -1.5) * texel);
    bloom += sample_prev_frame(screen_uv + vec2(-1.5,  1.5) * texel);
    bloom += sample_prev_frame(screen_uv + vec2( 1.5,  1.5) * texel);
    // Iteration 2: offset 3.5 pixels
    bloom += sample_prev_frame(screen_uv + vec2(-3.5, -3.5) * texel) * 0.5;
    bloom += sample_prev_frame(screen_uv + vec2( 3.5, -3.5) * texel) * 0.5;
    bloom += sample_prev_frame(screen_uv + vec2(-3.5,  3.5) * texel) * 0.5;
    bloom += sample_prev_frame(screen_uv + vec2( 3.5,  3.5) * texel) * 0.5;
    bloom /= 6.0;  // normalize (4 full + 4 half-weight)
    // Threshold: only bloom bright pixels
    float brightness = dot(bloom, vec3(0.2126, 0.7152, 0.0722));
    bloom *= smoothstep(0.5, 1.0, brightness) * 0.35;
    return bloom;
}
```
This gives soft, physically-plausible light bleed from emissive surfaces, projectiles,
and bright sky regions. 8 texture reads, each hitting L2 cache (adjacent pixels).

**Without feedback** (`DH_ENABLE_FRAME_FEEDBACK 0`):
World-space proximity bloom (existing approach + emissive wall check):
- Keep current `player_shot_bloom()` + `drone_shot_bloom()` (world-space light proximity)
- Add `emissive_wall_bloom()`: check 4 cells around current world hit for material ID 3
- Less convincing but zero additional texture reads

**Temporal accumulation bonus**: Because the feedback texture contains the previous frame's
bloom contributions, bloom naturally accumulates over multiple frames, creating a soft
persistence effect. This makes emissive surfaces appear to "glow" with a warm falloff
that a single-frame bloom can't achieve.

### 6B — Vignette

Darken screen edges. Subtle mood enhancement for a corridor raycaster:
```glsl
float vignette = 1.0 - 0.20 * pow(length((gl_FragCoord.xy / frame.uResolution - 0.5) * 1.4), 2.0);
fragColor.rgb *= vignette;
```

**Note**: Initial strength is 0.20 (not 0.35). In a corridor FPS with already-limited FOV,
aggressive vignette feels claustrophobic rather than cinematic. The player already has
walls on both sides reducing peripheral vision — vignette should be barely noticeable.
Tune range: 0.15–0.25. If 0.20 feels too subtle after testing, bump to 0.25.

### 6C — Film grain

Subtle dither noise that breaks up banding and sells the retro aesthetic:
```glsl
float grain = (hash12(gl_FragCoord.xy + fract(frame.uTime) * 137.0) - 0.5) * 0.025;
fragColor.rgb += grain;
```

**Note**: Multiplier 137.0 is coprime with typical hash periods, preventing visible
temporal repetition patterns. Amplitude 0.025 (±1.25%) is at the threshold of perception.

### 6D — Dithered shadow softening

Replace hard shadow edges with screen-space dither. Offset the shadow ray origin by
a hash of the fragment position, creating perceptually smoother shadow boundaries:

```glsl
// In pristine_shadow(), before the DDA:
vec2 dither_offset = (vec2(hash12(gl_FragCoord.xy), hash12(gl_FragCoord.yx)) - 0.5) * 0.04;
shadow_origin += dither_offset;
```

### Actionable steps

- [x] **6.1** Add `#define DH_ENABLE_POSTPROCESS 0` toggle in shader
- [x] **6.2** Bloom implementation:
  - **If `DH_ENABLE_FRAME_FEEDBACK 1`** (preferred):
    - New function `feedback_bloom(vec2 screen_uv)`: 8 diagonal samples from previous frame
    - Threshold by luminance (only bloom pixels > 0.5 brightness)
    - Add result to fragColor after main shading: `fragColor.rgb += feedback_bloom(screen_uv)`
    - Cost: 8 texture reads (all from same texture, good cache locality)
  - **If `DH_ENABLE_FRAME_FEEDBACK 0`** (fallback):
    - Keep existing `player_shot_bloom()` + `drone_shot_bloom()` (world-space proximity)
    - Add `emissive_wall_bloom()`: check 4 cells around current world hit for material ID 3
    - If emissive: add warm glow `vec3(0.8, 0.4, 0.1) * 0.2 / dist_to_cell`
    - Add to `shot_bloom` accumulator
- [x] **6.3** Vignette:
  - Apply at end of `main()`, after all `fragColor` assignments (but before damage tint)
  - `fragColor.rgb *= 1.0 - 0.20 * pow(length((ndc) * 0.7), 2.0)`
  - Use existing `ndc` variable (already computed)
  - Tuning range: 0.15–0.25 (start conservative for corridor FPS)
- [x] **6.4** Film grain:
  - Apply after vignette
  - `fragColor.rgb += (hash12(gl_FragCoord.xy + fract(frame.uTime) * 137.0) - 0.5) * 0.025`
  - Reuse existing `hash12()` function (already defined in shader)
- [x] **6.5** Shadow dithering:
  - In `pristine_shadow()` function: add screen-space dither to shadow ray origin
  - `wp += (vec2(hash12(gl_FragCoord.xy * 0.1), hash12(gl_FragCoord.yx * 0.1)) - 0.5) * 0.03`
  - This creates per-pixel jitter that breaks up hard shadow edges
- [x] **6.6** Compile and test: `compile_demon_hunt_shaders.bat` + `build_examples.bat vulkan demon_hunt`
  - Verify vignette darkens edges without being too aggressive
  - Verify grain is subtle (barely visible, breaks banding)
  - Verify shadow edges look smoother
  - Verify bloom (if feedback enabled): soft glow halos around projectiles and emissive walls
  - Verify bloom (if feedback disabled): world-space proximity glow still works
- [ ] **6.7** Performance check: verify fps stays above 200 (budget: +3–5%)
- [ ] **6.8** If final fps is below 200, disable floor reflections (Phase 5D) first, then bloom sample count

---

## Phase 7 — Temporal Effects (Feedback-Dependent)

*Requires `DH_ENABLE_FRAME_FEEDBACK 1` from Phase 1.5. Skip entirely if feedback is disabled.*

### 7A — Temporal anti-aliasing (TAA lite)

Blend current frame with previous frame at 10–15% weight. This smooths:
- Shadow dithering from Phase 6D (shadow jitter accumulates into smooth penumbra)
- Edge aliasing on wall boundaries
- Sprite edges

```glsl
vec3 taa_blend(vec3 current, vec2 screen_uv) {
    vec3 prev = sample_prev_frame(screen_uv);
    // AABB clamp in YCoCg color space for better color-transition handling
    float cur_luma = dot(current, vec3(0.25, 0.5, 0.25));
    float prev_luma = dot(prev, vec3(0.25, 0.5, 0.25));
    // Luminance-based rejection: if previous frame's luminance differs too much, reduce blend
    float luma_diff = abs(cur_luma - prev_luma);
    float blend_weight = mix(0.12, 0.02, smoothstep(0.1, 0.5, luma_diff));
    // Color clamp: per-channel min/max (tighter than ±30% on strong color contrasts)
    vec3 clamp_lo = current - vec3(0.15);
    vec3 clamp_hi = current + vec3(0.15);
    prev = clamp(prev, clamp_lo, clamp_hi);
    return mix(current, prev, blend_weight);
}
```

**Ghosting mitigation**: The luminance-weighted blend reduction handles the case where
strong color transitions (red demon next to white muzzle flash) would bleed across
boundaries. Without per-pixel motion vectors, this is inherently limited — but the
luminance-difference-driven weight reduction plus tight per-channel clamping (±0.15
absolute rather than ±30% relative) prevents the worst color bleeding. On fast camera
rotation, `luma_diff` will be large → blend weight drops to 2% → near-zero ghosting.

### 7B — Damage motion blur

When `frame.uThreatPulse > 0.0`, increase the temporal blend weight dramatically:
```glsl
float blur_weight = frame.uThreatPulse * 0.4;  // up to 40% previous frame on max damage
current = mix(current, sample_prev_frame(screen_uv), blur_weight);
```
This creates a disorienting smear effect when taking damage — the world trails behind
your movement. Combined with chromatic aberration (3D), damage feels visceral.

### 7C — Emissive persistence (light trails)

Emissive fragments (material 3, projectiles, hellraiser auras) leave a fading trail
in the feedback loop. Because the feedback already contains previous emissive contributions,
these naturally persist. Amplify by not clamping emissive regions in TAA:

```glsl
// In TAA blend: skip neighborhood clamp for bright pixels
float brightness = dot(current, vec3(0.2126, 0.7152, 0.0722));
if (brightness > 0.8) {
    // Emissive — allow longer persistence
    current = mix(current, prev, 0.25);  // 25% history for bright areas
}
```

### Actionable steps

- [ ] **7.1** Add `#define DH_ENABLE_TEMPORAL 0` toggle (requires `DH_ENABLE_FRAME_FEEDBACK`)
- [ ] **7.2** TAA lite implementation:
  - Apply at the very end of `main()`, after all other effects (vignette, grain, CA)
  - `fragColor.rgb = taa_blend(fragColor.rgb, screen_uv)`
  - Luminance-based blend weight: high luma_diff → low blend (prevents ghosting on camera turn)
  - Per-channel AABB clamp (±0.15 absolute) — tighter than percentage-based for color safety
- [ ] **7.3** Damage motion blur:
  - Inside the `frame.uThreatPulse > 0.0` block (same as CA)
  - Blend with previous frame weighted by threat_pulse intensity
- [ ] **7.4** Emissive persistence:
  - Modify TAA blend: detect bright pixels (luminance > 0.8), use higher history weight
  - Creates natural light trails from projectiles without any special bookkeeping
- [ ] **7.5** Anti-ghosting tuning:
  - Test with fast camera rotation (mouse flick)
  - If ghosting is visible, reduce history weight from 12% to 8%
  - If edges still alias, increase to 15%
  - Emissive persistence should be noticeable but not distracting
- [ ] **7.6** Compile and test: `compile_demon_hunt_shaders.bat` + `build_examples.bat vulkan demon_hunt`
  - Verify shadows look smoother (dither accumulation)
  - Verify no ghosting on fast camera turns
  - Verify damage blur feels impactful
  - Verify projectile trails look good
- [ ] **7.7** Performance check: should be near-zero cost (1 extra texture read in TAA path)
  - TAA adds ~0 ALU, 1 texture read per fragment (already in L2 from bloom reads)

---

## Performance Budget

| Phase | Feature | Estimated Cost | Cumulative FPS (from ~300) |
|-------|---------|----------------|---------------------------|
| 1 | Retire CPU sprites | -2% CPU (removal) | ~305 (slight CPU gain) |
| 1.5 | Feedback texture infra | +0.1% (one blit/frame) | ~305 (negligible) |
| 2 | Material shading | +5–8% ALU (branch + math per wall frag) | ~280 |
| 3 | Atmosphere (fog + AO + volumetric) | +8–12% ALU (AO DDA = biggest) | ~250 |
| 4 | Sprite materials | +3–5% ALU (per-sprite math, bounded by sprite count) | ~240 |
| 5 | Floor detail + reflections (feedback) | +2–4% (texture read, NOT DDA ray) | ~230 |
| 5 | Floor detail + reflections (no feedback) | +5–10% (DDA ray, expensive) | ~220 |
| 6 | Post-process (bloom/vignette/grain) | +3–5% ALU + 8 texture reads | ~215 |
| 7 | Temporal effects (TAA/blur) | +1% (1 texture read, trivial ALU) | ~212 |

Conservative estimate with feedback: **~212fps at full feature set**. Above 200fps target.
Conservative estimate without feedback: **~205fps** (floor reflections use expensive DDA).

**Key insight**: The feedback texture makes the plan *safer* — it converts the two most
expensive features (floor reflections: 8-step DDA, bloom: radial gather) into cheap texture
reads. The performance risk drops significantly.

**Escape valves** (if performance is worse than estimated):
1. Disable TAA (Phase 7) — saves 1%
2. Reduce bloom samples from 8 to 4 — saves 1%
3. Reduce AO to 1-step DDA — saves 3–4%
4. Disable floor reflections entirely — saves 2–4% (feedback) or 5–10% (no feedback)
5. Reduce volumetric god ray samples from 4 to 2 — saves 1–2%

---

## Priority Order

| Priority | Phase | Rationale |
|----------|-------|-----------|
| P0 | 1 — Retire CPU Sprites | Prerequisite: removes dead code, simplifies rendering path |
| P0 | 1.5 — Feedback Texture | Infrastructure: unlocks cheap reflections, real bloom, CA, TAA |
| P0 | 2 — Materials | Foundation for Phase 3 (AO needs material awareness), 5 (floor materials) |
| P1 | 3 — Atmosphere | Biggest visual impact per instruction: fog + AO transform the mood |
| P1 | 4 — Sprite Materials | Makes entities pop; independent of Phases 3/5 |
| P2 | 5 — Floor Detail | Builds on Phase 2 materials; reflections are cheap with feedback |
| P2 | 6 — Post-process | Pure polish; bloom is dramatically better with feedback |
| P3 | 7 — Temporal Effects | Final polish; TAA + damage blur; requires feedback |

---

## File Impact Summary

| File | Changes |
|------|---------|
| `examples/demon_hunt.c` | Phase 1: remove ~500 lines of CPU sprite draws. Phase 1.5: create feedback texture, add blit command, handle resize. Phase 2: add `g_material_map`, `sky_pack_material_rows()`, material assignment in `map_generate()`, expand `SkySceneSsboHeader`. Phase 5: add `ceilingRows` packing. |
| `examples/demon_hunt_sky.fs` | Phase 1.5: add `sampler2D uPrevFrame` + helper functions. All other phases: material shading, atmosphere, sprite enhancements, floor detail, post-process, temporal. Each gated behind `#define DH_ENABLE_*`. |
| `examples/demon_hunt_sky_frame.h` | Phase 3 (optional): add fog density/toggle UBO fields if runtime control desired. |
| `compile_demon_hunt_shaders.bat` | No changes needed (same compile flow). |

---

## Dependencies Between Phases

```
Phase 1 (retire CPU sprites)
    └─ no blockers, standalone

Phase 1.5 (feedback texture)
    └─ no blockers (can be done before or after Phase 1)
    └─ unlocks: Phase 3D (real CA), Phase 5D (cheap reflections),
                Phase 6A (real bloom), Phase 7 (temporal)

Phase 2 (materials)
    └─ requires: SSBO layout change (rebuild DLL not needed — SSBO is dynamic)
    └─ blocks: Phase 5 (floor materials) uses materialRows

Phase 3 (atmosphere)
    └─ requires: nothing for fog/AO/volumetric
    └─ 3D (CA): enhanced by Phase 1.5 (real CA), works without (color tint fallback)

Phase 4 (sprite materials)
    └─ requires: nothing (uses existing sprite data)
    └─ independent of Phases 2/3

Phase 5 (floor detail)
    └─ requires: Phase 2 (materialRows in SSBO)
    └─ 5D (reflections): enhanced by Phase 1.5 (texture read), works without (DDA fallback)
    └─ ceiling: requires ceilingRows addition to SSBO

Phase 6 (post-process)
    └─ 6A (bloom): enhanced by Phase 1.5 (real screen-space), works without (proximity-based)
    └─ 6B–6E: independent (vignette, grain, shadow dither need nothing)

Phase 7 (temporal)
    └─ HARD requires: Phase 1.5 (cannot function without feedback texture)
    └─ optional enhancement: Phase 6D (shadow dither looks much better with TAA)
```

---

## Compile-Time Define Interaction Matrix

With 7 `DH_ENABLE_*` defines, certain combinations are invalid. The shader must enforce
these at compile time with `#error` directives to prevent silent runtime garbage.

### Valid combinations

| Define | Depends on | Notes |
|--------|-----------|-------|
| `DH_ENABLE_MATERIALS` | (none) | Standalone |
| `DH_ENABLE_ATMOSPHERE` | (none) | Standalone (CA enhanced by FRAME_FEEDBACK but works without) |
| `DH_ENABLE_SPRITE_MATERIALS` | (none) | Standalone |
| `DH_ENABLE_FLOOR_DETAIL` | `DH_ENABLE_MATERIALS` | Floor material lookup reads `materialRows` |
| `DH_ENABLE_FRAME_FEEDBACK` | (none) | Infrastructure toggle |
| `DH_ENABLE_POSTPROCESS` | (none) | Bloom enhanced by FRAME_FEEDBACK but proximity fallback works without |
| `DH_ENABLE_TEMPORAL` | `DH_ENABLE_FRAME_FEEDBACK` | **Hard dependency** — TAA reads `uPrevFrame` |

### Required `#error` guards (add at top of shader, after all `#define` lines)

```glsl
#if DH_ENABLE_TEMPORAL && !DH_ENABLE_FRAME_FEEDBACK
#error "DH_ENABLE_TEMPORAL requires DH_ENABLE_FRAME_FEEDBACK — TAA reads uPrevFrame"
#endif

#if DH_ENABLE_FLOOR_DETAIL && !DH_ENABLE_MATERIALS
#error "DH_ENABLE_FLOOR_DETAIL requires DH_ENABLE_MATERIALS — floor shading reads materialRows"
#endif
```

### Soft dependencies (degrade gracefully, no `#error`)

| Consumer | Enhancer | Behavior without enhancer |
|----------|----------|--------------------------|
| Phase 3D (CA) | `DH_ENABLE_FRAME_FEEDBACK` | Falls back to color tint shift |
| Phase 5D (reflections) | `DH_ENABLE_FRAME_FEEDBACK` | Falls back to DDA reflection ray |
| Phase 6A (bloom) | `DH_ENABLE_FRAME_FEEDBACK` | Falls back to world-space proximity bloom |
| Phase 6D (shadow dither) | `DH_ENABLE_TEMPORAL` | Dither is visible per-frame but not accumulated |

### Implementation

Each soft dependency uses `#if DH_ENABLE_FRAME_FEEDBACK` inside the consuming phase's
code block to select the enhanced path vs. the fallback. Example pattern:

```glsl
#if DH_ENABLE_FLOOR_DETAIL
    // ... floor material shading ...
    #if DH_ENABLE_FRAME_FEEDBACK
        // feedback-accelerated reflection (1 texture read)
        vec3 reflected = sample_prev_frame(reflected_uv);
    #else
        // DDA fallback reflection (8 steps, expensive)
        vec3 reflected = dda_reflect(floor_ray_dir, ...);
    #endif
#endif
```

---

## Validation Protocol

After each phase:

1. **Compile**: `compile_demon_hunt_shaders.bat` — Vulkan SPIR-V passes clean (OpenGL target not validated — shader is ~77K instructions and cannot run on OpenGL)
2. **Build**: `build_examples.bat vulkan demon_hunt` — no compile/link errors
3. **Run**: Launch demo, play through 2–3 levels visually verifying new features
4. **FPS**: Check overlay (F10) — must stay above per-phase target
5. **Toggle test**: Set new `DH_ENABLE_*` to 0, recompile, verify no regressions

---

## Risk Register

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| SSBO layout change breaks shader alignment | Medium | High | Add `_alignPad` ints after materialRows; verify with `offsetof()` prints |
| AO DDA causes frame time spikes in open rooms | Low | Medium | 2-step hard cap is bounded; worst case = 2 reads per fragment |
| Floor reflection DDA combined with AO = 3 DDAs/frag | Low (feedback) / Medium (no feedback) | High | Feedback path eliminates DDA entirely; fallback path distance-gated |
| OpenGL cannot run the shader at all | Confirmed | None | Shader is ~77K instructions — OpenGL is expired for this demo. Vulkan-only, no mitigation needed. |
| Hunter Drone shader sprite implementation | Medium | Medium | Pre-resolved: committed to adding `SHADER_SPRITE_HUNTER_DRONE` in Phase 1 step 1.2. Hull/eye/wing/glow pattern mirrors existing hellraiser structure. |
| Material assignment makes mazes look too busy | Low | Low | Conservative palette: 50% Stone ensures visual coherence |
| Feedback texture source (swapchain blit) needs render pass split | Low (pre-resolved) | Low | VirtualDisplay approach pre-selected: render world to VD, copy VD tex → feedback, composite VD to main window. No swapchain access needed. |
| TAA ghosting on fast camera rotation | Medium | Low | Luminance-driven blend weight (12% → 2% on large luma_diff) + per-channel ±0.15 AABB clamp. No motion vectors available — inherent limitation, but acceptable for a slow-moving FPS. |
| Feedback texture undefined on first frame | Low | Low | Clear to FOG_COLOR on creation; shader detects black and skips |
| Window resize invalidates feedback texture dimensions | Low | Medium | Detect size change, destroy + recreate texture, clear to fog color |
| Vulkan validation errors from texture layout transitions | Medium | High | Follow the barrier pattern exactly; test with `VK_LAYER_KHRONOS_validation` |
| Descriptor set 2 conflicts with other shader binds | Low | High | Demon hunt shader currently uses set 0 (UBO) + set 1 (SSBO) only; set 2 is free |
