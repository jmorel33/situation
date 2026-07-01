# Renderer Siamese Colocation Plan — GL/VK Twin Placement

| Field | Value |
|-------|--------|
| **Status** | 🟡 **IN PROGRESS** — S2 pilot ✅ @ v2.4.361; Track C closed @ v2.4.362 |
| **Goal** | Keep OpenGL and Vulkan paths for the **same operation** in the **same file**, adjacent in source — not split across `renderer_lc.h` vs `renderer_frame_cmd.h` |
| **Builds on** | [`RENDERER_MODULARIZATION_PLAN.md`](../done/RENDERER_MODULARIZATION_PLAN.md) (domain slices ✅) |
| **Pairs with** | [`LIBRARY_RECOVERY_PLAN_244.md`](LIBRARY_RECOVERY_PLAN_244.md) Track C — **closed @ 362** (readback; not compositor) |
| **Explicit non-goal** | Global `renderer_gl.h` / `renderer_vk.h` backend fork files (closed policy) |

---

## Why this plan exists

Domain modularization (R0–R5) split the renderer by **subsystem** (`core`, `lc`, `shader`, `resources`, `frame_cmd`). That was correct for merge/review ownership.

It accidentally split **backend twins**:

| Operation | Vulkan (record) | OpenGL (record) | OpenGL (execute) | Problem |
|-----------|-----------------|-----------------|------------------|---------|
| `SituationCmdBeginRenderPass` | `vkCmdBeginRenderPass` in **`frame_cmd`** | `SIT_GL_SOFT_CMD_PUSH` in **`frame_cmd`** | `case SIT_OP_BEGIN_RENDER_PASS` in **`lc`** execute switch | VK + GL record together ✅; GL execute **~1,800 lines away** ❌ |
| `SituationRenderVirtualDisplays` | inline VK draw in **`vd.h`** | push opcode in **`vd.h`** | `case SIT_OP_RENDER_VIRTUAL_DISPLAYS` in **`lc`** | GL execute body far from VK twin ❌ |
| `SituationEndFrame` | `_SituationSubmitGraphics` in **`frame_cmd`** | calls `_SituationGLExecuteCommands` in **`lc`** | defined in **`lc`** | frame boundary split across files ❌ |

**Symptom (historical):** parity fixes required jumping between files; order-dependent VD pixel failures **looked** like compositor bugs but were **readback @ 362**.

**Siamese twins policy (product):**

- GL and VK are **different worlds** — they will never share one implementation.
- They **must stay born together in source**: same function or same file region, same PR, same review glance.
- **Harness:** same test, two backend builds — comparison is how we find bugs.
- **Not** separate backend files — that invites drift.

---

## Target pattern (per operation)

Within the **owning domain slice**, each hot-path operation follows:

```c
SITAPI SituationError SituationCmdDraw(..., SituationCommandBuffer cmd, ...) {
#if defined(SITUATION_USE_VULKAN)
    vkCmdDraw(...);                    /* twin A: immediate record */
#elif defined(SITUATION_USE_OPENGL)
    SIT_GL_SOFT_CMD_PUSH(buf, SIT_OP_DRAW, p);   /* twin B: defer */
#endif
}

#if defined(SITUATION_USE_OPENGL)
/* GL execute twin — immediately below record twin, same file */
static SituationError _SitGLExecDraw(SituationGLSoftCommandBuffer* buf,
                                     SitCommandPacket* p, int frame_index) {
    /* body moved from _SituationGLExecuteCommands switch */
}
#endif
```

**Dispatcher** (thin, stays one function):

```c
static SituationError _SituationGLExecuteCommands(SituationGLSoftCommandBuffer* buf, int frame_index) {
    /* baseline raster reset — once at top */
    for each packet:
        switch (opcode) {
            case SIT_OP_DRAW: return _SitGLExecDraw(buf, p, frame_index);
            ...
        }
}
```

After colocation, **reviewers see record + execute for GL next to `vkCmd*` for VK** in one scroll.

---

## What stays where (domain slices unchanged)

**Do not** revert R0–R5 include order or create backend fork files.

| Slice | Still owns | After colocation |
|-------|------------|------------------|
| **`renderer_core.h`** | uniform map, staging, graveyard, GL backup/restore helpers | unchanged |
| **`renderer_shader.h`** | shader load/compile, pipelines (GL+VK inline) | already mostly twinned ✅ |
| **`renderer_resources.h`** | buffers, textures, meshes (GL+VK inline) | already mostly twinned ✅ |
| **`renderer_frame_cmd.h`** | acquire/end, **`SituationCmd*`**, model I/O | **+ GL execute helpers** for frame opcodes; **+ soft-cmd push** moved from `lc` |
| **`renderer_lc.h`** | init/shutdown, render thread, backend bootstrap, internal renderer init | **− bulk execute switch**; keeps `_SituationRenderThreadEntry` calling into `frame_cmd` dispatcher |
| **`situation_impl_vd.h`** | VD public API + compositor | **+ GL execute body** for `SIT_OP_RENDER_VIRTUAL_DISPLAYS` directly under `SituationRenderVirtualDisplays` |

**Include order (unchanged):** `core → lc → shader → resources → frame_cmd`

`frame_cmd` is included **last**, so it may call static helpers defined in earlier slices. Colocated `_SitGLExec*` functions live in the slice that owns the **public entry** (`frame_cmd` or `vd.h`).

---

## Inventory — split twins to fix

Mechanical audit (2026-06-25):

| ID | GL execute site | Record / VK twin site | Priority |
|----|-----------------|----------------------|----------|
| **T-VD** | `lc.h` `SIT_OP_RENDER_VIRTUAL_DISPLAYS` (~2080+) | `vd.h` `SituationRenderVirtualDisplays` | **P0** — Track C / visible misalignment |
| **T-FRAME** | `lc.h` `_SituationGLExecuteCommands` (~769–2590, **52** `case SIT_OP_*`) | `frame_cmd.h` matching `SituationCmd*` / pushes | **P1** — bulk colocation |
| **T-PUSH** | `lc.h` `_SitGLSoftCmdPush` (~505) | `frame_cmd.h` all `SIT_GL_SOFT_CMD_PUSH` | **P1** — move push next to dispatcher in `frame_cmd` |
| **T-END** | `lc.h` render-thread submit (~10538) | `frame_cmd.h` `SituationEndFrame` + `_SituationSubmitGraphics` | **P2** — colocate frame submit block; thread entry stays thin in `lc` |
| **T-INIT** | `lc.h` GL/VK bootstrap | same file | ✅ already twinned |

**Opcode count:** 52 `case SIT_OP_*` arms in `_SituationGLExecuteCommands` (grep `lc.h`).

---

## Phases

### S0 — Prep (no code move)

- [ ] **S0.1** Add steering bullet + link to this plan (backend colocation policy).
- [ ] **S0.2** Baseline: `python scripts/inventory_renderer_module.py`; GL+VK full suite counts (recovery plan matrix).
- [ ] **S0.3** Baseline static count: `python scripts/verify_renderer_fwd.py` (**347/347**).
- [ ] **S0.4** Document opcode → anchor map (script output) in this plan §Migration log.

### S1 — Tooling (`scripts/colocate_gl_execute.py`)

Mechanical helper modeled on `extract_renderer_*.py`:

1. Parse `_SituationGLExecuteCommands` switch in `situation_impl_renderer_lc.h`.
2. Extract each `case SIT_OP_*: { ... } break;` into a standalone `static SituationError _SitGLExec_<Opcode>(...)`.
3. Map opcode → insertion anchor:
   - Default: after the `SituationCmd*` / helper that emits that opcode in `frame_cmd.h`.
   - Special: `SIT_OP_RENDER_VIRTUAL_DISPLAYS` → after `SituationRenderVirtualDisplays` in `vd.h`.
4. Replace switch arm with `return _SitGLExec_<Opcode>(buf, p, frame_index);`.
5. Emit `scripts/audit_siamese_colocation.py` — fail if any `_SitGLExec_*` definition is not in the same file as its `SIT_OP_*` push or `vkCmd` twin (within N lines or same file).

**Flags:** `--dry-run`, `--opcode SIT_OP_RENDER_VIRTUAL_DISPLAYS` (single-op pilot), `--write`.

- [x] **S1.1** Implement `colocate_gl_execute.py` (pilot: one opcode).
- [x] **S1.2** Implement `audit_siamese_colocation.py`.
- [ ] **S1.3** Wire audit into optional CI / pre-commit doc (same tier as `verify_renderer_fwd.py`).

### S2 — Pilot: VD composite (P0, Track C)

**Why first:** visible GL misalignment; VK perfect; twins currently split `vd.h` ↔ `lc.h`.

- [x] **S2.1** Run `colocate_gl_execute.py --opcode SIT_OP_RENDER_VIRTUAL_DISPLAYS --write`.
- [x] **S2.2** Place `_SitGLExecRenderVirtualDisplays` **immediately under** `SituationRenderVirtualDisplays` in `situation_impl_vd.h` (GL-only `#if`).
- [x] **S2.3** `audit_siamese_colocation.py` green for T-VD.
- [x] **S2.4** GL+VK builds; harness: `--module virtual_display` — **GL 34/34**, **VK 34/34** @ v2.4.362 (readback fix; mechanical S2 move did not regress).
- [x] **S2.5** UPDATELOG — v2.4.361–362 entries (mechanical S2 + readback hardening).

**Exit:** VD opcode body lives in `vd.h`; no duplicate static defs; twins in one file.

### S3 — Soft-buffer infra to `frame_cmd`

- [ ] **S3.1** Move `_SitGLSoftCmdPush` + `SIT_GL_SOFT_CMD_PUSH` macro from `lc.h` → `frame_cmd.h` (above first `SituationCmd*` user).
- [ ] **S3.2** Update `renderer_fwd.h` section banners if needed (still single fwd file).
- [ ] **S3.3** `verify_renderer_fwd.py` green; builds green.

### S4 — Bulk opcode colocation (P1)

Batch by harness module to limit blast radius:

| Batch | Opcodes (representative) | Harness gate |
|-------|--------------------------|--------------|
| **S4a** | `BEGIN/END_RENDER_PASS`, `CLEAR`, viewport, scissor | `graphics --filter pattern` |
| **S4b** | `DRAW*`, `BIND_*`, raster stack | `graphics`, `model_loader` |
| **S4c** | `DRAW_QUAD`, `DRAW_TEXT*`, internal 2D | `text_rendering`, VD tests |
| **S4d** | compute: `DISPATCH*`, `PIPELINE_BARRIER`, bind compute | `compute` module |
| **S4e** | remainder + `PRESENT` | full GL suite |

Per batch:

- [ ] Run `colocate_gl_execute.py --batch <name> --write`.
- [ ] Shrink `_SituationGLExecuteCommands` switch to dispatch-only calls.
- [ ] `audit_siamese_colocation.py` + `verify_renderer_fwd.py` + GL+VK build + module harness.

**Exit:** `_SituationGLExecuteCommands` in `frame_cmd.h` (~100 lines: reset + dispatch loop); **zero** `case SIT_OP_*` bodies remain in `lc.h`.

### S5 — Frame boundary twins (P2)

- [ ] **S5.1** Group in `frame_cmd.h`: `SituationEndFrame` GL path (`_SituationGLExecuteCommands`, canvas blit, swap) adjacent to VK path (`_SituationSubmitGraphics`, present).
- [ ] **S5.2** `renderer_lc.h` `_SituationRenderThreadEntry` — thin: dequeue frame → call `frame_cmd` submit helpers (no opcode bodies).
- [ ] **S5.3** Document render-thread call graph in `architecture.md`.

### S6 — Close + steering

- [ ] **S6.1** `audit_siamese_colocation.py` green repo-wide.
- [ ] **S6.2** Update [`RENDERER_MODULARIZATION_PLAN.md`](../done/RENDERER_MODULARIZATION_PLAN.md) §Explicitly deferred — link here instead of “backend fork”.
- [x] **S6.3** Recovery plan Track C notes: **closed @ 362** — edit VD composite in **`vd.h`** (GL+VK twins); readback in `frame_cmd`/`lc`/`resources`.
- [ ] **S6.4** Version bump + whatsnew when all batches shipped.

---

## Regression rules (non-negotiable)

| Rule | Gate |
|------|------|
| **Mechanical move only** per batch — no behaviour change in colocation PRs | diff review; Track C fixes are **separate** commits |
| **No `renderer_gl.h` / `renderer_vk.h`** | audit script + plan policy |
| **`verify_renderer_fwd.py` green** | 348/348 static parity (@ v2.4.361+) |
| **`audit_siamese_colocation.py` green** | after S2 |
| **GL + VK build** | `build_situation.bat opengl` + `vulkan` |
| **Harness** | module gates in §S4 table; full suite before S6 |
| **Include order unchanged** | orchestrator still `core → lc → shader → resources → frame_cmd` |
| **Single TU** | no new `.c` files |

---

## Explicitly deferred (not this plan)

| Idea | Decision |
|------|------------|
| Global `renderer_gl.h` / `renderer_vk.h` | **Won't do** — drift risk |
| Merge domain slices back into monolith | **Won't do** |
| Collapse GL soft-buffer model to immediate GL | **Won't do** — architectural change |
| Colocate shader compile GL/VK from `shader.h` into `frame_cmd` | **No** — already twinned in `shader.h` |

---

## Verification commands

```powershell
python scripts/verify_renderer_fwd.py
python scripts/inventory_renderer_module.py
python scripts/audit_siamese_colocation.py      # after S1

& "C:\Users\User\Desktop\hobby\_kiro\situation\build\build_situation.bat" opengl
& "C:\Users\User\Desktop\hobby\_kiro\situation\build\build_situation.bat" vulkan
& "C:\Users\User\Desktop\hobby\_kiro\situation\build\run_tests.bat" opengl --headless
& "C:\Users\User\Desktop\hobby\_kiro\situation\build\run_tests.bat" vulkan --headless
```

Debug (Track C):

```powershell
& "C:\Users\User\Desktop\hobby\_kiro\situation\debug.bat" opengl --module virtual_display --break SituationRenderVirtualDisplays
```

---

## Migration log

| Date | Event |
|------|--------|
| 2026-06-25 | Plan authored post v2.4.360 — domain split complete; twin split identified (`lc` execute vs `frame_cmd` record) |
| 2026-06-25 | **S2 pilot landed** — `_SitGLExecRenderVirtualDisplays` colocated in `situation_impl_vd.h`; dispatch-only case in `renderer_lc.h`; forward decl in `situation_impl_forward.h` |
| 2026-06-25 | **Track C closed @ 362** — VD 34/34 GL; full suite GL+VK green; root cause readback not compositor |

---

## Cross-links

- Domain slice map: [`RENDERER_MODULARIZATION_PLAN.md`](../done/RENDERER_MODULARIZATION_PLAN.md) §Target architecture
- GL VD / readback: [`LIBRARY_RECOVERY_PLAN_244.md`](LIBRARY_RECOVERY_PLAN_244.md) Track C — **closed @ 362**
- Canvas stretch / readback: [`CANVAS_STRETCH_READBACK_FIX_PLAN.md`](../done/CANVAS_STRETCH_READBACK_FIX_PLAN.md)
- Viewport parity: [`VULKAN_VIEWPORT_SCISSOR_SANITISATION_PLAN.md`](VULKAN_VIEWPORT_SCISSOR_SANITISATION_PLAN.md)
