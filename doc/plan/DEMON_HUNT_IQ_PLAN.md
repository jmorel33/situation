# Demon Hunt IQ Plan

**Date**: 2026-05-18  
**Status**: Phase 3 enabled (portal, exit pillar, player shots, particles in shader); F9 toggles full shader vs CPU fallback  
**Priority**: High — visual correctness, occlusion quality, prop-demo credibility  
**Scope**: `examples/demon_hunt.c`, `examples/demon_hunt_sky.fs`

---

## Objective

Migrate Demon Hunt's sprite-style world entities from CPU-projected 2D quads into the shader raycast core.

The current renderer is split:

- The shader owns walls, floor, sky, arches, teleporter pads, fog, world lighting, and map occlusion.
- The CPU projects demons, Hellraisers, ammo, particles, player shots, portals, and the exit pillar as 2D rectangles after the shader pass.

That split limits visual intelligence. CPU quads can be sorted and line-of-sight filtered, but they cannot be partially occluded by the same wall, arch, and corridor geometry the shader sees per pixel. The goal is one visual authority: for each pixel, the shader decides what the camera ray hit first.

---

## Core Contract

CPU remains responsible for gameplay state:

- AI, pathfinding, collision, damage, pickups, scoring, spawning, timers, and audio triggers.
- Entity transforms and gameplay flags.
- Packing a compact render list for the shader each frame.

Shader becomes responsible for world-space presentation:

- Wall/floor/sky/arch ray hits.
- Billboard primitive hits.
- Per-pixel sprite occlusion against walls and arches.
- Shared fog, lighting, glow, shadows, and future effects.

No gameplay logic moves into GLSL. Only visual hit testing and shading move there.

---

## Current Sprite Sources

CPU sprite-like rendering currently lives in `render_world()` after `sky_draw_fullscreen()`:

- `SPR_DEMON`
- `SPR_HELLRAISER`
- `SPR_AMMO`
- `SPR_PARTICLE`
- `SPR_PLAYER_SHOT`
- `SPR_PORTAL`
- `SPR_TELEPORTER`
- Exit pillar/glow is not in `SprType`, but is also CPU-projected and drawn with `draw_rect_px()`.

The final teleporter pad branch is already shader-owned, so teleporter pads should remain in the floor shader path. Any teleporter vertical effect added later can use the same billboard primitive system.

---

## Target Shader Model

Add shader-side billboard primitives that are ray-tested against the same camera ray used for the world:

1. Build the camera ray from `gl_FragCoord`, `uResolution`, `uFlatInvVP`, and `uCamPos`.
2. Run the existing `cast_prim()` world ray to get wall/arch hit distance.
3. Ray-test active billboard primitives.
4. Choose the nearest sprite hit with distance less than the nearest wall/arch distance.
5. Shade that sprite pixel.
6. If no sprite wins, keep the existing wall/floor/sky result.

This makes sprite visibility naturally correct around wall edges, arch stone, corridor turns, and floor/wall transitions.

---

## Render Data Layout

Start with uniforms, not textures or SSBOs. Demon Hunt has bounded counts and already uses uniform arrays for teleporters, Hellraisers, and player shots. Use `MAX_SHADER_SPRITES = 256` for the first implementation so the shader feed can cover the current active gameplay set without immediately redesigning transport.

Suggested compact shader struct, represented as parallel `vec4` arrays for GLSL portability:

```glsl
uniform vec4 uSprites0[MAX_SHADER_SPRITES]; // x, y, z, active
uniform vec4 uSprites1[MAX_SHADER_SPRITES]; // half_width, height, base_y, type
uniform vec4 uSprites2[MAX_SHADER_SPRITES]; // param0, param1, param2, param3
uniform int  uSpriteCount;
uniform int  uSpriteDebugMode;
```

Meaning:

- `x/y/z`: world-space anchor or center.
- `active`: `0.0` means skip.
- `half_width`: billboard half-width in world-ish units.
- `height`: billboard visual height.
- `base_y`: bottom anchor for grounded sprites, or center bias for particles/projectiles.
- `type`: integer encoded as float for demon, Hellraiser, ammo, portal, particle, projectile, exit pillar.
- `param*`: type-specific values such as hurt flash, life, brightness, open/closed state, pulse seed, or color variant.
- `uSpriteDebugMode`: `0` means normal rendering, nonzero values keep the sprite uniforms reachable for validation/debug views.

The first pass should keep this deliberately simple and fixed-size. CPU packing and shader arrays both use 256 entries: `SHADER_SPRITE_MAX = 256` in C and `MAX_SHADER_SPRITES = 256` in GLSL. If the sprite system grows beyond this prototype transport, revisit with texture-buffer or SSBO support later. Until then, packing should use a deterministic priority order instead of silently overflowing: enemies, exit pillar, portals, player shots, ammo, then nearest/brightest particles.

---

## Billboard Hit Test

Each sprite is a vertical camera-facing plane:

- Center at `sprite.xz`.
- Horizontal axis faces the camera using a right vector derived from the camera-to-sprite direction or the current camera basis.
- Vertical axis is world Y.
- Ray/plane intersection yields `t`.
- Convert hit point into local billboard coordinates.
- Reject if outside `[-half_width, half_width]` and `[base_y, base_y + height]`.

Depth comparison:

- Use the same raw/perpendicular distance convention as the wall path.
- A sprite hit is visible only if its distance is in front of the current wall/arch hit.
- If the world ray hits only floor/sky, sprite hits can still win if they are valid and positive.

The first implementation can use rectangular masks matching the current visual style. Later passes can add per-type silhouette functions.

---

## Shading Strategy

Start with parity, then improve.

### Phase 1 Shading: Parity Rectangles

Implement simple procedural masks that mimic current rectangle art:

- Demon body, eyes, core, arms, aura.
- Hellraiser body, horns, eyes, aura.
- Ammo block and core.
- Player shot glow/core.
- Portal monolith/glow/energy.
- Exit pillar/glow.
- Particles as soft or square pips.

Use the existing lighting helpers:

- `maze_shadow()`
- `pristine_shadow()`
- `point_shadow()`
- `player_shot_light()`
- `hellraiser_light()`

Then apply the same fog used by walls/floor so sprites sit inside the scene instead of pasted on top.

### Alpha and Coverage Contract

Shader sprites need an explicit coverage model because the current CPU art layers opaque rectangles, translucent aura/glow rectangles, and tiny particle/projectile cores.

- `shade_sprite()` should return premultiplied or straight color plus alpha/coverage deliberately, not just "a hit happened".
- Empty mask pixels must return zero coverage so the world or a farther sprite remains visible.
- Opaque body/core pixels can fully win the ray if they are closer than the wall/arch hit.
- Translucent aura, glow, and particle pixels should composite over the already-resolved background color instead of acting like solid occluders.
- If multiple translucent sprite pixels overlap on the same ray, the first implementation may resolve nearest-first with alpha compositing; revisit ordered multi-layer accumulation only if artifacts become visible.
- Prototype work may start with a mostly opaque ammo or player-shot core, but the composition path should be shaped for alpha from the beginning.

### Phase 2 Shading: Better Primitives

Once parity works, improve individual types:

- Soft sprite edges without texture sampling.
- Shape masks for demon silhouettes.
- Portal distortion/glow.
- Hellraiser heat/haze or red rim light.
- Projectile bloom that respects occlusion.
- Particles that fade into fog and lighting.

---

## Migration Phases and Actionables

### Phase 0: Prepare Data Without Visual Change

**Status**: Complete.

Goal: build and validate the shader sprite feed while keeping all existing CPU sprite drawing active.

Completed on 2026-05-18:

- Added the 256-slot shader sprite constants and CPU packing arrays in `examples/demon_hunt.c`.
- Added `pack_shader_sprites()` and packed demons, Hellraisers, portals, exit pillar, player shots, ammo, and capped lowest-priority particles.
- Uploaded `uSpriteCount`, `uSpriteDebugMode`, `uSprites0`, `uSprites1`, and `uSprites2` in `sky_draw_fullscreen()`.
- Preserved existing lighting feeds: `uPlayerShots`, `uHellraisers`, and `uTeleporters`.
- Added matching shader constants/uniforms and a reachable `uSpriteDebugMode` path in `examples/demon_hunt_sky.fs`.
- Kept CPU sprite rendering and `SprSort` behavior unchanged.
- Verified with lints and a successful OpenGL `demon_hunt` example build.

C-side actionables:

- [x] Add constants near the existing sprite/entity declarations:
  - `#define SHADER_SPRITE_MAX 256`
  - `#define SHADER_SPRITE_DEMON 1`
  - `#define SHADER_SPRITE_HELLRAISER 2`
  - `#define SHADER_SPRITE_AMMO 3`
  - `#define SHADER_SPRITE_PARTICLE 4`
  - `#define SHADER_SPRITE_PLAYER_SHOT 5`
  - `#define SHADER_SPRITE_PORTAL 6`
  - `#define SHADER_SPRITE_EXIT_PILLAR 7`
- [x] Add CPU packing arrays with exact shader layout:
  - `float g_shader_sprites0[SHADER_SPRITE_MAX][4]`
  - `float g_shader_sprites1[SHADER_SPRITE_MAX][4]`
  - `float g_shader_sprites2[SHADER_SPRITE_MAX][4]`
  - `int g_shader_sprite_count`
  - `int g_shader_sprite_debug_mode`
- [x] Add `shader_sprite_reset()` that clears count and zeroes only the active range needed.
- [x] Add `shader_sprite_push(type, x, y, z, half_width, height, base_y, p0, p1, p2, p3)`.
- [x] Make overflow deterministic: if all 256 slots are full, skip lower-priority late particles first and keep gameplay-critical sprites packed.
- [x] Add a single `pack_shader_sprites()` call before `sky_draw_fullscreen()` uploads uniforms.
- [x] Pack all existing CPU-visible world sprite sources, even before the shader renders them:
  - Demons: position, hurt flash, alive flag, pulse seed.
  - Hellraisers: position, active flag, spawn freeze or pulse seed.
  - Ammo: position, active flag, hover seed.
  - Particles: position, life, RGB/brightness.
  - Player shots: position, active flag, travel fade.
  - Portals: position, alive flag, pulse seed.
  - Exit pillar: generated exit position and gate-open state.
- [x] Do not pack teleporter floor pads as sprites. They are already shader floor features.
- [x] Upload `uSpriteCount`, `uSprites0[i]`, `uSprites1[i]`, and `uSprites2[i]` in `sky_draw_fullscreen()`.
- [x] Upload `uSpriteDebugMode` in `sky_draw_fullscreen()` and keep it defaulted to `0`.
- [x] Preserve existing lighting uniforms during this phase, especially `uPlayerShots`, `uHellraisers`, and `uTeleporters`.
- [x] Keep all CPU sprite rendering and `SprSort` behavior unchanged in this phase.

Shader actionables:

- [x] Add `MAX_SHADER_SPRITES` and matching sprite type constants in `demon_hunt_sky.fs`.
- [x] Add uniforms:
  - `uniform int uSpriteCount;`
  - `uniform int uSpriteDebugMode;`
  - `uniform vec4 uSprites0[MAX_SHADER_SPRITES];`
  - `uniform vec4 uSprites1[MAX_SHADER_SPRITES];`
  - `uniform vec4 uSprites2[MAX_SHADER_SPRITES];`
- [x] Add a reachable debug-only function, gated by `uSpriteDebugMode`, that can count active sprites or tint one test pixel. This prevents GLSL from optimizing away sprite uniforms while normal rendering remains unchanged when the toggle is `0`.

Validation:

- [x] Shader compiles with arrays declared.
- [x] Sprite uniforms are not optimized out when debug mode is enabled.
- [ ] The title/log can temporarily report `g_shader_sprite_count` during play if needed.
- [x] CPU visuals are unchanged.
- [x] No fullscreen or virtual-display presentation behavior changes.

Exit criteria:

- Shader receives the same logical set of world sprites the CPU would draw.
- No visible rendering changes yet.

### Phase 1: Shader Sprite Prototype

**Status**: Ammo prototype implemented; in-game visual validation still pending.

Goal: prove one simple sprite type can be ray-hit, depth-tested, and shaded in the shader.

Prototype type selected: `SHADER_SPRITE_AMMO`.

Completed on 2026-05-18:

- Added `g_shader_sprites_enabled`, defaulting enabled, with an `F9` runtime toggle and HUD status text.
- Uploaded `uShaderSpritesEnabled` to `demon_hunt_sky.fs`.
- Suppressed CPU ammo sprite drawing only while shader sprites are enabled.
- Added `SpriteHit`, ammo billboard hit testing, `cast_sprites()`, ammo coverage, ammo shading, and sprite/world compositing in `demon_hunt_sky.fs`.
- Kept the old CPU ammo path available through the `F9` toggle for side-by-side comparison.
- Preserved existing wall/floor lighting uniforms and helpers.
- Verified no linter errors and a successful OpenGL `demon_hunt` example build.

Implementation details now in place:

- Sprite hit distance uses raw XZ ray distance for wall/arch comparison and perpendicular distance for fog-like effects.
- Ammo is the only shader-rendered sprite type in Phase 1; all other sprite types remain CPU-rendered.
- `composite_sprite()` resolves world color first, then blends the covered ammo pixel over it using alpha/coverage.
- CPU line-of-sight filtering still exists only on the CPU fallback path. Shader ammo visibility is governed by per-pixel ray/world depth.
- The successful C build does not replace the remaining interactive check that the runtime-loaded GLSL compiles and looks correct in the game window.

C-side actionables:

- [x] Add a runtime debug toggle, `g_shader_sprites_enabled`, default on for the ammo prototype and toggleable with `F9`.
- [x] Add a second toggle or compile guard to suppress CPU drawing only for the prototype type.
- [x] Keep CPU line-of-sight filtering available only as a temporary comparison aid, not as final shader visibility.
- [x] Keep sprite packing independent from CPU sort order. Shader order is resolved by distance.

Shader actionables:

- [x] Add a `SpriteHit` helper representation:
  - `float t`
  - `float perp`
  - `int index`
  - `int type`
  - `vec2 uv`
  - `vec3 world_pos`
  - `float coverage`
- [x] Add `bool ray_billboard_hit(...)`:
  - Inputs: camera origin, ray direction, forward/right basis, sprite data.
  - Output: hit distance and local coordinates.
  - Reject inactive sprites.
  - Reject sprites behind camera.
  - Reject local X outside `[-half_width, half_width]`.
  - Reject local Y outside `[base_y, base_y + height]`.
- [x] Add `SpriteHit cast_sprites(vec3 ro, vec3 rd, vec2 fwd, float max_raw)`:
  - Iterate all sprites.
  - Hit-test active sprites.
  - Shade/test coverage for rectangle hits and keep the nearest covered sprite pixel closer than the wall/arch hit.
  - If a nearer rectangle hit produces zero coverage at this pixel, continue testing farther sprites/world.
- [x] Add `vec4 shade_sprite(SpriteHit hit, vec3 rd, float wall_perp)` with only the prototype type implemented, where `.a` is meaningful coverage.
- [x] Add `vec4 composite_sprite(vec4 world_color, SpriteHit hit)` so opaque sprite pixels can replace world color while glow/aura pixels blend over it.
- [x] In `main()`, after `cast_prim()`, call `cast_sprites()`.
- [x] Resolve world color first, then composite any closer sprite hit according to coverage/alpha.

Distance actionables:

- [x] Decide and document whether sprite `t` is raw XZ distance or full ray distance.
- [x] Convert to the same comparison convention used by `cast_prim()` before comparing against wall/arch `raw`.
- [x] Use perpendicular distance for fog and scale-like effects, matching wall rendering.

Validation:

- [ ] Prototype sprite disappears fully behind a wall.
- [ ] Prototype sprite clips partially at wall edges.
- [ ] Prototype transparent/glow pixels blend over the world instead of creating rectangular solid occluders.
- [ ] Prototype sprite does not draw through arches unless the ray passes through open arch space.
- [ ] Prototype sprite remains sharp in windowed and fullscreen.
- [x] CPU and shader versions can be toggled for side-by-side behavioral comparison.

Exit criteria:

- One simple sprite type is correctly shader-rendered and occluded.
- No shader compile failures at runtime.
- No fullscreen scaling/sharpness regression.

### Phase 2: Core Enemy Migration

**Status**: Demon and Hellraiser shader paths implemented behind a CPU-default fallback, but Phase 2 occlusion validation failed.

Goal: move demons and Hellraisers into shader rendering because they are the most important occlusion cases.

Completed on 2026-05-18:

- Extended shader sprite coverage and hit testing to `SHADER_SPRITE_DEMON` and `SHADER_SPRITE_HELLRAISER`.
- Added `shade_demon_sprite()` with aura, body, eyes, core, arm masks, hurt flash, fog, and existing scene light helpers.
- Added `shade_hellraiser_sprite()` with aura, tall body, horn, eye, core masks, pulse, fog, and existing scene light helpers.
- Added shared shader-side sprite lighting using sun shadow, teleporter light, Hellraiser light, and player-shot light.
- Suppressed CPU demon and Hellraiser quad insertion only while `g_shader_sprites_enabled` is active.
- Kept the old CPU enemy paths available through the `F9` fallback toggle.
- Updated HUD status text to report `Enemies/ammo: shader` or `Enemies/ammo: CPU`.
- Verified no linter errors and a successful OpenGL `demon_hunt` example build.

Stabilization update:

- `g_shader_sprites_enabled` now defaults to `0`, so startup uses the known-good CPU sprite path.
- `F9` opt-in enables the experimental shader sprite path after the game is running.
- CPU packing and shader arrays remain 256 entries as required by the Phase 0/1 contract.
- If startup still fails with the CPU-default fallback, investigate base shader compile/link separately from sprite rendering.

Validation result:

- The shader path currently reproduces the CPU sprite quirks instead of solving them.
- Enemy sprites still pop and bleed over walls/surroundings in the cases this refactor was intended to fix.
- Root cause: Phase 2 migrated billboard drawing, but did not yet implement a full shader-side visibility resolver. The shader needs explicit per-pixel depth ordering, world-depth rejection, and alpha/coverage composition rules for every candidate sprite pixel.
- Therefore, Phase 2 is code-complete but not behavior-complete. Do not proceed to Phase 3 until Phase 2.1 passes occlusion validation.

C-side actionables:

- [x] Pack demon data:
  - `pos = {x, 0.5, z}`
  - `half_width`
  - `height`
  - `base_y`
  - `hurt_flash`
  - `alive`
  - stable seed/index
- [x] Pack Hellraiser data:
  - `pos = {x, 0.58, z}`
  - larger `half_width` and `height`
  - `active`
  - `spawn_freeze` or pulse seed
- [x] Keep all AI, damage, melee, pathfinding, score, and death logic unchanged.
- [x] Preserve `uPlayerShots`, `uHellraisers`, and `uTeleporters` lighting feeds while migrating enemy visuals. Do not delete or rename these lighting paths until equivalent replacement lighting is verified.
- [x] Disable CPU quad drawing for demons only after shader demon parity is acceptable.
- [x] Disable CPU quad drawing for Hellraisers only after shader Hellraiser parity is acceptable.
- [x] Keep CPU shadow/glow rectangles until equivalent shader shading exists, then remove.

Shader actionables:

- [x] Implement `shade_demon_sprite()`:
  - Body mask.
  - Eye mask.
  - Core mask.
  - Arm/hand masks if needed.
  - Aura mask.
  - Hurt flash from `param0`.
- [x] Implement `shade_hellraiser_sprite()`:
  - Tall body mask.
  - Horn masks.
  - Eye masks.
  - Red aura.
  - Pulse based on time and seed.
- [x] Apply `sprite_light` equivalent in GLSL:
  - Directional term from camera/sprite normal approximation.
  - `pristine_shadow()` or `maze_shadow()` for sun occlusion.
  - `player_shot_light()`, `hellraiser_light()`, and teleporter point light contribution.
- [x] Keep wall/floor lighting behavior unchanged while sprite visuals move. The IQ refactor should improve occlusion without dimming projectile, Hellraiser, or teleporter light already visible on geometry.
- [x] Apply fog using the same `fog_mix()` convention as walls/floor.
- [x] Ensure transparent/empty parts of the sprite mask return "no sprite pixel" so world behind shows through.

Occlusion actionables:

- [x] Test enemies half-hidden behind wall corners.
- [x] Test enemies inside/behind arch cells.
- [ ] Test close enemies against near-plane behavior.
- [x] Test multiple enemies overlapping each other. Nearest covered sprite pixel should win per ray.

Validation:

- [ ] Demons no longer pop through walls or arch stone. **Failed in current Phase 2 shader path.**
- [ ] Hellraisers are visually threatening but correctly occluded. **Failed in current Phase 2 shader path.**
- [ ] Hurt flash and eye glow still read clearly.
- [ ] Enemy lighting remains plausible.

Exit criteria:

- Demons and Hellraisers are no longer drawn by CPU quads while shader sprites are enabled.
- Their shader versions are occlusion-correct and gameplay behavior is unchanged.

### Phase 2.1: Shader Visibility Resolver

**Status**: Implemented in code and enabled by default. `F9` controls shader/CPU comparison.

Goal: replace the Phase 2 "shader-port of CPU rectangles" with a real per-pixel visibility resolver so the shader can decide what is in front, what is behind, and what should be composited.

This phase is about correctness first, not prettier art. The expected result is that shader-rendered ammo, demons, and Hellraisers no longer bleed over walls, arch stone, nearby geometry, or each other.

C-side actionables:

- [x] Keep `SHADER_SPRITE_MAX = 256` and `MAX_SHADER_SPRITES = 256`.
- [x] Set `g_shader_sprites_enabled` defaulting to `1` so the shader path is active by default while `F9` remains the CPU fallback.
- [x] Keep the `F9` CPU/shader comparison toggle.
- [x] Preserve the packed render list, but document its priority order as feed order only, not visibility order.
- [x] Add optional debug display/logging for:
  - packed sprite count,
  - shader-sprite enabled state,
  - selected resolver mode if multiple modes are tested.
- [x] Preserve existing gameplay state and lighting feeds: `uPlayerShots`, `uHellraisers`, and `uTeleporters`.
- [x] Do not migrate more sprite types until this phase passes.

Shader actionables:

- [x] Replace "nearest rectangle hit" with a per-pixel candidate resolver:
  - iterate all active sprite records,
  - ray-test the billboard plane,
  - compute local UV,
  - compute hard coverage and soft/emissive coverage separately,
  - reject candidates behind the wall/arch raw distance,
  - keep enough nearest candidates to resolve opaque and translucent ordering.
- [x] Separate sprite material results into:
  - `opaque_coverage`,
  - `alpha_coverage`,
  - `emissive/glow`,
  - `depth_raw`,
  - `depth_perp`,
  - shaded color.
- [x] Treat body/core/limbs as depth-owning opaque or mostly opaque coverage.
- [x] Treat aura/glow as non-depth-owning emissive/alpha that cannot by itself make a rectangular billboard block the world.
- [x] Compose against world color after world shading, but before final output:
  - reject all sprite pixels with `depth_raw >= world_raw`,
  - resolve nearest opaque sprite first,
  - composite translucent/glow contributions front-to-back or nearest-first with explicit alpha rules,
  - ensure empty mask pixels continue to reveal world or farther sprite pixels.
- [x] Add a small fixed candidate buffer if needed, for example nearest opaque plus nearest one or two translucent/glow hits. Keep loops bounded.
- [x] Ensure overlapping sprites are depth-resolved per pixel rather than relying on CPU `SprSort` order.
- [x] Ensure wall/arch depth wins before any sprite pixel shades.
- [x] Preserve existing floor/wall lighting behavior while changing only sprite visibility/composition.

Current state:

- Shader sprites are active by default and the `F9` CPU fallback remains available.
- Phase 2.2 made shader sprites feel closer to the CPU path: ammo hover, demon hover/hand/eye pulse, Hellraiser pulse, and demon translucent surround are now shader-driven.
- The remaining blocker is not migration coverage or animation, but visual correctness of occlusion. Phase 3 stays blocked until wall-corner, arch-stone, and sprite-overlap cases are visibly correct.

Implementation notes:

- `SpriteHit` now carries separate `opaque_coverage` and `alpha_coverage`.
- `SpriteResolve` keeps the nearest opaque candidate and nearest alpha/glow candidate independently.
- Demon/Hellraiser aura no longer participates in hard depth ownership.
- `composite_sprites()` resolves world color first, then applies nearest opaque coverage, then applies a constrained nearest alpha/glow contribution.
- The OpenGL example build passes after this resolver change.
- Shader validation fix: renamed procedural cloud helper `noise2()` to `noise2d()` because `noise2` conflicts with GLSL built-in/reserved noise overloads on the runtime compiler. `glslangValidator -S frag examples\demon_hunt_sky.fs` now passes.
- Resolver enablement: `ENABLE_SPRITE_RESOLVER = 1`, `SHADER_SPRITE_RESOLVER_AVAILABLE = 1`, and `g_shader_sprites_enabled = 1`. The shader path is active by default, and `F9` toggles back to the CPU path for comparison.
- Added explicit `init_sky_gpu` progress logging around shader source loading, compile/link, and mesh creation so future hangs identify the failing stage.
- Host fallback fix: added `SHADER_SPRITE_RESOLVER_AVAILABLE` and `shader_sprite_runtime_enabled()` so `F9` only suppresses CPU demons, Hellraisers, or ammo when the GLSL resolver is actually available. This prevents mismatches where particles render but enemies disappear.
- Current verification: `glslangValidator -S frag examples\demon_hunt_sky.fs` passes, lints pass, and the OpenGL `demon_hunt` example builds.
- Corner-bleed correction: each billboard pixel now requires line-of-sight from the billboard-plane hit point back to the sprite anchor via `point_shadow(hit_xz, center)`. This rejects card pixels that are depth-front of the wall but disconnected from the sprite by maze geometry.
- Post-animation verification: the shader path now looks much better overall, but user testing still reports that occlusion is not nailed. Treat further art work as secondary until the resolver behavior is proven.
- Stronger occlusion correction: sprite-card visibility now uses `segment_world_visible()` from the exact billboard sample position back to the sprite center at that sample height. This checks both solid wall cells and arch stone, replacing the older 2D wall-only `point_shadow()` card test for sprite pixels.
- Added `F8` sprite debug cycling. Mode `2` shows a full-screen occlusion overlay: opaque sprite hits are red, alpha/aura hits are purple, and world depth remains visible in the background. This is intended for wall-corner and arch-stone diagnosis while keeping `F9` as the CPU/shader rendering comparison.

Finalization actionables:

- [ ] Runtime-test startup with the resolver enabled by default.
- [ ] Use `F9` to capture direct CPU/shader comparisons for the same wall-corner and arch cases.
- [x] Add a temporary occlusion debug mode under `uSpriteDebugMode` that can visualize:
  - world raw depth,
  - sprite raw depth,
  - alpha-only aura pixels.
- [x] Audit `cast_prim()` raw distances against `ray_billboard_hit()` raw distances so wall/arch and sprite candidates are compared in the same distance space.
- [x] Add stricter per-pixel card visibility for large billboards via a 3D sample-to-center segment test that includes arch stone.
- [ ] Verify arch-stone depth specifically. Arch openings can be correct while arch stone still fails if the sprite resolver only sees the final wall distance and not the nearer arch interval.
- [ ] Verify alpha aura clipping separately from opaque body clipping. Aura should be visible around exposed demon pixels, but it must not leak around wall corners or through arch stone.
- [ ] If the sample-to-center test is still insufficient for large billboards, shrink the hard sprite hit extents at wall contact while keeping alpha soft.
- [ ] If startup hangs again, use the new `init_sky_gpu` log checkpoints to identify whether the stall is source loading, shader compile/link, mesh creation, or first frame.
- [ ] If shader mode starts but occlusion still fails, focus only on resolver ordering, raw depth agreement, arch interval handling, and alpha coverage clipping before improving art.
- [ ] Keep Phase 3 blocked until the validation checklist below passes.

Validation:

- [ ] Demons behind wall corners clip at the wall edge instead of bleeding through.
- [ ] Hellraisers behind arch stone are hidden by the stone and visible only through open arch space.
- [ ] Multiple enemies overlapping resolve by true per-pixel depth.
- [ ] Aura/glow no longer creates rectangular wall bleed.
- [ ] Ammo still works under the same resolver.
- [ ] `F9` comparison clearly shows shader occlusion improving over CPU sprites.
- [ ] Windowed/fullscreen sharpness remains unchanged.
- [ ] OpenGL build succeeds and runtime shader compile/link succeeds.

Exit criteria:

- Phase 2 enemy migration is not only code-present, but visibly occlusion-correct.
- Shader path is good enough to become the default again.
- Phase 2.2 animation parity may proceed in parallel with visibility tuning, but Phase 3 may begin only after both Phase 2.1 and Phase 2.2 validation items pass.

### Phase 2.2: Animation and Oscillator Parity

**Status**: First implementation complete and visually improved; runtime comparison still required before Phase 3.

Goal: pass the CPU-side animation timing and oscillator-driven state into the shader so shader-rendered sprites preserve the same motion, threat rhythm, and mood as the CPU path.

Original regression:

- Shader sprites use ad-hoc `uTime` sine waves.
- CPU sprites use richer timing from `g_bob`, per-entity indices, hurt flash, ammo hover timing, Hellraiser warning/active oscillator cadence, and audio/gameplay event rhythm.
- Result: shader sprites can be spatially migrated but feel less animated and less ominous than the CPU versions.

C-side actionables:

- [x] Add global shader animation uniforms:
  - `uBobPhase` from `g_bob`,
  - `uThreatPulse` from the Hellraiser oscillator cadence,
  - optional `uMusicPulse` or `uMeloStep` from the melody oscillator if useful for visual rhythm.
- [x] Preserve and upload `uTime` for continuous shader effects, but do not rely on it as the only animation driver.
- [x] Pack per-sprite animation params in `uSprites2` consistently:
  - Demons: `hurt_flash`, hover phase, eye/core phase, hand/arm phase.
  - Hellraisers: `spawn_freeze`, hover phase, stable seed/index, oscillator-derived threat pulse.
  - Ammo: active flag, hover phase, respawn/visibility state.
- [x] Derive CPU-side pulse values from existing oscillator state where the CPU path already uses oscillators:
  - `HELLRAISER_OSC_ID`,
  - `MELO_OSC_ID` if visual music sync is desired,
  - `g_bob` for player/camera-linked bob.
- [x] Keep `F9` comparison available so animation parity can be judged against the CPU path.
- [x] Do not remove CPU animation code until shader animation parity is accepted.

Shader actionables:

- [x] Add matching uniforms:
  - `uniform float uBobPhase;`
  - `uniform float uThreatPulse;`
  - optional `uniform float uMusicPulse;`
- [x] Replace arbitrary per-type `sin(uTime * rate + seed)` calls with CPU-fed phase/pulse data where parity matters.
- [x] Demon shader animation:
  - restore hover rhythm,
  - restore hand/arm bob feel,
  - preserve hurt flash/eye/core pulse readability.
- [x] Hellraiser shader animation:
  - use oscillator-driven warning/active pulse,
  - preserve ominous red aura/eye cadence,
  - keep spawn-freeze or warning-state visual emphasis.
- [x] Ammo shader animation:
  - restore hover/pulse parity with the CPU path.
- [x] Keep all animation changes independent from occlusion logic so visibility bugs and timing bugs can be tested separately.

Implementation notes:

- `examples/demon_hunt.c` now uploads `uBobPhase`, `uThreatPulse`, and `uMusicPulse` every frame.
- Demon sprite params now carry CPU-packed hover, eye/core, and hand/arm phases instead of leaving all motion to ad-hoc shader time.
- Hellraiser sprite params now carry a hover phase plus oscillator-derived threat pulse.
- Ammo sprite params now carry a hover phase so shader hover is stable and comparable via `F9`.
- `examples/demon_hunt_sky.fs` applies those phases to billboard vertical offsets, demon arm masks, demon eye/core intensity, Hellraiser red pulse, and ammo hover.
- Demon translucent surround was restored after user testing showed the shader version had lost too much of the CPU aura mood. The shader now uses broader alpha-only purple coverage and a higher alpha ceiling while preserving the contract that aura/glow must not own hard depth.
- Current verification: `glslangValidator -S frag examples\demon_hunt_sky.fs` passes, lints pass, and `build_examples.bat opengl demon_hunt` succeeds.

Validation:

- [ ] With `F9`, ammo hover cadence matches or intentionally improves on CPU hover.
- [ ] Demon hover, hand bob, eyes, and hurt flash are at least as readable/ominous as the CPU path.
- [ ] Hellraiser warning/active pulse feels synced to the existing oscillator-driven threat cadence.
- [ ] Shader path no longer feels visually static compared with CPU sprites.
- [ ] Occlusion tests from Phase 2.1 still pass after animation data is introduced.
- [ ] OpenGL shader compile/link remains stable.

Exit criteria:

- Shader sprites preserve the CPU path's animation intent while gaining shader-side occlusion.
- The shader path is visually better, not merely a less animated port of the CPU rectangles.
- Phase 3 may begin only after animation parity and visibility validation both pass.

### Phase 3: Object and Effect Migration

**Status**: First implementation is code-present but compile-gated off after driver link OOM; runtime remains on the stable enemy/ammo shader path plus CPU effects.

Goal: move remaining world sprite primitives into the shader and retire the CPU sprite sort.

C-side actionables:

- [x] Pack ammo boxes with active flag and hover seed.
- [x] Pack player shots with travel fade and active flag.
- [x] Pack particles with life, color, and brightness.
- [x] Pack portals with alive flag and pulse seed.
- [x] Pack exit pillar with gate-open state and pulse seed.
- [x] Decide whether any teleporter vertical effect is needed. Keep floor pad rendering in shader floor path.
- [ ] Remove migrated types from `SprSort` only after each type is shader-equivalent and the Phase 3 shader path links reliably.

Shader actionables:

- [x] Implement `shade_ammo_sprite()`:
  - Small grounded block.
  - Bright core.
  - Optional hover/pulse.
- [x] Implement `shade_player_shot_sprite()` behind `ENABLE_PHASE3_SPRITES`:
  - Core plus glow.
  - Fade by travel/max distance.
  - Respect occlusion against walls/arches.
- [x] Implement `shade_particle_sprite()` behind `ENABLE_PHASE3_SPRITES`:
  - Small soft square or disk.
  - Fade by life.
  - Color from packed params.
- [x] Implement `shade_portal_sprite()` behind `ENABLE_PHASE3_SPRITES`:
  - Monolith body.
  - Trim/highlight.
  - Pulsing energy core.
  - Aura mask.
- [x] Implement `shade_exit_pillar_sprite()` behind `ENABLE_PHASE3_SPRITES`:
  - Closed/open color state.
  - Glow controlled by gate-open state.
- [x] Add per-type helper functions instead of one monolithic branch.

Implementation notes:

- `ray_billboard_hit()` can accept all packed Phase 3 world sprite types when `ENABLE_PHASE3_SPRITES` is enabled, but that macro is currently `0`.
- Player shots, particles, portals, and the exit pillar have shader helpers implemented but compile-gated out for stability.
- Driver failure observed after enabling the full Phase 3 shader path: `OpenGL: GLSL shader program linking failed ... fatal error C9999: out of memory - internal malloc failed`.
- Stabilization response: set `ENABLE_PHASE3_SPRITES = 0` in `examples/demon_hunt_sky.fs` and `SHADER_SPRITE_PHASE3_AVAILABLE = 0` in `examples/demon_hunt.c`.
- CPU drawing for portals, particles, player shots, and the exit pillar remains active while Phase 3 shader effects are gated off. `F9` still toggles the stable shader enemy/ammo path.
- Teleporter floor pads remain shader floor features, so no teleporter vertical sprite was added.
- Current verification: `glslangValidator -S frag examples\demon_hunt_sky.fs` passes, lints pass, and `build_examples.bat opengl demon_hunt` succeeds.

Next actionables:

- [ ] Reintroduce Phase 3 types one at a time, starting with the cheapest type, to identify whether the OOM is caused by total shader size, branch count, alpha/opaque expansion, or a specific helper.
- [ ] Prefer splitting the resolver into compile-time feature slices instead of linking every experimental type at once.
- [ ] Keep CPU drawing as the fallback for any Phase 3 type whose shader path is disabled.
- [ ] Do not proceed to Phase 4 cleanup until the OpenGL runtime links reliably with the intended Phase 3 subset.

Validation:

- [ ] Ammo does not render through walls.
- [ ] Player shots disappear behind corners and still light visible surfaces.
- [ ] Existing projectile and Hellraiser lighting still affects walls/floors even if their visible sprite primitive is hidden behind geometry.
- [ ] Particles do not bleed through occluders.
- [ ] Portal monoliths respect corridor geometry.
- [ ] Exit pillar appears only when visible through the maze.

Exit criteria:

- No CPU-projected world sprites remain.
- `SprType`, `SprSort`, `sprite_sort_compare_desc()`, and CPU sprite draw branches are removed or reduced to unused fallback code.

### Phase 4: Cleanup, Organization, and IQ Pass

**Status**: Safe organization slice started. Destructive cleanup remains blocked until Phase 3 shader effects link reliably and CPU fallbacks can be retired.

Goal: simplify the renderer after migration and prepare for future fancy effects.

C-side actionables:

- [ ] Delete obsolete CPU sprite projection helpers if no longer used.
- [ ] Keep `project_point()` only if still used by HUD/minimap/debug features.
- [ ] Remove `los_to_point()` calls that existed only to hide CPU sprites.
- [x] Consolidate shader uniform upload loops for sprite arrays.
- [x] Add comments separating gameplay state from render packing state.
- [ ] Update `UPDATELOG.md` after behavior is verified.

Shader actionables:

- [x] Organize sprite code into clear sections:
  - Sprite constants/uniforms.
  - Hit testing.
  - Per-type masks.
  - Per-type shading.
  - Sprite/world composition.
- [ ] Remove duplicated or unused lighting code.
- [ ] Ensure all sprite helpers have bounded loops and predictable branch cost.
- [ ] Keep all arrays fixed-size for now.

Implementation notes:

- Added `upload_shader_sprite_uniforms()` in `examples/demon_hunt.c` so sprite uniform upload is no longer embedded directly inside `sky_draw_fullscreen()`.
- Added a render-packing boundary comment before `pack_shader_sprites()` to clarify that packed records are render data, not gameplay simulation.
- Sectioned `examples/demon_hunt_sky.fs` into shared helpers, sprite masks/coverage, visibility resolver, lighting/shading, composition, and debug overlays.
- Kept CPU sprite projection, sorting, and fallback draw branches intact because Phase 3 world effects are currently guarded by `ENABLE_PHASE3_SPRITES = 0` after the OpenGL driver link OOM.
- Current verification: `glslangValidator -S frag examples\demon_hunt_sky.fs` passes, lints pass, and `build_examples.bat opengl demon_hunt` succeeds.

IQ enhancement actionables:

- [ ] Add soft silhouette edges where useful.
- [ ] Add portal-specific glow/distortion after occlusion is stable.
- [ ] Add Hellraiser rim light or heat shimmer.
- [x] Add projectile bloom that remains occlusion-correct.
- [ ] Add shadowed billboard variants if they improve readability.

Projectile bloom notes:

- Implemented in the stable shader path using existing `uPlayerShots`, not the Phase 3 sprite path.
- Bloom is ray-proximity based and rejects shots behind the current wall/arch raw depth.
- Bloom also checks `segment_world_visible(uCamPos, shot)` so it does not intentionally light through maze blockers.
- This is an IQ enhancement only; broader sprite/wall occlusion validation remains unfinished.
- Current verification: `glslangValidator -S frag examples\demon_hunt_sky.fs` passes, lints pass, and `build_examples.bat opengl demon_hunt` succeeds.

Validation:

- [ ] World visuals are unified in the shader core.
- [ ] CPU-side world rendering is reduced to shader uniform packing plus UI/overlay commands.
- [ ] Minimap and UI remain command-buffer overlays.
- [ ] Windowed and fullscreen remain sharp and correctly scaled.

Exit criteria:

- Demon Hunt's world renderer has one visual authority.
- Future visual effects can build on shader ray/sprite hits instead of CPU overlay hacks.

### Phase 5: Configurable Sky Director

**Status**: Planned. Do not begin until the stable shader path is organized enough that sky work will not worsen the current sprite/occlusion complexity.

Goal: make the sky a fully configurable part of the Demon Hunt visual identity instead of a mostly fixed backdrop. The sky should be driven by a small host-side `SkyConfig`/`SkyPreset` and rendered entirely in `demon_hunt_sky.fs`.

C-side actionables:

- [ ] Add a `SkyConfig` or `SkyPreset` struct owned by gameplay/session setup, not hard-coded scattered uniforms.
- [ ] Upload explicit sky uniforms from `sky_draw_fullscreen()` or a dedicated `upload_sky_uniforms()` helper.
- [ ] Allow level archetypes to choose sky presets: normal hunt, arena, long walk, Hellraiser/void, sunset, night, storm, black-hole event.
- [ ] Keep defaults identical or close to the current look until presets are intentionally selected.
- [ ] Keep sky settings visual-only. Sun/moon mood may affect lighting uniforms, but it must not change gameplay simulation.

Shader actionables:

- [ ] Configure sun direction, color, intensity, disc size, halo size, and horizon contribution.
- [ ] Configure moon direction, color, intensity, disc size, crescent/fullness mode, and optional halo.
- [ ] Configure sky color gradient: zenith, horizon, nadir/fog color, exposure, saturation, and sunset warm band.
- [ ] Add sunset controls: sun elevation blend, orange/pink horizon ramp, dusk desaturation, and optional dark upper-sky fade.
- [ ] Add cloud controls:
  - Sparsity/coverage.
  - Cloud type: wispy, streaked, storm, high haze, broken puffs.
  - Scale, speed, direction, softness, opacity, and height illusion.
  - Primary/secondary cloud colors and shadow tint.
- [ ] Add godray controls: strength, decay, sample count or cheap approximation mode, angular width, color, noise breakup, and wall/arch occlusion awareness where affordable.
- [ ] Add an ultra-large skydome black-hole spiral option:
  - World/direction anchor so it feels fixed in the sky.
  - Event horizon radius, accretion-disc radius, spiral twist, rotation speed, lensing strength, rim color, core darkness, and star/dust streak density.
  - Preset knobs for "distant omen" versus "dominant sky threat."
  - Must remain a sky effect, not a gameplay gravity system.

Validation:

- [ ] Existing default sky remains visually stable when all new sky options use default values.
- [ ] Each preset can be toggled or assigned to a test level without recompiling.
- [ ] Sun/moon/cloud changes do not break wall, floor, sprite, projectile, Hellraiser, or teleporter lighting uniforms.
- [ ] Godrays and black-hole spiral do not obscure gameplay readability in corridors.
- [ ] Shader compile/link remains reliable on the OpenGL target.

Exit criteria:

- Sky presentation is data-driven, with clear presets and tunable uniforms.
- Sun, moon, color mood, sunset, clouds, godrays, and black-hole spiral are independent enough to combine without one-off shader edits.

---

## Non-Goals

- Do not move gameplay simulation into GLSL.
- Do not introduce texture assets for sprites yet.
- Do not add SSBO or texture-buffer plumbing until uniform arrays become a real limit.
- Do not mix this with fullscreen/presentation changes.
- Do not remove the virtual display path.

---

## Risks

- Uniform pressure: `MAX_SHADER_SPRITES = 256` is acceptable for the current OpenGL target, but large future sprite counts may need another transport.
- Shader complexity: `demon_hunt_sky.fs` is becoming a real renderer and will need organization.
- Distance mismatch: sprite distance must be compared in the same convention as wall/arch distance.
- Alpha/coverage mismatch: translucent aura, glow, particles, and projectiles must blend over resolved world color instead of becoming rectangular hard occluders.
- Lighting regression risk: the existing `uPlayerShots`, `uHellraisers`, and `uTeleporters` feeds already drive wall/floor lighting and must survive visual sprite migration.
- Shape parity: rectangle-art parity should come before fancy silhouettes.
- Vulkan warning: Demon Hunt remains an OpenGL validation target until the known Vulkan projection issue is resolved.

---

## Execution Order

Do not implement all sprite types at once. The safe order is:

1. **Data packing only** — no visual change.
2. **One simple shader sprite** — player shot or ammo.
3. **One enemy** — demon first, Hellraiser second.
4. **Remaining pickups/effects** — ammo, shots, particles, portal, exit pillar.
5. **CPU sprite removal** — only after shader parity per type.
6. **IQ effects** — only after occlusion correctness is stable.

Each phase should be independently buildable and playable. If a phase fails, keep the CPU sprite path for unmigrated types and fix only the failing type.

---

## Implementation Guardrails

- Keep `960x600` virtual display presentation untouched.
- Keep fullscreen/windowed behavior untouched.
- Keep CPU gameplay state authoritative.
- Keep game lifecycle reset paths centralized. A fresh run must set level/session state before generating level-dependent maps, so title start, game-over restart, pause restart, and completed-hunt restart cannot reuse stale level state.
- Keep the level designer scalable and structured. The game targets a 100-level progression with normal hunts, arena beats every 3 levels, and long-walk beats every 9 levels instead of relying on a tiny fixed level table or unshaped randomness.
- Keep new enemy archetypes playable in CPU fallback first, then migrate them to the shader only after gameplay, readability, and difficulty are accepted.
- Keep each sprite type independently toggleable during migration.
- Keep `uSpriteDebugMode` available until shader-side rendering is stable enough that uniform reachability is no longer a concern.
- Keep existing shader lighting uniforms and helpers alive until replacement lighting is implemented and visually verified.
- Prefer simple rectangle masks before silhouette artistry.
- Use existing map/ray helpers for occlusion instead of inventing a parallel visibility system.
- Do not rely on CPU sort order once a type is shader-rendered.
- Remove CPU sprite drawing gradually, not in one large deletion.

---

## Validation Checklist

- Starting a new hunt from title begins at level 1 with a level-1 map.
- Starting a new hunt from game over begins at level 1 with a level-1 map, regardless of the level where the player died.
- Continuing after a win advances exactly one level; restarting after the final win starts a fresh level-1 hunt.
- Level progression supports 100 levels without hand-authoring every level.
- Hunt levels broaden map dimensions, grid density, room ranges, and teleporter count over time while leaving room for deterministic special beats.
- Arena levels appear every 3 levels, except where the 9-level long-walk beat takes priority.
- Long-walk levels appear every 9 levels and give the player a readable destination run instead of another room-grid hunt.
- Hunter drones follow a two-level off/two-level on cadence: levels 1-2 have no drones, 3-4 have drones, 5-6 have no drones, 7-8 have drones, and so on.
- Sprites disappear fully behind solid walls.
- Sprites partially clip behind wall edges.
- Sprites interact correctly with arches.
- Close sprites do not pop through geometry.
- Distant sprites fog consistently with walls/floor.
- Hellraisers remain visible and readable when unobstructed.
- Player shots illuminate the world and render only when visible.
- Portal and exit pillar effects respect corridor occlusion.
- Minimap/UI remain unaffected.
- Windowed/fullscreen sharpness remains fixed.

---

## Final Desired State

Demon Hunt's renderer should answer one question per pixel:

**What does the camera ray hit first: sky, floor, wall, arch, or sprite primitive?**

Once the shader owns that answer, the demo becomes much smarter visually and gives us a foundation for richer effects without fighting CPU overlay artifacts.
