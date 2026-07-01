# Situation Lua bindings

LuaJIT **FFI** bindings for the Situation C library (`situation_opengl.dll` / `situation_vulkan.dll`).

## Regenerate bindings

```bat
python tools\generate_lua_bindings.py
```

Or run all generators:

```bat
tools\run_all.bat
```

## Build hello_situation.exe

The Lua wrapper ships as a **single self-contained executable**. On launch it extracts embedded DLLs to `%TEMP%\situation_lua_<pid>\` — nothing needs to sit beside the `.exe`.

| Step | Script | What it does |
|------|--------|----------------|
| Generate bindings | `python tools\generate_lua_bindings.py` | Regenerate `wrappers/lua/situation/` from `situation_api.h` |
| Build library | `build\build_situation.bat opengl` | Produce `build\dll\situation_opengl.dll` |
| **Compile** | `scripts\wrapper_compile_lua.bat` | Stage sources; embed Lua bytecode + Situation/lua51 DLL blobs |
| **Link** | `scripts\wrapper_link_lua.bat` | Link embedded host → `build\examples\lua\<example>.exe` |
| Run | `build\build_lua_example.bat opengl` | Full pipeline; runs the demo |

**Prerequisites:**

- MSYS2 MinGW-w64 (`gcc`, `g++`) — links the embedded host
- Bundled LuaJIT at `_languages/lua/luajit/bin/lua51.dll` (see `_languages/lua/README.md`)
- Python 3 — runs `tools/gen_lua_embed.py` and `tools/gen_lua_dll_embed.py`

```bat
build\build_situation.bat opengl
python tools\generate_lua_bindings.py
build\build_lua_example.bat opengl hello_situation --no-run
build\build_lua_example.bat opengl hello_situation
```

**Output** (only file in the folder):

```
build/examples/lua/
└── hello_situation.exe
```

Run directly:

```bat
build\examples\lua\hello_situation.exe
```

Vulkan:

```bat
build\build_situation.bat vulkan
build\build_lua_example.bat vulkan hello_situation
```

### Runtime extraction

At startup the host extracts:

| Blob | Purpose |
|------|---------|
| `situation_opengl.dll` or `situation_vulkan.dll` | Situation library (from `build/dll/` at link time) |
| `lua51.dll` | LuaJIT runtime (loaded dynamically — avoids static-link instability) |

The host sets `SIT_LUA_DLL_PATH` to the extracted Situation DLL before Lua runs. Temp files are removed on clean shutdown.

### Dev mode (sources, external DLL)

For fast iteration on `.lua` files without rebuilding the embedded exe:

```bat
build\run_lua_dev.bat
```

Expects `build\dll\situation_opengl.dll` on PATH. Uses bundled `luajit.exe` against `wrappers/lua/` sources.

> **Note:** `sit.cmd_draw_text_ex()` and `sit.default_font()` require the embedded host draw shim (`sit_lua_draw_shim.c`). In dev mode, text overlay calls error unless you rebuild the embedded exe.

## Layout

```
wrappers/lua/
├── launcher/
│   ├── sit_lua_host.c       # Extract DLLs, init runtime, run embedded bytecode
│   ├── sit_lua_runtime.c/h  # Dynamic lua51.dll loading (LoadLibrary + fn pointers)
│   └── sit_lua_draw_shim.c/h # Safe DrawTextEx (SituationFont passed by pointer)
├── situation/
│   ├── init.lua          # package entry (hand)
│   ├── dll.lua           # ffi.load path discovery (hand)
│   ├── manual.lua        # variadic / callback / draw-shim wrappers (hand)
│   ├── ffi_cdef.lua      # generated ffi.cdef block
│   ├── foreign.lua       # generated export list + bind()
│   ├── types.lua         # generated struct helpers
│   ├── constants.lua     # generated SIT_* defines
│   ├── callbacks.lua     # generated ffi.typeof aliases
│   └── helpers.lua       # check(), init_info_window(), begin_frame()
├── examples/
│   └── hello_situation.lua
├── API_INDEX.md
└── MANUAL_BINDINGS.md
```

Build tooling (repo `tools/`):

| Tool | Output |
|------|--------|
| `gen_lua_embed.py` | `build/obj/lua/.../embed/sit_lua_embed.c` — embedded Lua bytecode |
| `gen_lua_dll_embed.py` | `build/obj/lua/.../dll_embed/sit_lua_dll_embed.c` — embedded DLL blobs |

## Usage

```lua
local sit = require("situation")
sit.load("opengl")  -- or os.getenv("SIT_LUA_BACKEND")

local info = sit.init_info_window(1280, 720, "My App")
sit.check(sit.SituationInit(0, nil, info))

while not sit.SituationWindowShouldClose() do
    sit.begin_frame()
    -- ...
end
sit.SituationShutdown()
```

Call Situation from the **main thread** only.

### Text overlay (DrawTextEx)

`SituationCmdDrawTextEx` passes a large `SituationFont` struct **by value**, which crashes LuaJIT FFI on Windows. Use the host shim instead:

```lua
local font = sit.default_font()
sit.check(sit.cmd_draw_text_ex(cmd, font, "Hello", x, y, 24.0, 1.0, color))
```

Do not call `sit.SituationCmdDrawTextEx` directly from Lua.

## Backends

| Backend | Support | Notes |
|---------|---------|-------|
| `opengl` | ✅ | Embeds `situation_opengl.dll` |
| `vulkan` | ✅ | Embeds `situation_vulkan.dll` |
| `static-opengl` | ❌ | Use embedded DLL mode (`opengl`) |
| `static-vulkan` | ❌ | Use embedded DLL mode (`vulkan`) |

## Troubleshooting

| Symptom | Fix |
|---------|-----|
| Window opens black and closes immediately | **Do not embed a broken `build\dll\situation_opengl.dll`.** Current MinGW builds may hit render-thread `GL execute failed: -600`. Lua embed auto-uses `build\dll\situation_opengl_lua_embed.dll` (known-good copy) when present. Override: `set SIT_LUA_EMBED_DLL=path\to\situation_opengl.dll` before `build_lua_example.bat`. |
| GLFW `Invalid window attribute 0x00021001` spam | Harmless on older embedded DLLs (10-bit probe bug). `helpers.lua` defaults to `SIT_OUTPUT_COLOR_8BIT` to silence it. |
| `sit_lua draw shim unavailable` | Rebuild with `build\build_lua_example.bat` (dev `luajit.exe` path lacks the shim). |
| `LuaJIT runtime DLL not found` | Run `_languages\lua\populate_toolchain.bat install` |
| `SituationLoadShaderFromMemory` fails after regen | Regenerate bindings — generator preserves `const char*` in FFI cdef. |

### Known-good DLL for Lua embed

Copy a working `situation_opengl.dll` to:

```
build/dll/situation_opengl_lua_embed.dll
```

The Lua build prefers this file over freshly built `build/dll/situation_opengl.dll`. A backup from 2026-06-25 is kept as `build/dll/situation_opengl_new.dll.bak`.