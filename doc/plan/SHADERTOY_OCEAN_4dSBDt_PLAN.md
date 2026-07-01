# Realistic Ocean Shader Lab Plan (Shadertoy inspiration trilogy)

**Date:** 2026-06-26  
**Status:** G2 realism pass landed — GL/VK link OK; G0 mood PNGs + O-16 ref-GPU gate + perf bar remain  
**Priority:** Medium — visual demo / shader-lab credibility  
**Lead inspiration (user):** [**`4sXGRM`**](https://www.shadertoy.com/view/4sXGRM) — catalog **oceanic**; **primary quality bar** for water realism, motion, and overall cohesion.

**Visual references (watch + notes only — no source paste):**

| Priority | ID | URL | Role |
| -------- | -- | --- | ---- |
| **★ Primary** | **`4sXGRM`** | [shadertoy.com/view/4sXGRM](https://www.shadertoy.com/view/4sXGRM) | **Hero reference** — best overall ocean: colour, waves, sky–sea integration, believability |
| Secondary | **`4dSBDt`** | [shadertoy.com/view/4dSBDt](https://www.shadertoy.com/view/4dSBDt) | **Enscape Cube** — Thomas Schander; cinematic atmosphere / framing |
| Secondary | **`MdXyzX`** | [shadertoy.com/view/MdXyzX](https://www.shadertoy.com/view/MdXyzX) | Fast open-water **engineering** — tiered height raymarch, golden-angle waves |

**Scope:** Original Situation **shader lab** example — **MIT-licensed original GLSL**, synthesising ideas from **all three** references plus `shader_lab_raytrace2`, targeting **greater realism** than the inspirations. **Not a port. Not a reimplementation.**

---

## Objective

Build a **new** ocean / seascape demo that Situation owns end-to-end: original code, MIT-clean, dual-backend (OpenGL + Vulkan), interactive camera — using **`4sXGRM` + `4dSBDt` + `MdXyzX`** only as **creative and technical inspiration**.

**Synthesis intent:**

| Reference | Primary gift to our demo |
| --------- | ------------------------ |
| **`4sXGRM`** ★ | **North star** — overall water believability, colour depth, wave read, foam/glint, horizon merge, **cloud–sea relationship** (study *what* makes it feel “best”) |
| **`4dSBDt`** | Mood, framing, sky–horizon cohesion, **clouds / atmosphere**, “premium viz” art direction |
| **`MdXyzX`** | **Performance craft** — cheap multi-scale waves, tiered height ray march, Fresnel + sky reflect; calm-water limit case when amplitudes → 0 |

**Success criterion:** A viewer who loves **`4sXGRM`** says *“same league, but more physically grounded”* — not a pixel copy of any reference. The demo must feel like a **journey**: clouds worth looking at, water that works **calm or lively**, and **camera travel** as a first-class mode.

**Deliverable:** **`examples/21_ocean_realistic/`** — **Example 21**, **Advanced Shader** category (see [`DIGESTIBLE_EXAMPLES_PLAN.md`](DIGESTIBLE_EXAMPLES_PLAN.md) §21). `main.c` + embedded original GLSL 450 + per-folder `README.md` + **inspiration attribution** (not license inheritance).

**Build:**

```bat
build\build_examples.bat static-opengl 21_ocean_realistic
build\build_examples.bat static-vulkan 21_ocean_realistic
```

**Promotion path:** Implement under `examples/other/` only if iterating; **ship** as numbered `21_ocean_realistic/` (not Tier 6 `other/`).

### Design pillars (user @ 2026-06-26)

| Pillar | Requirement | Implication |
| ------ | ----------- | ----------- |
| **Clouds** | **Important** — not an afterthought sky gradient | v1 budgets fragment time for volumetric or layered procedural clouds; water reflections must read cloud shapes |
| **Water mood** | **Waves are nice; calm water is equally interesting** | `uSeaState` supports **calm ↔ choppy** continuum; calm mode stresses mirror reflections, micro-ripple normals, colour depth — not flat blue |
| **Camera travel** | **Supported** — not orbit-only | Scripted **path playback** (time-based position + orientation) plus optional user override; travel sells scale and sky–sea composition |

**Default experience:** Launch into a **slow camera travel** over **calm-to-moderate** water under a **cloudy** sky; user can crank waves, grab orbit, or pause travel.

**Non-goals (v1):**

- No verbatim or line-derived Shadertoy GLSL in the repo.
- No library API changes (`sit/` untouched unless a proven blocker).
- No Shadertoy multipass / `iChannel` emulator.
- No wrapper-language ports in v1.

---

## Example placement — **21 · Advanced Shader**

| Field | Value |
| ----- | ----- |
| **Number** | **21** |
| **Folder** | `examples/21_ocean_realistic/` |
| **Category** | **Advanced Shader** — fullscreen fragment showcase (clouds + ocean + travel); beyond Tier 4 **13** (lit mesh) and Tier 6 raytrace link |
| **Tier label** | Advanced Shader *(extends numbered set after Tier 5 capstone 20)* |
| **Replaces** | `DIGESTIBLE_EXAMPLES_PLAN` “future **21** platformer” slot — platformer stays Tier 6 / later number |
| **Index** | Row in `examples/README.md` quick-build table + §21 in digestible plan |

**Relation to other shader demos:**

| Example | Role |
| ------- | ---- |
| **13** `shader_lab_3d` | Entry 3D + lighting (mesh) |
| **16** `hot_reload` *(planned)* | Shader iteration workflow |
| **21** `ocean_realistic` | **Flagship advanced FS** — ray/dir march, clouds, water moods, camera travel |
| Tier 6 `shader_lab_raytrace2` | Stay in `other/` — jewel-box raytrace; 21 is the **numbered** ocean flagship |

Use `examples/shared/sit_example.h` for universal hotkeys (ESC, F11, F9, F12) where compatible; custom travel/orbit host is expected (same class as `shader_lab_raytrace2`).

---

## OpenGL fragment instruction budget (~65k)

Many **OpenGL** drivers enforce a **~65 535 fragment instruction** limit per program (an API/driver constraint — not tied to a specific GPU). Cloud march + tiered water + calm/choppy branches can blow this if written carelessly. **Vulkan/SPIR-V has no equivalent cap** — the VK build uses a heavier megashader tier.

**Gate (blocks G1 on OpenGL):** `SituationLoadShaderFromMemory` **links** on OpenGL without driver “instruction limit exceeded” / compile fail. Record outcome @ **O-16**.

| Strategy | Purpose |
| -------- | ------- |
| **Tiered quality** | Fewer cloud steps @ default; `[` `]` cloud density trades quality vs cost |
| **`uRenderScale`** | 0.5× internal res — fewer pixels, same shader |
| **Calm path cheap** | `chop < ε` → skip foam / extra wave octaves / heavy crest math |
| **Shared helpers** | One `skyColor()`, one `sampleClouds()` — avoid duplicated march loops |
| **Unroll discipline** | Fixed small `for (i=0; i<4; i++)` loops; no dynamic iteration explosion |
| **Dead code** | No debug branches left in ship FS string |
| **Vulkan parity** | **Intentionally diverge** — GL budget tier vs VK megashader tier (see README §Shader tiers) |

**If OpenGL fails link @ v1 scope:**

1. Reduce `CLOUD_STEPS` / `SEA_MARCH_STEPS` constants (document in README).
2. Move worst cloud self-shadow loop to a cheaper 2D horizon layer only.
3. Log driver + `SituationGetLastErrorMsg()` in NOTES — do not silently ship Vulkan-only.

**Stretch:** After link, query instruction count if driver exposes `GL_PROGRAM_INSTRUCTIONS` / ARB pipeline stats (optional O-17).

---

## Copyright & creative policy (agreed @ 2026-06-26)

| Rule | Rationale |
| ---- | ----------- |
| **Do not paste** Shadertoy Image-tab source into the repo | Shadertoy uploads are typically **CC BY-NC-SA**; Situation examples ship under **MIT** |
| **Do not** chase visual parity with someone else’s shader | Avoid derivative-work arguments; we are not cloning |
| **Do** watch the reference offline (browser) and take **notes** on techniques | Ideas (Fresnel, height-field tracing, sky gradient) are not copyrightable |
| **Do** write **fresh GLSL** — extend `shader_lab_raytrace2` ocean blocks or replace entirely | Provenance stays Situation-original |
| **Do** attribute in README — all three URLs, authors where known; *“visual inspiration only”* | Courtesy + transparency; not a license transfer |
| **Optional:** mood-board **screenshots only** (`reference_4sXGRM_mood.png`, `reference_4dSBDt_mood.png`, `reference_MdXyzX_mood.png`) | Dev comparison; do not redistribute Shadertoy **source** |

**Gate:** Phase 1 coding is blocked until **O-4** (policy acknowledged in example header) is checked.

---

## Source reference (mood board — not a spec)

Treat all three user-specified URLs as **watch-and-notes** references. Borrow **ideas**; write **new** code.

| Reference | Borrow (ideas) | Do not |
| --------- | -------------- | ------ |
| **`4sXGRM`** ★ | **Overall craft** — palette, wave silhouette, specular breakup, foam/whitecap read, sky reflection quality, camera hero angle; note multipass vs single-pass | Copy GLSL |
| **`4dSBDt`** | Composition, sky–sea balance, cinematic framing, atmospheric colour harmony | Copy GLSL |
| **`MdXyzX`** | Golden-angle **multi-emitter** wave summation; **two-tier** ray march; plane ray setup; compact Fresnel + sky reflect + tonemap | Copy GLSL |
| `Ms2SD1` Seascape *(optional)* | Height-map tracing craft if a fourth study reference is needed | Copy GLSL |
| `shader_lab_raytrace2` | Host scaffolding, existing ocean height / Fresnel / foam starting point | — |

### Techniques to study on `4sXGRM` (rewrite in original form) — **start here**

When viewing in browser, note **behaviour** in `NOTES.md` §4sXGRM first — not code. Author/title from Shadertoy page @ **O-0c**:

1. **Clouds** — structure, motion, shadow on water, reflection read; single-pass vs horizon layer cake.
2. **Wave model** — height-field vs SDF/volume vs hybrid; **calm limit** (does still water look intentional?).
3. **Colour pipeline** — shallow vs deep body colour; absorption; how horizon colour matches sky **and** clouds.
4. **Specular** — sun glint width, roughness; **calm** = elongated mirror path vs **choppy** = sparkle field.
5. **Foam / white water** — crests only when `seaChop` high; absent or subtle in calm mode.
6. **Sky & reflect** — Fresnel balance; cloud reflections on water.
7. **Camera** — **travel path** (drift, dolly, arc); what framing sells scale; note for our scripted paths.
8. **Post** — tonemap, bloom, vignette, exposure.
9. **Cost** — step counts / passes; cloud + water budget split.

Phase 0 must answer: *“Why does the user rank this above `MdXyzX` and `4dSBDt`?”* — capture that in NOTES as **design principles** for our original shader.

### Techniques to study on `MdXyzX` (rewrite in original form)

When viewing in browser, note **behaviour** in `NOTES.md` — not code:

1. **Wave height** — radial `wave(uv, emitter, speed, phase)` summed at golden-angle emitters; fewer octaves for march, more for normals.
2. **Ray march** — coarse steps along view ray between **high** plane (y=0) and **low** plane (y=−depth); refine hit interval in a second pass.
3. **Normals** — finite differences on a **higher-quality** height evaluation than the march uses.
4. **Sky branch** — early out when ray points above horizon; simple analytic `getatm` + tight sun lobe.
5. **Water shade** — Fresnel mix of reflected sky vs absorbed body; distance-based normal soften.
6. **Camera** — mouse-driven yaw/pitch on ray direction (map to Situation orbit, not necessarily identical math).

Our v1 should **match `4sXGRM`’s emotional read** while **exceeding it on physical grounding** (absorption, specular footprint, foam science) and **borrowing `MdXyzX` performance discipline** where compatible.

---

## Realism targets (exceed the references)

Prioritize improvements the reference and `raytrace2` skip or simplify. Pick a **subset for v1**; document the rest as v2.

| Area | v1 target | Stretch (v2) |
| ---- | --------- | -------------- |
| **Clouds** ★ | **Procedural cloud layer** (2D horizon + optional ray-dir fBM march); self-shadow tint; **visible in water reflect** | Multi-scale volumetric march; cloud speed wind uniform |
| **Surface** | Multi-scale Gerstner/sine mix; **`seaChop` → 0** calm path with micro-ripples (not flat) | FFT ocean (compute) |
| **Fresnel** | Schlick; roughness rises as chop falls (calm = sharper reflect) | Full GGX on water |
| **Depth** | Beer–Lambert absorption + shallow turquoise → deep navy | Subsurface scatter |
| **Foam** | Steepness mask; **fades out** when calm | Spray streaks |
| **Sky** | Gradient + sun disk **under** cloud layer | Nishita-lite scatter |
| **Atmosphere** | Horizon fog matched to cloud base colour | Aerial perspective |
| **Choppiness** | `uSeaState.x` wind, `.y` chop — **live + presets** (Calm / Moderate / Storm) | Whitecap bias |
| **Camera travel** ★ | **≥1 scripted path** (spline or keyframed pos/target); **T** toggle travel; mouse overrides while held | Path picker; gentle bob on deck |
| **Post** | Filmic tonemap + `uExposure` | Bloom on sun + cloud silver lining |

**Bar for G2:** Side-by-side with mood screenshots — **`4sXGRM` read** on water **and clouds**; **calm preset** looks deliberate (not “broken waves”); **60 s camera travel** loop without pop; ≥3 §Realism targets; **`MdXyzX`-class** budget @ 1280×720 (or documented quality steps).

---

## Why Situation already has a foundation

| Existing asset | Reuse |
| -------------- | ----- |
| `shader_lab_raytrace2.c` | Fullscreen tri, orbit camera, `uInvVP` rays, **ocean height + Fresnel + foam sketch** |
| `shader_lab_torus.c` | Dual GL/VK shader strings, backend detection |
| `VULKAN_UNIFORM_AGNOSTIC_PLAN.md` | Push constants (VK) vs uniforms (GL) |

**Differentiator vs raytrace2:** Drops the jewel-box scene; invests budget in **clouds + water (calm and choppy) + camera travel** — original shading code, not a scene port.

---

## C host scaffolding (from `shader_lab_raytrace2`)

**Mindset:** Copy the **proven host shell** from `examples/other/shader_lab_raytrace2.c` verbatim where it works; delete jewel-box-only pieces; extend for travel + sea/cloud uniforms. **Do not** adopt `sit_example.h` for the main loop — raytrace2’s custom loop is the template (HUD layout, orbit math, pause semantics). Pull universal quit/fullscreen from `sit_example.h` only if we later want F11/ESC parity.

### What to lift unchanged (raytrace2 → 21)

| Block | raytrace2 source | 21 usage |
| ----- | ---------------- | -------- |
| Includes | `situation.h`, `cglm`, `font_data.h` | Same |
| Vertex type | `RtvVertex` `{pos,nrm,uv}` | Rename `OceanVertex`; same layout |
| Fullscreen tri | `{{-1,-1},{3,-1},{-1,3}}` + `ix[3]` | Identical CCW cover |
| VS (GL) | `k_vs` — clip-space tri | Copy |
| VS (VK pull) | `k_vs_pull` + BDA extensions | Copy; gate on `SIT_FEATURE_BINDLESS_BUFFERS` |
| `init_gpu()` | mesh create → shader load → `SituationGetLastErrorMsg` | Same flow; tag errors `[21_ocean]` |
| `orbit_eye()` | yaw/pitch/radius around `target` | Reuse for **orbit override** and **T-off** stationary mode |
| Matrix upload | `glm_perspective` → `glm_lookat` → `glm_mat4_inv` → `uInvVP` + `uCameraPos` | Same every frame |
| Ray setup (FS) | NDC from `gl_FragCoord` → `uInvVP` → `ro`/`rd` | Copy pattern into **new** FS |
| Draw path | `SITUATION_BEGIN_FRAME` → acquire cmd → render pass → bind pipeline → uniforms → draw mesh → HUD → end | Same |
| VK pull draw | `SituationCmdBindMeshPullBuffers` when `g_use_vertex_pull` | Same |
| HUD | `ui_draw_line_centered` + `SituationLoadBitmapFontFromMemory` | Retheme lines for travel/sea/cloud |
| Input | LMB orbit, wheel zoom, Space pause, V VSync, R reset, F12 PNG | Extend (see below) |
| Screenshot | `SituationTakeScreenshot` + timestamp path | Prefix `ocean_21_` |

### What to delete (raytrace2-only)

- `uPreset` / keys 1–3 **scene** presets → repurpose **1/2/3** for **sea** presets (Calm / Moderate / Storm).
- Entire FS: `sceneIntersect`, spheres, box, glass, shadow rays, jewel-box `trace()`.
- `target = {0, 0.65, 0}` orbit pivot tuned for jewel box → travel-centric defaults.

### What to add (21-only host)

| Feature | Implementation sketch |
| ------- | --------------------- |
| **Camera travel (default)** | `travel_eval(float uTravelTime, vec3* eye, vec3* target, float* phase)` — v1: low altitude drift along +Z, `y ≈ 1.2`, slow `sin` yaw; `phase = fmod(uTravelTime * speed, 1)` |
| **Travel toggle** | `bool g_travel = true`; **T** flips; when off, freeze at current path sample or fall back to orbit pivot |
| **Dual time** | `uTime` (shader anim) + `uTravelTime` (path) — both pause on **Space** |
| **Orbit override** | LMB down sets `g_orbit_override`; while held, use `orbit_eye` instead of travel; optional blend-back on release (v2) |
| **Sea presets** | `float g_sea_state[4] = {wind, chop, waveHeight, calmRipple}`; keys **1/2/3** set table from plan |
| **Cloud state** | `float g_cloud_state[4]`; **`,` `.`** adjust coverage (optional v1) |
| **Sun** | `vec3 g_sun_dir` — fixed or slow arc; upload `uSunDir` |
| **Exposure** | `float g_exposure = 1.0f`; upload `uExposure` |
| **Wheel on travel** | When `g_travel`: scale path speed; when orbit: zoom radius (raytrace2 behaviour) |

Suggested host state block (mirror raytrace2 `main` locals):

```c
/* Defaults: moderate sea, cloudy, travel on — matches design pillars */
bool  g_anim = true, g_travel = true, g_orbit_override = false, g_vsync = true;
float g_time = 0.f, g_travel_time = 0.f, g_travel_speed = 1.f;
float g_yaw = 0.85f, g_pitch = 0.22f, g_radius = 8.f;
vec3  g_orbit_target = {0.f, 0.f, 0.f};
float g_sea_state[4]   = {0.35f, 0.45f, 0.12f, 0.08f};  /* preset 2 */
float g_cloud_state[4] = {0.62f, 0.04f, 1.0f, 0.35f};
vec3  g_sun_dir = {0.35f, 0.92f, 0.25f};
float g_exposure = 1.05f;
```

### Uniform upload order (each frame)

Same as raytrace2 — set after `SituationCmdBindPipeline`:

1. `uCameraPos` — `vec3`
2. `uInvVP` — `mat4`
3. `uResolution` — `vec2`
4. `uTime` — `float`
5. `uTravelPhase` — `float` *(new)*
6. `uSunDir` — `vec3`
7. `uSeaState` — `vec4`
8. `uCloudState` — `vec4`
9. `uExposure` — `float`

Vulkan: same names via `SituationSetShaderUniform` (library maps to push/UBO). No bare `uniform float` in FS without GL counterpart.

### FS contract (new strings — not raytrace2 paste)

- **Keep from raytrace2 FS (rewrite, don’t copy-paste):** `oceanH` / `oceanNormal` *ideas*, Fresnel + foam + depth murk structure in `shadePrimary` for `h.id==1` — evolve into height-field ray march (MdXyzX tier idea).
- **Drop:** all scene geometry, shadows, glass, presets in shader.
- **Add:** `sampleClouds(rd, tm)`, analytic sky under clouds, tiered sea march, calm branch when `uSeaState.y < ε`.

### Secondary reference: `shader_lab_torus.c`

Use only for:

- `SituationGetGraphicsBackend()` / backend name in startup `printf`
- Pattern of **dual shader strings** if GL and VK FS ever diverge (prefer single FS body until O-16 fails)

### `init_gpu` error path (required)

```c
if (err != SITUATION_SUCCESS) {
    char* msg = NULL;
    SituationGetLastErrorMsg(&msg);
    fprintf(stderr, "[21_ocean] Shader compile failed: %s\n", msg ? msg : "?");
    if (msg) SituationFreeString(msg);
    ...
}
```

OpenGL link failure here is **O-16 gate** — log full driver message before trimming `CLOUD_STEPS`.

### File header (O-4)

```c
/* Example 21 — Realistic Ocean (Advanced Shader)
 * Original work — MIT (Situation). Visual inspiration only:
 *   Shadertoy 4sXGRM, 4dSBDt (Thomas Schander), MdXyzX — no third-party GLSL included.
 * Host scaffolding derived from examples/other/shader_lab_raytrace2.c (MIT). */
```

### O-10 acceptance (scaffold done)

- [ ] `examples/21_ocean_realistic/main.c` compiles `static-opengl` + `static-vulkan`
- [ ] Window opens; fullscreen tri draws; travel camera moves; HUD shows backend + mode
- [ ] FS is **new** minimal sky + sea (clouds can be gradient stub until O-20)
- [ ] No `#include` of Shadertoy or raytrace2 FS strings

---

## Technical contract (Situation-native)

### Camera travel (host — first-class)

CPU owns camera; fragment shader receives matrices only.

| Mode | Behaviour |
| ---- | --------- |
| **Travel (default)** | Position + look-at from **keyframed path** or Catmull-Rom spline over `uTravelTime`; loops seamlessly |
| **Orbit override** | LMB drag temporarily replaces orientation; release → blend back to path over ~1 s (optional) |
| **Pause** | **Space** freezes `uTime` **and** `uTravelTime` so clouds/water/travel halt together |

Suggested path (v1): low-altitude **forward drift** along +Z with slow sinusoidal yaw — ~90 s loop; second path (v2): higher wide shot for cloudscape.

Host uploads each frame:

- `uCameraPos`, `uInvVP` — from travel or orbit (same as `shader_lab_raytrace2`)
- `uTravelPhase` — 0…1 along path (debug HUD)

### Uniforms (host)

| Uniform | Purpose |
| ------- | ------- |
| `uResolution` | `SituationGetRenderWidth/Height()` |
| `uTime` | Accumulated frame time (pauses with Space) |
| `uCameraPos`, `uInvVP` | World rays — from **travel path** or orbit override |
| `uSunDir` | Normalized sun direction |
| `uSeaState` | `(wind, chop, waveHeight, calmRipple)` — **chop=0** enables calm branch |
| `uCloudState` | `(coverage, speed, brightness, shadowStrength)` |
| `uExposure` | Tonemap scale |

**Sea presets (CPU keys 1 / 2 / 3):**

| Key | `chop` | Character |
| --- | ------ | ----------- |
| **1** | ~0.05 | **Calm** — mirror sea, micro-ripples, cloud reflections dominant |
| **2** | ~0.45 | **Moderate** — default travel demo |
| **3** | ~1.0 | **Lively** — visible crests, foam hints |

Vulkan: push constant block or small UBO — no bare fragment `uniform float` on VK.

### Draw path

- One fullscreen CCW triangle; no depth test.
- `#version 450 core` (GL) / `#version 450` (VK) with `gl_VertexIndex` split in VS only.

---

## Phase gates

| Gate | Unlocks | Required |
| ---- | ------- | -------- |
| **G0 — Reference study** | Coding | Mood board + technique notes; copyright policy ticked |
| **G1 — First light** | Realism passes | Sea + **clouds** + **camera travel** visible both backends |
| **G2 — Realism** | Polish | Calm + choppy presets; cloud reflect; ≥3 §Realism targets |
| **G3 — Interaction** | Ship prep | Travel toggle, orbit override, sea presets |
| **G4 — Ship** | Docs | Build scripts + README + examples index |

---

## Master checklist

### Phase 0 — Reference study (G0) — no source paste

- [ ] **O-0c** View [**4sXGRM**](https://www.shadertoy.com/view/4sXGRM) first; screenshot → `examples/21_ocean_realistic/refs/reference_4sXGRM_mood.png`; record **title + author** from page
- [ ] **O-0a** View [4dSBDt](https://www.shadertoy.com/view/4dSBDt); screenshot → `refs/reference_4dSBDt_mood.png`
- [ ] **O-0b** View [MdXyzX](https://www.shadertoy.com/view/MdXyzX); screenshot → `refs/reference_MdXyzX_mood.png`
- [ ] **O-1** Write `examples/21_ocean_realistic/refs/NOTES.md` — **three sections** (`4sXGRM` first, then 4dSBDt, MdXyzX): techniques observed, **no code**; §4sXGRM design principles (“why best”)
- [ ] **O-2** Record authors + URLs in NOTES + README attribution block (all three)
- [ ] **O-3** List v1 **realism upgrades** + ideas per ref; explicitly note **cloud**, **calm water**, **camera travel** choices for NOTES §Design pillars
- [ ] **O-4** Example header: *original work, MIT; visual inspiration: Shadertoy 4sXGRM + 4dSBDt + MdXyzX — no third-party GLSL included*

### Phase 1 — First light (G1)

- [x] **O-10** Create `examples/21_ocean_realistic/main.c` — scaffold from `shader_lab_raytrace2.c` (host only; **new** FS strings)
- [x] **O-11** Implement minimal **original** FS: **cloud layer** + sky + height-field sea; host **travel path** driving `uInvVP`
- [x] **O-12** OpenGL compile + run
- [x] **O-13** Vulkan compile + run (`static-vulkan`)
- [x] **O-14** Animated water surface visible ≥ 10 s
- [ ] **O-15** No render-thread `-600` on test DLL (Track D — use known-good OpenGL DLL if needed)
- [ ] **O-16** **OpenGL instruction gate** — FS compiles + links on ref GPU (GTX 1070 class); no exceed of ~65k instruction limit
- [ ] **O-17** *(Optional)* Log linked instruction count / driver limit if query API available

### Phase 2 — Realism pass (G2)

- [x] **O-20** **Clouds** — structure + motion; shadows tint water; reflections show cloud shapes
- [x] **O-21** Depth colour absorption (shallow vs deep)
- [x] **O-22** Specular — calm mirror vs choppy sparkle (coupled to `uSeaState.y`)
- [x] **O-23** **Calm preset (key 1)** — chop≈0 still looks alive (micro-ripples); cloud reflect legible
- [x] **O-24** Foam only when chop high; fade for calm/moderate
- [x] **O-25** Horizon atmospheric fade aligned with **cloud base** colour
- [ ] **O-26** Compare mood PNGs — document wins (clouds, calm, travel framing)
- [ ] **O-27** 60 FPS @ 1280×720 GTX 1070 with **travel + clouds + moderate sea** (or quality steps)

### Phase 3 — Interaction & camera (G3)

- [x] **O-30** **Camera travel** on by default; **T** toggle travel vs stationary orbit pivot
- [x] **O-31** LMB orbit **override** (optional blend back to path on release)
- [x] **O-32** Mouse wheel — FOV or travel speed
- [x] **O-33** **Space** — pause time + travel + water phase
- [x] **O-34** **1 / 2 / 3** — Calm / Moderate / Storm sea presets
- [x] **O-35** **`[` `]`** — fine-tune chop; **`,` `.`** — cloud coverage (optional)
- [x] **O-36** **V** VSync; **R** reset camera to path start; **F12** PNG screenshot

### Phase 4 — Performance & robustness

- [x] **O-40** Frame-time HUD
- [ ] **O-41** Optional `uRenderScale` (0.5×) if needed
- [ ] **O-42** Headless smoke: init → one frame → shutdown
- [ ] **O-43** README notes iGPU expectations

### Phase 5 — Integration & docs (G4)

- [x] **O-50** `build/build_examples.bat` — `21_ocean_realistic` on `static-opengl` + `static-vulkan`
- [x] **O-51** `examples/21_ocean_realistic/README.md` — build, controls, **GL instruction budget**, inspiration attribution
- [ ] **O-52** [`DIGESTIBLE_EXAMPLES_PLAN.md`](DIGESTIBLE_EXAMPLES_PLAN.md) §21 + `examples/README.md` quick-build row
- [ ] **O-53** Optional `COMPILATION_GUIDE.md` one-liner
- [ ] **O-54** `whatsnew.md` on ship

---

## File layout

```
examples/21_ocean_realistic/
├── main.c                             # host + embedded original GLSL
├── README.md                          # Example 21 — Advanced Shader
└── refs/                              # optional mood boards (git)
    ├── NOTES.md                       # technique study (no pasted Shadertoy code)
    ├── reference_4sXGRM_mood.png
    ├── reference_4dSBDt_mood.png
    └── reference_MdXyzX_mood.png
```

---

## Build & run (target)

```bat
build\build_examples.bat static-opengl 21_ocean_realistic
build\examples\21_ocean_realistic.exe

build\build_examples.bat static-vulkan 21_ocean_realistic
```

---

## Verification matrix

| Check | OpenGL | Vulkan |
| ----- | ------ | ------ |
| Exe launches | [ ] | [ ] |
| **Camera travel** loop ≥ 90 s seamless | [ ] | [ ] |
| **Clouds** animate + reflect on water | [ ] | [ ] |
| **Calm preset (1)** convincing ≥ 30 s | [ ] | [ ] |
| **Moderate/Storm (2/3)** wave read | [ ] | [ ] |
| Travel toggle + pause + orbit | [ ] | [ ] |
| Realism vs mood reference (NOTES) | [ ] | [ ] |
| **No third-party GLSL in tree** | [ ] | [ ] |
| **OpenGL FS links** (≤~65k instructions) | [ ] | n/a |

---

## Investigation log *(fill during work)*

```
Date:
Inspiration (URLs only, no source copied):
  - 4sXGRM — oceanic — (author @ O-0c) ★ primary
  - 4dSBDt — Enscape Cube — Thomas Schander
  - MdXyzX — fast open-water (author @ O-2)

Design pillars (user):
  - Clouds: important
  - Calm water: as interesting as waves
  - Camera travel: supported

4sXGRM design principles captured:
  -

Ideas adopted in original form (per ref):
  - 4sXGRM:
  - 4dSBDt:
  - MdXyzX:

v1 realism features shipped:
  -

Wins vs 4sXGRM mood ref:
  -
Wins vs 4dSBDt mood ref:
  -
Wins vs MdXyzX mood ref:
  -

Performance @ 1280×720:
  OpenGL: ___ ms/frame
  Vulkan: ___ ms/frame
```

---

## Reference

| Item | Value |
| ---- | ----- |
| Visual inspiration ★ **primary** | https://www.shadertoy.com/view/4sXGRM — catalog **oceanic** (author @ O-0c) |
| Visual inspiration (atmosphere) | https://www.shadertoy.com/view/4dSBDt — Enscape Cube, Thomas Schander |
| Visual inspiration (perf craft) | https://www.shadertoy.com/view/MdXyzX — fast procedural ocean |
| Example license | MIT (Situation `LICENSE`) — **original shader code** |
| Code base | Host from `shader_lab_raytrace2`; water ideas synthesised from all three refs |
| OpenGL DLL | `LIBRARY_RECOVERY_PLAN_244.md` Track D if testing fresh DLL |

**README attribution template (ship):**

> *Original shader and example — MIT (Situation). Visual inspiration only: [Shadertoy 4sXGRM](https://www.shadertoy.com/view/4sXGRM) (primary), [Shadertoy 4dSBDt](https://www.shadertoy.com/view/4dSBDt) (Thomas Schander), [Shadertoy MdXyzX](https://www.shadertoy.com/view/MdXyzX). No Shadertoy source code is included in this repository.*

---

## Implementation order

| Step | Work | Gate |
| ---- | ---- | ---- |
| **1** | O-0 … O-4 — mood board + NOTES + policy | G0 |
| **2** | O-10 … O-15 — first light | G1 |
| **3** | O-20 … O-27 — realism (clouds, calm, travel) | G2 |
| **4** | O-30 … O-36 — travel + presets + controls | G3 |
| **5** | O-40 … O-43 — perf | — |
| **6** | O-50 … O-54 — ship | G4 |

---

*Revision 2026-06-27 (g): **§C host scaffolding** from `shader_lab_raytrace2`; `examples/21_ocean_realistic/main.c` scaffold builds GL+VK static. Prior (f): Example 21 + O-16 gate; (e): design pillars; (d): 4sXGRM primary.*