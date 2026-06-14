# Situation tooling

Generators for API documentation and language bindings. All tools read **`sit/situation_api.h`** as the single source of truth.

## Layout

| Tool | Output |
|------|--------|
| [`generate_api_index.py`](generate_api_index.py) | `doc/situation_api_index.md`, `doc/situation_api_generated.md` |
| [`generate_odin_bindings.py`](generate_odin_bindings.py) | `wrappers/odin/*.odin`, `wrappers/odin/API_INDEX.md` |
| [`generate_zig_bindings.py`](generate_zig_bindings.py) | `wrappers/zig/src/*.zig`, `wrappers/zig/API_INDEX.md` |
| [`generate_rust_bindings.py`](generate_rust_bindings.py) | `wrappers/rust/src/*.rs`, `wrappers/rust/API_INDEX.md` |
| [`situation_api_parser.py`](situation_api_parser.py) | Shared parser (import only) |
| [`binding_common.py`](binding_common.py) | Shared type/signature utilities |
| [`run_all.bat`](run_all.bat) | Regenerates API index + all bindings in one shot |

## Quick start

```bat
REM API markdown index
python tools\generate_api_index.py

REM Language bindings
python tools\generate_odin_bindings.py
python tools\generate_zig_bindings.py
python tools\generate_rust_bindings.py

REM Or run everything at once:
tools\run_all.bat
```

## Language bindings

1. Build the DLL first: `build\build_situation.bat opengl` (or `vulkan`)
2. Generate: `python tools\generate_<lang>_bindings.py`
3. See each wrapper README:
   - [`wrappers/zig/README.md`](../wrappers/zig/README.md)
   - [`wrappers/rust/README.md`](../wrappers/rust/README.md)
   - `wrappers/odin/` — see `wrappers/odin/API_INDEX.md` after generation

All three generators share the same patterns:

- Curated ABI-critical structs in a types module
- Auto opaque stubs for referenced internal types
- `#define` → enum value resolution via `build_define_map()`
- Variadic symbols → `MANUAL_BINDINGS.md` (not exported to FFI)

## When to re-run

After any change to `sit/situation_api.h`, `sit/situation_base_errno.h`, or public `#define` keys in `sit/situation_base_etc.h`:

```bat
tools\run_all.bat
python scripts\verify_doc_links.py
```

## Parser notes

- **Variadic** functions (`...`) are listed in `MANUAL_BINDINGS.md`, not exported to foreign/FFI.
- **Callback registration** APIs with nested proc types may need hand wrappers.
- Complex structs use **curated** layouts in the types module (ABI-critical paths). Extend manually when adding new APIs.
- `situation_api_parser.py` and `binding_common.py` are shared by all three generators — changes affect all outputs.
