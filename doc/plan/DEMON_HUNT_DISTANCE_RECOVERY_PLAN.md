# Demon Hunt — Distance-First Recovery Plan

**Date**: 2026-05-20  
**Status**: **Secondary / on hold** — only if structural Steps A–D still OOM  
**Primary (active)**: [`DEMON_HUNT_SHADER_STRUCTURAL_REFACTOR.md`](DEMON_HUNT_SHADER_STRUCTURAL_REFACTOR.md)  
**Parent**: [`DEMON_HUNT_IQ_PLAN.md`](DEMON_HUNT_IQ_PLAN.md)  
**Scope**: `examples/demon_hunt.c`, `examples/demon_hunt_sky.vs`, optional split FS files

> Distance buffer + shade split is a **fallback compile strategy**, not a rejection of unified raycasting.

---

## Master checklist (this plan — on hold until structural plan fails)

- [x] Phase 0E — unified `map_dda_occluded` in monolithic FS _(structural plan)_
- [x] Phase 0D — CPU world fallback when shader link fails
- [ ] Phase 0A — bisect toggles on OOM GPU _(structural plan Step A)_
- [x] Phase 0B — rebuild DLL + `demon_hunt.exe`
- [x] Phase 0C — harness `demon_hunt_sky_shader_link` OK (dev machine)
- [ ] Phase 0C — in-game log shows `shader compile/link OK` on OOM GPU
- [ ] Gate: structural Steps A–D exhausted → only then start Phase 1 below
- [ ] Phase 1 — distance kernel FS + RT
- [ ] Phase 2 — shade mega-kernel FS
- [ ] Phase 3 — wire frame + dual OK flags + HUD
- [ ] Phase 4 — sprite path via distance buffer
- [ ] Phase 5 — hardening (optional)

---

## Phase 0 — Unblock play

**Goal:** No black void when shader link fails.

- [ ] **0A** Bisect toggles in `demon_hunt_sky.fs` _(see structural plan bisect list)_
- [ ] **0B** `build_situation.bat opengl` + `build_examples.bat opengl demon_hunt`
- [ ] **0C** `demon_hunt_sky.log` → `shader compile/link OK`
- [x] **0D** `sky_draw_cpu_world_fallback()` when `!g_sky_ok` _(2026-05-20)_
- [x] **0E** `map_dda_occluded` dedupe in monolithic FS _(2026-05-20)_

**Exit criteria:**

- [ ] Playable walls via GPU **or** CPU columns
- [ ] HUD documents shader failure when GPU path off

> Do **not** start Phase 1 until structural plan Steps A–D are tried on the OOM machine.

---

## Phase 1 — Distance kernel (1–2 days)

**Goal:** Small compile unit — visibility only.

- [ ] Create `examples/demon_hunt_distance.fs` — `cast_prim`, `cell_wall` / `cell_arch` from SSBO, optional `resolve_sprites`
- [ ] Create `examples/demon_hunt_distance.vs` — same fullscreen tri as today
- [ ] C: `g_distance_tex` via `SituationCreateTextureEx` @ game resolution
- [ ] C: `distance_pass_draw()` — bind SSBO @1, draw tri, write distance RT
- [ ] Link test in isolation — log shows **distance** program OK
- [ ] Confirm distance FS has **no** `pristine_shadow`, `shade_floor`, sky FBM, bloom

---

## Phase 2 — Shade mega-kernel (1–2 days)

**Goal:** Shade program reads distance buffer.

- [ ] Create `examples/demon_hunt_shade.fs` — sample distance; branch on `hit_wall`; wall strip from stored `raw` / `perp` / `side`
- [ ] Move `sky_shade`, `shade_floor`, lighting loops, `composite_sprites` into shade FS
- [ ] C: `shade_pass_draw()` — bind distance tex + SSBO + uniforms
- [ ] Retire or thin-wrap monolithic `demon_hunt_sky.fs`
- [ ] Link test — shade program OK in isolation

---

## Phase 3 — Wire frame + fallback (0.5 day)

- [ ] `render_world`: `distance_pass` → `shade_pass` (replace `sky_draw_fullscreen`)
- [ ] `g_sky_ok` → `g_distance_ok && g_shade_ok` (or two flags)
- [ ] Either link fail → CPU fallback still runs
- [ ] HUD: `Distance: OK  Shade: OK  Sprites: shader|CPU`

---

## Phase 4 — Sprite path alignment (1 day)

- [ ] Re-enable resolver in **distance** kernel only
- [ ] Shade reads sprite winner from distance buffer (id + dist)
- [ ] CPU sprite loop skips when `shader_sprite_runtime_enabled()`
- [ ] Harness: `sit_test` fragment / SSBO tests still green

---

## Phase 5 — Hardening (optional)

- [ ] Precompile `.spv` offline — skip shaderc at launch
- [ ] Retry shader compile on title screen (not only once on enter)
- [ ] Update `DEMON_HUNT_IQ_PLAN.md` § Target Shader Model to distance-first

---

## Acceptance criteria

- [ ] Link succeeds — `demon_hunt_sky.log`, no `C9999` OOM
- [ ] World visible — walls, floor, sky in play
- [ ] Sprites occluded behind walls when Phase 4 on
- [ ] Link failure not game over — CPU fallback draws something
- [ ] `sit_test --filter fragment` still green

---

## Target architecture (reference)

```
Frame:
  [Distance kernel]  →  RGBA32F hit buffer per pixel
  [Shade mega-kernel] →  reads distance + SSBO + uniforms → final color
```

| Channel | Content |
|---------|---------|
| R | `raw` — ray distance along view |
| G | `perp` — fisheye-corrected depth for wall strips |
| B | packed `hit_wall`, `side_hit`, optional arch kind |
| A | sprite `sprite_raw` or 0 |

**Not in scope:** bloom pass, SSAO, decorative multi-pass stacks.

---

## Why distance was not done first (context)

IQ plan optimized for per-pixel sprite occlusion in **one** fragment program → duplicated DDAs → linker OOM; CPU world removed before distance+shade existed; flat-band fallback was documented but not wired until 2026-05-20.

---

## What we are NOT doing

- Decorative multi-pass pipelines (bloom/SSAO stacks) — out of scope
- Permanent CPU raycast walls — fallback only when GPU link fails
- Growing monolithic `demon_hunt_sky.fs` further — out of scope

---

## Immediate next action

1. [ ] Complete structural plan Step A bisect on OOM GPU
2. [ ] If link OK: structural Steps C + D in monolithic FS
3. [ ] **Only if still OOM:** start Phase 1 above
