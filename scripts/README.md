# Maintenance scripts

One-off and recurring utilities for the Situation repo. Run from the **repository root** unless noted.

## Python on Windows

Prefer **MSYS MinGW `python3`** (3.10+). Plain `python` on PATH is often Inkscape or Windows Store **3.8** and may fail on scripts that need newer stdlib (or behave differently).

```powershell
& "C:\msys64\mingw64\bin\python3.exe" scripts\<script>.py
```

On MSYS/Linux shells: `python3 scripts/<script>.py`

Individual scripts may document extra pip deps (e.g. Pillow for `gen_situation_icon.py`).

---

## Generated artifacts (commit output with code changes)

| Script | Output | When |
|--------|--------|------|
| `gen_situation_base_trace.py` | `sit/situation_base_trace.h` | After add/rename/remove of function bodies under `sit/` |
| `gen_situation_icon.py` | `sit/situation_icon.ico` | After changing `icon_source.PNG` (needs Pillow) |
| `gen_spirv_embed.ps1` | Harness SPIR-V C embed | After recompiling harness shaders (see script params) |
| `gen_demon_hunt_spirv_embed.ps1` | Demon Hunt SPIR-V embed | Same, for demon hunt shaders |

---

## Documentation

| Script | Purpose |
|--------|---------|
| `generate_situation_api_docs.py` | Shim → `tools/generate_api_index.py` |
| `verify_doc_links.py` | Check relative `doc/*.md` anchor links |
| `split_updatelog_chunks.py` | Split `doc/UPDATELOG.md` + v2.3 archive into `updatelog_{23,24}_NN.md` chunks (~100 patches) and regenerate index |
| `read_situation_version.py` | Read `sit/situation_base_version.h` → `--windres` / `--string` / `--make` for PE stamping |

---

## Errno audit

| Script | Purpose |
|--------|---------|
| `audit_errno.ps1` | Compare `situation_base_errno.h` vs usage in `sit/` + `tests/` |
| `audit_errno_report.ps1` | Full unused-errno report → `doc/ERRNO_USAGE_REPORT.md` |

```powershell
& ".\scripts\audit_errno_report.ps1"
```

---

## Verification

| Script | Purpose |
|--------|---------|
| `verify_renderer_fwd.py` | `situation_impl_renderer_fwd.h` matches static defs in renderer |
| `verify_impl_forward.py` | `situation_impl_forward.h` matches non-renderer static forward decls |
| `inventory_renderer_module.py` | Post-split LOC / statics / SITAPI audit per renderer slice |
| `audit_renderer_cross_slice.py` | Cross-slice symbol coupling report (docs / PR visibility) |
| `audit_siamese_colocation.py` | GL execute helpers colocated with record twins (Siamese plan gate) |
| `verify_doc_links.py` | Internal markdown link integrity |
| `verify_kterm_console.bat` | KTerm console smoke check |

---

## Active migration tooling

| Script | Purpose |
|--------|---------|
| `colocate_gl_execute.py` | Mechanical GL execute colocation (`RENDERER_SIAMESE_COLOCATION_PLAN.md`) |

---

## Source / build helpers

| Script | Purpose |
|--------|---------|
| `concat_situation.ps1` / `concat_situation.sh` | Single-file concat of `sit/` for distribution |
| `spirv_shader_debug.py` | Offline SPIR-V/GLSL analysis (see script docstring for subcommands) |
| `spirv_desc_spike.py` | Descriptor set/binding spike vs GLSL |

## Wrapper build helpers (`wrapper_*.bat`)

Shared by `build\build_*_example.bat` entry points. Call from repo root via the build scripts — not usually invoked directly.

| Script | Language | Role |
|--------|----------|------|
| `wrapper_link_config.bat` | all | Backend → `SIT_DLL_*`, static archive paths |
| `wrapper_paths.bat` | all | `build\examples\<lang>\` output + `build\obj\<lang>\` intermediates |
| `wrapper_mingw_setup.bat` | C-linked | Add MinGW-w64 `bin` to `PATH` |
| `wrapper_ensure_import_lib.bat` | DLL link | Generate MinGW `.lib` from Situation DLL |
| `wrapper_link_dll.bat` | Odin/Zig/Rust/Fortran/Modula2 | Link against `build\dll\situation_*.lib`, copy DLL |
| `wrapper_gcc_link_static.bat` | Fortran/Modula2/Rust | Self-contained static OpenGL/Vulkan link |
| `wrapper_compile_fortran.bat` | Fortran | Compile bindings + example |
| `wrapper_compile_modula2.bat` | Modula-2 | Compile bindings + example |
| `wrapper_compile_python.bat` | Python | Stage + `python -m compileall` |
| `wrapper_link_python.bat` | Python | PyInstaller `--onefile` + copy DLL |
| `wrapper_compile_lua.bat` | Lua | Stage sources; `gen_lua_embed.py` + `gen_lua_dll_embed.py` |
| `wrapper_link_lua.bat` | Lua | Link embedded host → single `.exe` |
| `wrapper_patch_odin_foreign.bat` / `wrapper_restore_odin_foreign.bat` | Odin | Patch `foreign import` for active backend |

See **`doc/COMPILATION_GUIDE.md`** → *Language Wrappers* and each `wrappers/<lang>/README.md`.

---

## One-offs (archived in `_old/`)

Completed plan migrations and rare dev helpers live in **`scripts/_old/`** — kept for reference, not day-to-day use.

| Script | Purpose |
|--------|---------|
| `extract_renderer_*.py` | R0–R5 mechanical slice extracts (re-run only on pre-split monolith) |
| `audit_renderer_*_extract.py` | Per-slice extract integrity audits (R1–R5) |
| `census_renderer_r5.py` | Pre-R5 monolith stub census |
| `patch_gl_soft_calls.py` | Migrate GL soft-cmd push call sites (Phase 3) |
| `tag_phase9_hardening.py` | Prepend HARDENING comments on void forward decls |
| `audit_phase10_caller.py` | Find ignored `SituationError` returns in init tree |
| `port_leftover_api_docs.py` | Port remaining SITAPI docs into `doc/situation_api.md` |
| `collapse_api_cmd_docs.py` | Collapse legacy `SituationCmd*` sections to link table |
| `fix_api_md_structure.py` | Repair `<details>` structure after bulk doc insert |
| `split_updatelog_v23.py` | Superseded — redirects to `split_updatelog_chunks.py` |
| `list_internal_voids.ps1` | CSV inventory of internal void helpers |
| `check_shader_len.py` | Modula2 wrapper shader string length check (one-off) |
| `_scan_modified_today.py` | Dev helper — files modified today |
| `lastmod.ps1` | File last-mod times |

---

## Header convention

Python generators that write committed files should document in the module docstring:

- **What** it generates
- **When** to run it
- **Usage** (repo-root command)
- **Output path(s)**
- **Windows python3 note** if stdlib ≥ 3.10 features are used

See `gen_situation_icon.py` and `gen_situation_base_trace.py` as templates.
