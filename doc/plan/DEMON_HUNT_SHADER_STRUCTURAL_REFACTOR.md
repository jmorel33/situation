# Demon Hunt — Unified Raycaster Shader: Structural Refactor

**Date**: 2026-05-20  
**Status**: **In progress** — Step B/C partial; Step E wired  
**Scope**: `examples/demon_hunt_sky.fs` (stay **one** unified raycaster program)  
**Related**: [`DEMON_HUNT_DISTANCE_RECOVERY_PLAN.md`](DEMON_HUNT_DISTANCE_RECOVERY_PLAN.md) (distance buffer = optional future, not required for correctness)

---

## Master checklist

- [x] Step B — unified `map_dda_occluded()` (one 64-step walker)
- [x] Step A (partial) — recovery `#define`s; soft shadow off; sprite lite lighting on
- [x] Step E — CPU world fallback when `!g_sky_ok`
- [ ] Step A (bisect) — flip toggles on OOM GPU; record pass/fail in log below
- [x] Step B verify — `sit_test --module graphics --filter demon_hunt_sky_shader_link` OK (~11s link)
- [x] Step C — `shade_sprite_opaque` / `shade_sprite_alpha`; `gather_dynamic_lights`; shadow budget
- [ ] Step D — re-enable soft shadow, bloom, phase3 after clean link on OOM GPU
- [ ] Step E verify — CPU sprites still draw when `ENABLE_SPRITE_RESOLVER 0`

---

## Execution log

_Check off when done; add date + one-line result in parentheses._

- [x] **2026-05-20 · B** — `map_dda_occluded()`; thin `maze_shadow` / `point_shadow` / `segment_world_visible` wrappers _(one 64-step walker)_
- [x] **2026-05-20 · A partial** — `DH_ENABLE_*` toggles; soft shadow off; sprite lite lighting on
- [x] **2026-05-20 · E** — `sky_draw_cpu_world_fallback()` in `demon_hunt.c`
- [ ] **A bisect** — run toggle bisect on OOM machine → paste `skydome shader failed:` or `compile/link OK` here: _______________
- [x] **2026-05-20 · C** — `shade_sprite_opaque` / `gather_dynamic_lights` / `DH_MAX_POINT_SHADOW_TESTS`
- [ ] **D** — feature re-enable pass (soft shadow → bloom → phase3 → sprite lite off)

---

## Recovery toggles

Edit in `examples/demon_hunt_sky.fs` (lines ~22–28):

```glsl
#define DH_ENABLE_SOFT_SHADOW        0  /* 1 = 4-tap sun */
#define DH_ENABLE_BLOOM              1
#define DH_ENABLE_PHASE3_SPRITES     1
#define DH_SPRITE_LITE_LIGHTING      1  /* 1 = no pristine_shadow on sprites */
```

### Bisect (stop at first link OK)

For each row: check **tried**, then check **linked** or **failed**.

- [ ] Tried: `DH_ENABLE_PHASE3_SPRITES` → `0`, rebuild, run hunt
  - [ ] Linked OK
  - [ ] Still OOM / fail
- [ ] Tried: `DH_ENABLE_BLOOM` → `0`, rebuild
  - [ ] Linked OK
  - [ ] Still OOM / fail
- [ ] Tried: `ENABLE_SPRITE_RESOLVER` → `0`, rebuild _(CPU sprites)_
  - [ ] Linked OK
  - [ ] Still OOM / fail
- [x] `DH_ENABLE_SOFT_SHADOW` → `0` _(already default)_

**Minimal profile that linked:** _______________

---

## Build & verify

From repo root:

```bat
build_situation.bat opengl
build_examples.bat opengl demon_hunt
```

Run `build\examples\demon_hunt.exe` from repo root.

### Success (check all that apply after a run)

- [x] Harness: `sit_test --module graphics --filter demon_hunt_sky_shader_link` passes
- [ ] `demon_hunt_sky.log` contains `shader compile/link OK` (in-game)
- [ ] `demon_hunt_sky.log` contains `skydome GPU path OK`
- [ ] In play: walls / floor / sky visible (not black void)
- [ ] HUD does **not** show `World shader FAILED`

### If link still fails

- [ ] CPU column walls visible (Step E fallback)
- [ ] Last error line copied from log: _______________

---

## Step A — Bisect (1–2 hours)

- [x] Add `DH_ENABLE_*` toggles at top of `demon_hunt_sky.fs`
- [ ] Run bisect list above; fill execution log
- [ ] Record minimal toggle profile that links on OOM GPU

---

## Step B — Dedupe DDA (~half day)

- [x] Implement `map_dda_occluded(mode, …)` after `arch_hit_interval`
- [x] Replace `maze_shadow`, `point_shadow`, `segment_world_visible` with wrappers
- [x] Harness link test passes on dev machine
- [ ] In-game: floor sun shadow still looks reasonable

---

## Step C — Sprite path slim (~half day)

- [x] Single `shade_sprite_opaque` / `shade_sprite_alpha` with type switch
- [x] Opaque alpha from `hit.opaque_coverage` (resolver); no second coverage pass on ammo
- [x] `gather_dynamic_lights` replaces 4× duplicated light loops at floor/wall/sprite call sites
- [x] `DH_MAX_POINT_SHADOW_TESTS` caps point-shadow DDAs per gather
- [ ] Keep `DH_SPRITE_LITE_LIGHTING 1` until OOM GPU confirms link
- [ ] Optional: restore sun tap on sprites after link stable (Step D)

---

## Step D — Re-enable features

- [ ] `DH_ENABLE_SOFT_SHADOW 1` → rebuild → play-test walls
- [ ] `DH_ENABLE_BLOOM 1` (if it was off during bisect)
- [ ] `DH_ENABLE_PHASE3_SPRITES 1` (if it was off)
- [ ] `DH_SPRITE_LITE_LIGHTING 0` only if link still OK

---

## Step E — Fallback (never black void)

- [x] `sky_draw_cpu_world_fallback()` in `examples/demon_hunt.c`
- [x] Call from `sky_draw_fullscreen` when `!g_sky_ok`
- [ ] Confirm CPU sprites still draw when `ENABLE_SPRITE_RESOLVER 0`

---

## Context (why this plan exists)

| Claim | Evidence |
|-------|----------|
| Not a megakernel | ~1,380 lines FS vs QSR ~25k LOC compute that links |
| 32 sprites did not cause OOM | Phase 0 compiled at **256** slots |
| OOM = duplicated structure | Triple DDA, 4× soft sun, per-light shadow loops |
| Goal = one unified raycaster | Same `main()`, same ray — not a prettify pass stack |

### Structural defects

| Issue | Status |
|-------|--------|
| Three 64-step DDA copies | [x] unified into `map_dda_occluded` |
| `pristine_shadow` 4× sun | [x] gated by `DH_ENABLE_SOFT_SHADOW` (default off) |
| Many `point_shadow` per pixel | [x] shared walker + `DH_MAX_POINT_SHADOW_TESTS` per gather |
| Bloom | [x] gated by `DH_ENABLE_BLOOM` |
| Sprite shade duplication | [x] Step C (single shade entry + gather) |

**Keep:** single program, SSBO `ShaderScenePack`, per-pixel sprite vs wall.  
**Distance buffer:** secondary plan — only if Steps A–D still OOM.

---

## What we are NOT / ARE saying

- ~~32 sprites killed the compiler~~
- ~~Must split into distance + shade pass~~
- **Unified raycaster in one shader: correct**
- **Fix: refactor in place** + bisect + CPU fallback

**Next action:** Step A bisect on OOM GPU → Step C → Step D.
