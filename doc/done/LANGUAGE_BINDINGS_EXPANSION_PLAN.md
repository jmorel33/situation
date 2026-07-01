# Language Bindings Expansion Plan — Zig & Rust

**Status:** Planning (future v2.5+ milestone)
**Depends on:** Odin bindings stabilized (✅ proven), `situation_api_parser.py` mature (✅)

---

## Overview

Extend Situation's reach beyond C and Odin to **Zig** and **Rust** — the two fastest-growing systems languages in the gamedev/creative-coding space. Both consume C DLLs natively, so the architecture is identical to Odin: auto-generated FFI wrappers driven by `tools/situation_api_parser.py`.

---

## Architecture (Shared)

```
sit/situation_api.h
        │
        ▼
tools/situation_api_parser.py  ← single source of truth (already parses 531 SITAPI functions)
        │
        ├──► tools/generate_odin_bindings.py   → wrappers/odin/    ✅ DONE
        ├──► tools/generate_zig_bindings.py    → wrappers/zig/     ✅ DONE
        └──► tools/generate_rust_bindings.py   → wrappers/rust/    ✅ DONE
```

Each generator reads the same `ApiEntry` list and emits idiomatic FFI for its target language. The DLL + import lib (`build/dll/situation_opengl.lib`) is shared across all.

---

## Phase 1: Zig Bindings

### Why Zig First
- Zig's `@cImport` can consume C headers directly, but a curated wrapper is better UX
- Zig links `.lib` files natively (no extra tooling like MSVC needed)
- Zig's build system (`build.zig`) makes dependency management trivial
- Growing adoption in gamedev (Zig game jams, Mach engine community)

### Output Structure
```
wrappers/zig/
├── build.zig              ← build script (links situation_opengl.lib)
├── src/
│   ├── situation.zig      ← package root (pub usingnamespace c)
│   ├── c.zig              ← raw C FFI (extern "c" fn declarations)
│   ├── types.zig          ← structs, enums, handles
│   └── helpers.zig        ← Zig-idiomatic wrappers (slices, optionals, error unions)
├── examples/
│   └── hello_situation/main.zig
└── README.md
```

### Generator Tasks
- [x] `tools/generate_zig_bindings.py` — emit `extern "c"` function declarations
- [x] Map C types: `void*` → `?*anyopaque`, `char*` → `[*:0]const u8`, `bool` → `bool`
- [ ] Map `SituationError` → Zig error union pattern (`fn init() !void` style wrapper) — helpers only for now
- [x] Emit struct definitions with `extern struct` layout
- [x] Emit enums as `pub const Enum = enum(c_int) { ... }`
- [x] Generate `build.zig` that links the `.lib` and adds include paths
- [x] Comments from API header preserved as `///` doc comments

### Zig-Specific Idioms
- Error returns: wrap `SituationError` functions as `fn foo() Error!ReturnType`
- Slices: where C uses `ptr + count`, expose as `[]const T` in the wrapper layer
- Optional pointers: `?*T` for nullable C pointers
- Packed structs for ABI-sensitive types (ColorRGBA)

### Validation
- [ ] `zig build` compiles without errors
- [ ] Hello-world example: init → shader → render → shutdown (same as Odin demo)
- [ ] Zero crashes on the same test path

---

## Phase 2: Rust Bindings

### Why Rust
- Largest community of the three target languages
- `cargo` ecosystem means easy distribution (crates.io potential)
- Safety guarantees complement Situation's "main-thread discipline" model
- Rust gamedev is active (Bevy users wanting lower-level access)

### Output Structure
```
wrappers/rust/
├── Cargo.toml             ← crate definition
├── build.rs               ← build script (links situation_opengl.lib)
├── src/
│   ├── lib.rs             ← crate root, re-exports
│   ├── ffi.rs             ← raw `extern "C"` bindings (unsafe)
│   ├── types.rs           ← #[repr(C)] structs, enums
│   ├── error.rs           ← SituationError → Result<T, Error> conversion
│   └── safe.rs            ← safe wrappers (lifetime-bound handles, RAII)
├── examples/
│   └── hello_situation.rs
└── README.md
```

### Generator Tasks
- [x] `tools/generate_rust_bindings.py` — emit `extern "C" { fn ... }` blocks
- [x] Map C types: `void*` → `*mut c_void`, `char*` → `*const c_char`, `bool` → `bool`
- [x] `#[repr(C)]` structs matching ABI exactly
- [x] `#[repr(i32)]` enums for C int enums
- [ ] `SituationError` → `Result<T, SituationError>` in safe wrappers — helpers only for now
- [ ] RAII: `Drop` impl for `SituationShader`, `SituationAudioGraph*`, etc. — future safe layer
- [x] `build.rs` with `println!("cargo:rustc-link-lib=situation_opengl")`
- [x] Doc comments from API header as `///` rustdoc

### Rust-Specific Idioms
- Unsafe raw layer (`ffi.rs`) + safe public API (`safe.rs`)
- `Drop` traits for resources (shader, graph, texture) — automatic cleanup
- `Send`/`!Send` markers: graph pointers are `!Send` (main-thread only)
- Builder pattern for `SituationInitInfo`
- `thiserror` or manual `impl Display` for error types

### Validation
- [ ] `cargo build` compiles without errors
- [ ] `cargo clippy` passes
- [ ] Same hello-world demo path as Odin/Zig
- [ ] Zero crashes

---

## Phase 3: Shared Infrastructure Improvements

Before or during Phase 1-2, harden the shared parser:

- [ ] **Struct layout extraction** — `situation_api_parser.py` already parses structs but doesn't emit field offsets. Add `sizeof`/`offsetof` validation for ABI-critical types.
- [ ] **Callback type generation** — already working for Odin; ensure Zig/Rust get equivalent `fn` pointer types.
- [ ] **Constant extraction** — `#define SIT_KEY_*` → language-native constants (already done for Odin).
- [ ] **CI validation** — script that runs all three generators and `check`/`build` commands to catch regressions when the C API changes.

---

## Build Scripts (Per Language)

All three scripts take **`[backend] [example_name]`** — same backends as `build_examples.bat`: `opengl`, `vulkan`, `static-opengl`, `static-vulkan`. Shared helpers in `scripts/wrapper_*.bat`. See `doc/COMPILATION_GUIDE.md`.

| Language | Build Script | What It Does |
|----------|-------------|--------------|
| Odin | `build_odin_example.bat [backend] [example]` | DLL: gendef → dlltool → odin build → copy DLL. Static: obj build → GCC link |
| Zig | `build_zig_example.bat [backend] [example]` | `build.zig` (`-Dlink=`, `-Dexample=`); static uses GNU target |
| Rust | `build_rust_example.bat [backend] [example]` | `cargo build`; `build.rs` reads `SITUATION_LINK`; static uses GCC linker |

---

## Timeline Estimate

| Phase | Effort | Notes |
|-------|--------|-------|
| Zig generator + example | ~1 session | Zig FFI is simpler than Odin (no package weirdness) |
| Rust generator + example | ~2 sessions | Safe wrapper layer adds complexity |
| CI validation script | ~30 min | Run generators + check/build |

---

## Success Criteria

All three languages (Odin, Zig, Rust) can:
1. Auto-generate bindings from the same `situation_api.h` with one command
2. Compile and link against the pre-built DLL
3. Run the same demo (window + shader + audio + MIDI) with zero crashes
4. Rebuild bindings after any API change in under 5 seconds

---

## Notes

- **No runtime overhead**: All bindings are thin FFI layers — zero-cost abstraction over the DLL calls
- **Single source of truth**: `sit/situation_api.h` is the canonical definition; all generators read from it
- **Versioned**: Generated files include the Situation version string; mismatch = compile error (type changes) or runtime version check
- **Distribution**: Each `wrappers/<lang>/` folder is self-contained and redistributable independently of the full Situation source tree
