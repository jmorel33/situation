# Situation — Rust bindings

Auto-generated FFI for the Situation C library (`situation_opengl.dll` / `situation_vulkan.dll`).

## Generate

```bat
build_situation.bat opengl
python tools\generate_rust_bindings.py
```

Jam-tier only:

```bat
python tools\generate_rust_bindings.py --jam
```

Vulkan import lib:

```bat
python tools\generate_rust_bindings.py --lib situation_vulkan
```

## Output

Generated files live in `wrappers/rust/src/`:

| File | Role |
|------|------|
| `lib.rs` | Crate root (re-exports; regenerated on full build) |
| `situation_types.rs` | `#[repr(C)]` structs and enums |
| `situation_ffi.rs` | `extern "C" { ... }` block (~531 fns) |
| `situation_ffi_jam.rs` | Jam-tier imports (`--jam`) |
| `situation_callbacks.rs` | `Option<unsafe extern "C" fn(...)>` aliases |
| `situation_constants.rs` | `SIT_KEY_*`, etc. |
| `situation_helpers.rs` | `situation_begin_frame()`, etc. |
| `build.rs` | Links `../../build/dll/situation_opengl.lib` |
| `API_INDEX.md` | Per-symbol index |
| `MANUAL_BINDINGS.md` | Variadic / hand-wrap symbols |

## Use from Rust

```rust
use situation::*;

fn main() {
    let mut config = SituationInitInfo {
        window_width: 1280,
        window_height: 720,
        window_title: c"Hello from Rust".as_ptr(),
        ..unsafe { std::mem::zeroed() }
    };
    unsafe {
        SituationInit(0, std::ptr::null(), &mut config);
    }
    // ...
}
```

Build from `wrappers/rust/`:

```bat
cargo build --example hello_situation
```

Copy `situation_opengl.dll` next to the built executable.

## Maintenance

Do not hand-edit generated `.rs` files (except optional safe wrapper layer in a separate module). Edit `tools/generate_rust_bindings.py` or `tools/situation_api_parser.py`, then re-run.

The raw `situation_ffi.rs` layer is `unsafe` by design. Add a `safe.rs` module later for RAII handles and `Result<T, SituationError>` wrappers.
