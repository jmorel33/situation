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

| [`jam_api_slice.txt`](jam_api_slice.txt) | Jam-tier symbol list for `--jam` exports |



## Quick start



```bat

REM API markdown index (replaces scripts\generate_situation_api_docs.py)

python tools\generate_api_index.py



REM Full Odin FFI (531 foreign procs)

python tools\generate_odin_bindings.py



REM Full Zig FFI

python tools\generate_zig_bindings.py



REM Full Rust FFI

python tools\generate_rust_bindings.py



REM Jam slice (~35 procs) per language

python tools\generate_odin_bindings.py --jam

python tools\generate_zig_bindings.py --jam

python tools\generate_rust_bindings.py --jam



REM Or run everything:

tools\run_all.bat

```



`scripts\generate_situation_api_docs.py` remains as a **shim** that calls `tools\generate_api_index.py` for existing docs/workflows.



## Language bindings



1. Build the DLL: `build_situation.bat opengl`

2. Generate: `python tools\generate_<lang>_bindings.py`

3. See each wrapper README:

   - [`wrappers/odin/README.md`](../bindings/odin/README.md) (entry) / [`wrappers/odin/`](../wrappers/odin/)

   - [`wrappers/zig/README.md`](../wrappers/zig/README.md)

   - [`wrappers/rust/README.md`](../wrappers/rust/README.md)



All three generators share the same patterns (from the Odin generator):



- Curated ABI-critical structs in types module

- Auto opaque stubs for referenced internal types

- `#define` → enum value resolution via `build_define_map()`

- Variadic symbols → `MANUAL_BINDINGS.md` (not exported)

- `--jam` filter via `jam_api_slice.txt`



## When to re-run



After any change to `sit/situation_api.h`, `sit/situation_base_errno.h`, or public `#define` keys in `sit/situation_base_etc.h`:



```bat

tools\run_all.bat

python scripts\verify_doc_links.py

```



## Parser notes



- **Variadic** functions (`...`) are listed in `MANUAL_BINDINGS.md`, not exported to foreign/FFI.

- **Callback registration** APIs with nested proc types may need hand wrappers.

- Complex structs use **curated** layouts in the types module (ABI-critical paths). Extend manually when adding new jam APIs.

