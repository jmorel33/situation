# Plan: Vulkan Dynamic UBO Ring Buffer — Non-Regressive Roadmap

**Last reviewed:** 2026-06-22 (cross-checked against v2.4.335 codebase)
**AAA alignment:** Per-draw uniform data without GPU pipeline stalls; shared frame rings instead of per-object staging; same “bind once, offset many” philosophy as Phase E bindless push constants in `plan_handles_ssbo.md`. **Orthogonal to Phase D bindless textures** — DU does not touch `global_textures[]`.

**North star consumer:** **Phase G scaler sprite system** (`plan_handles_ssbo.md`) — each sprite instance needs a small uniform/SSBO record every frame; DU1 is the non-regressive path to stream **thousands of instance records** without per-sprite `VkBuffer` + descriptor alloc.

**Next milestone:** **Phase DU1.0** — per-frame UBO ring + master descriptor set (user MESH path only). **Do not migrate internal view/proj UBOs until DU1 is green.**

---

## Goal (AAA)

```
Per frame (per frame-in-flight slot):
  One host-mapped UBO ring buffer
  One master dynamic UBO descriptor set → ring (VK_WHOLE_SIZE)
  Many draws: UpdateBuffer → memcpy suballoc; BindDescriptorSetDynamic(offset) — no vkUpdateDescriptorSets, no staging barriers
```

Target workloads: many user meshes / materials updating matrices or material constants in one frame without serializing the GPU; **future Phase G sprite instance stream** (pos, scale, uv, tint per sprite). Internal 2D (quad/text/VD) stays on **`view_data_ubo_layout`** until **DU2**.

### How DU1 feeds the scaler sprite system

```
Frame acquire → reset UBO ring cursor
  → for each sprite: UpdateBuffer(instance_record)  // ring suballoc
  → one or few BindDescriptorSetDynamic(master_set, offset_i)
  → draw batch (instanced quad or pull VS reading instance SSBO)
  → texture_id per instance via Phase D bindless (atlas slice)
```

| Sprite need | Plan owner |
|-------------|------------|
| Instance data bandwidth | **DU1** (ring) or **G2** SSBO batch |
| Atlas texture index | **D0/D2** (`texture_id` + `global_textures[]`) |
| 9-slice / scale shader | **G3** |
| Sort + batch policy | **G2** |

---

## What shipped vs what this plan adds

| Layer | Status | Version |
|-------|--------|---------|
| **DU0 — Dynamic descriptor API** | ✅ Shipped | v2.3.29+ |
| **DU1 — Ring suballocation (performance)** | 🔲 Not started | — |
| **DU2 — Internal view/proj ring** | 🔲 Blocked on DU1 | — |

### DU0 — Shipped foundation (v2.3.29, hardened v2.4.55+)

- [x] **`dynamic_ubo_layout`** — `VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC` @ binding 0.
- [x] **`SituationCmdBindDescriptorSetDynamic`** — `pDynamicOffsets` on bind.
- [x] **`SITUATION_BUFFER_USAGE_DYNAMIC_UNIFORM`** — public usage flag (alias of uniform + transfer_dst).
- [x] **User MESH / compute graphics** — `_SituationVulkanResolveBufferDescriptor` routes set 0 UBO → `dynamic_ubo_layout` + dynamic offset.
- [x] **Harness `descriptor_bind_dynamic_offset`** — same dedicated buffer, offset 0 = red, offset 256 = blue.

**What DU0 is *not*:** ring buffer, master descriptor set, or elimination of per-buffer `VkBuffer` + per-buffer descriptor pool. Each `SituationCreateBuffer` UBO still allocates its own GPU buffer; dynamic binding only slices **within that buffer**.

---

## The problem DU1 solves (still open)

### Current `SituationUpdateBuffer` (Vulkan)

1. Host-visible buffer → direct `memcpy` (good for dedicated UBOs).
2. GPU-only buffer → staging / `vkCmdUpdateBuffer` via single-time command buffer (can serialize or add recording overhead).

### AAA pain (100 draws / frame)

Each object with its own small UBO today implies:

- Dedicated `VkBuffer` + descriptor set per buffer (memory + pool pressure).
- Updates that miss host-visible path pay copy/barrier cost.
- No shared “versioning cursor” like **`dynamic_vbo`** (text) already uses for vertices.

**DU1** copies the proven **`dynamic_vbo`** pattern for **user uniform data** on the MESH profile path.

---

## Phase plan

### Phase DU1 — User-path UBO ring (non-regressive)

**Scope:** User shaders (`SIT_SPIRV_LAYOUT_PROFILE_MESH`, compute uniform path). **Out of scope:** internal quad/text/VD (`view_proj_ubo_descriptor_set`), Phase D bindless, SSBO ring.

#### DU1.0 — Ring infrastructure

- [ ] **DU1.0.1** — Query and store `minUniformBufferOffsetAlignment` + `maxUniformBufferRange` at init.
- [ ] **DU1.0.2** — Per frame-in-flight: `global_ubo_ring[i]` (~2–4 MB, `HOST_VISIBLE | HOST_COHERENT`, mapped) — mirror `dynamic_vbo[]` lifecycle.
- [ ] **DU1.0.3** — **Master descriptor set** per frame (or one set + frame index in bind): points at `global_ubo_ring[current_frame_index]`, range `VK_WHOLE_SIZE`, layout `dynamic_ubo_layout`.
- [ ] **DU1.0.4** — Reset `global_ubo_cursor[frame]` at **`SituationAcquireFrameCommandBuffer`** (same slot as `current_frame_index`).

#### DU1.1 — Opt-in suballocation (keep dedicated path)

- [ ] **DU1.1.1** — `_SituationBufferSlot`: `ring_suballoc` flag + `ring_offset` (internal; no public struct change required if offset stays in slot).
- [ ] **DU1.1.2** — **`SituationCreateBuffer`**: if `SITUATION_BUFFER_USAGE_DYNAMIC_UNIFORM` **and** `size <= DU_RING_MAX_SUBALLOC` (e.g. 16 KB) **and** ring has space → ring suballoc only (no dedicated `VkBuffer`).
- [ ] **DU1.1.3** — **`SituationUpdateBuffer`**: ring-backed buffers → align cursor, `memcpy`, store offset; **no** `vkUpdateDescriptorSets`, **no** staging for that path.
- [ ] **DU1.1.4** — **`SituationCmdBindDescriptorSetDynamic`**: ring-backed → bind **master** set + slot’s `ring_offset`; dedicated buffers → **unchanged** today’s path.
- [ ] **DU1.1.5** — **Fallback:** size over limit, ring exhausted, or non-uniform usage → existing dedicated buffer + current behavior (zero regression).

#### DU1.2 — Harness gates (required before DU1 “done”)

- [ ] **DU1.2.1** — **`test_ubo_ring_single_draw`** — ring-backed buffer, one draw, readback matches (parity with `descriptor_bind_dynamic_offset`).
- [ ] **DU1.2.2** — **`test_ubo_ring_multi_draw_same_frame`** — N≥4 suballocs in one frame, N draws with different dynamic offsets, distinct readback colors (proves no GPU serialization / tearing).
- [ ] **DU1.2.3** — Regression suite unchanged: `descriptor_bind_dynamic_offset`, `descriptor_bind_sampled_texture`, full `--module graphics` baseline recorded in UPDATELOG.

**DU1 exit criteria**

| Gate | Must pass |
|------|-----------|
| `test_ubo_ring_single_draw` | ✅ |
| `test_ubo_ring_multi_draw_same_frame` | ✅ |
| `descriptor_bind_dynamic_offset` | ✅ (dedicated-buffer path unchanged) |
| Internal textured draws | ✅ unchanged (`single_sampler` model) |

**No-go:** merging ring path as default for all UBOs, or touching internal `view_proj_ubo_*`, while DU1.2 is red.

---

### Phase DU2 — Internal view/proj ring (optional, later)

*Blocked on DU1 exit criteria. Lower priority than Phase D0 / user perf.*

- [ ] **DU2.1** — Evaluate migrating **`view_proj_ubo_descriptor_set`** to ring + dynamic offset vs keeping per-frame dedicated buffers (already mapped; may be “good enough”).
- [ ] **DU2.2** — If migrated: audit all internal set-0 binds (quad, text, YPQ, VD, user bind collision).

**Default recommendation:** defer DU2 until DU1 proves ring discipline; internal path already uses per-frame mapped UBOs without staging.

---

## Hard no-go rules (non-regressive)

1. **Dedicated path stays** — any buffer that fails ring criteria uses today’s `CreateBuffer` + `EnsureBufferDescriptorSet` behavior.
2. **Public API unchanged** — same `SituationCreateBuffer`, `SituationUpdateBuffer`, `SituationCmdBindDescriptorSetDynamic` signatures.
3. **User MESH only for DU1** — no internal pipeline layout changes in DU1.
4. **Harness before default** — opt-in via `SITUATION_BUFFER_USAGE_DYNAMIC_UNIFORM` + size threshold until DU1.2 green; only then consider widening defaults.
5. **Per-frame cursor** — ring cursor is **per `current_frame_index`**, never global (match `dynamic_vbo` / frame-in-flight safety).
6. **Ring overflow** — fail create/update with clear error (or transparent fallback to dedicated), never silent corruption.
7. **Independent of Phase D** — DU1 must not depend on bindless texture sampling; parallel work OK.

---

## Reference implementation in tree

| Pattern | File / symbol | Use for DU1 |
|---------|---------------|-------------|
| Per-frame mapped ring | `dynamic_vbo[]`, `dynamic_vbo_mapped[]` | Buffer + map lifecycle |
| Cursor at draw | text path `target_offset` | Suballoc discipline |
| Per-frame view UBO | `view_proj_ubo_buffer[i]` | DU2 candidate only |
| Dynamic bind API | `SituationCmdBindDescriptorSetDynamic` | Already passes `pDynamicOffsets` |
| Resolver | `_SituationVulkanResolveBufferDescriptor` | Ring bind must still resolve `dynamic_ubo_layout` |

---

## Harness coverage

| Test | Phase | Proves | Status |
|------|-------|--------|--------|
| `descriptor_bind_dynamic_offset` | DU0 | Dynamic offset on **dedicated** UBO | ✅ |
| **`test_ubo_ring_single_draw`** | DU1.2 | Ring suballoc + master set | 🔲 |
| **`test_ubo_ring_multi_draw_same_frame`** | DU1.2 | Versioning / no stall (AAA goal) | 🔲 |
| `test_buffer_device_address` | B | Unrelated; must stay green | ✅ |

---

## Dependency map

```
DU0 dynamic API ✅ (v2.3.29)
    │
    └── DU1 user UBO ring ←── YOU ARE HERE
            │   (harness-gated, MESH profile only)
            ├── DU2 internal view/proj (optional)
            │
            └── plan_handles_ssbo Phase E (mega-shader push block)
                    └── Phase G scaler sprites (north star product)
```

**Parallel (safe):** Phase D0 bindless proof (`plan_handles_ssbo.md`) — different descriptor set / shader path.

---

## Cross-references

| Document | Relationship |
|----------|--------------|
| `doc/plan/plan_handles_ssbo.md` | Phase D/E/G bindless + **scaler sprite north star**; DU feeds G instance stream |
| `doc/plan/VULKAN_SPIRV_USER_DESCRIPTOR_PARITY.md` | MESH profile = `dynamic_ubo_layout` @ set 0 |
| `doc/UPDATELOG.md` v2.3.29 | DU0 shipped |
| `doc/updatelog_23.md` | Original “Velocity (Dynamic UBOs)” release notes |
| `doc/architecture.md` | Frame-in-flight rings, backpressure, render thread |

---

## Original analysis (v2.3.27 — superseded scope)

The sections below remain valid **design intent** for DU1; step numbers map to **DU1.0–1.1** above. Status “Deferred from v2.3.27” applied to the **ring** work only — DU0 landed in v2.3.29 without the ring.

### Benefits (unchanged target)

| Metric | Dedicated + staging (today, GPU-only path) | DU1 ring |
|--------|---------------------------------------------|----------|
| CPU overhead | Staging / single-time cmds | `memcpy` + align |
| GPU sync | Can serialize updates | Parallel suballocs same frame |
| Memory | Per-buffer alloc | Shared ring per frame-in-flight |
| Complexity | Lower | Medium (alignment, overflow) |

### Constraints

- Dynamic UBO range limit per binding (`maxUniformBufferRange`, often 64 KB) — large buffers stay dedicated.
- `minUniformBufferOffsetAlignment` (typically 256 B) — all ring suballocs must align.
- SSBOs are **not** DU1 scope (different layout / no dynamic offset in current resolver).

---

## Current status summary (v2.4.335)

| Phase | Status |
|-------|--------|
| DU0 — Dynamic descriptor API | ✅ Complete (v2.3.29+, MESH profile v2.4.55+) |
| DU1 — User UBO ring | 🔲 Next — harness-gated, non-regressive |
| DU2 — Internal view/proj ring | 🔲 Blocked on DU1 |
