# Situation — Zig bindings

Auto-generated FFI for the Situation C library (`situation_opengl.dll` / `situation_vulkan.dll`, or static `.a` archives).

## Generate

```bat
build_situation.bat opengl
python tools\generate_zig_bindings.py
```

Vulkan import lib:

```bat
python tools\generate_zig_bindings.py --lib situation_vulkan
```

## Output

Generated files live in `wrappers/Zig/src/`:

| File | Role |
|------|------|
| `situation.zig` | Package root (re-exports; created once) |
| `situation_types.zig` | `extern struct`, enums, opaque handles |
| `situation_foreign.zig` | `pub extern fn` declarations |
| `situation_callbacks.zig` | `*const fn(...) callconv(.C)` aliases |
| `situation_constants.zig` | `SIT_KEY_*`, etc. |
| `situation_helpers.zig` | `situationBeginFrame()`, etc. |
| `build.zig` | Link logic (`-Dlink=`, `-Dexample=`) |
| `API_INDEX.md` | Per-symbol index |
| `MANUAL_BINDINGS.md` | Variadic / hand-wrap symbols |

## Build example

From the repo root (recommended):

```bat
build_zig_example.bat opengl
build_zig_example.bat vulkan hello_situation
build_zig_example.bat static-opengl
```

Backends: `opengl`, `vulkan`, `static-opengl`, `static-vulkan` — same as `build_examples.bat`. Output: `build/examples/zig/`.

DLL modes copy `situation_*.dll` next to the exe. Static modes link `build/dll/situation_*.a` via `build.zig` (GNU target). See `doc/COMPILATION_GUIDE.md`.

Manual Zig build (advanced):

```bat
zig build --build-file wrappers\Zig\build.zig -Dlink=opengl -Dexample=hello_situation -p build\examples\zig
```

## Use from Zig

```zig
const situation = @import("situation");

pub fn main() !void {
    var config = situation.SituationInitInfo{
        .window_width = 1280,
        .window_height = 720,
        .window_title = "Hello from Zig",
    };
    _ = situation.SituationInit(0, null, &config);
    defer situation.SituationShutdown();

    while (!situation.SituationWindowShouldClose()) {
        situation.situationBeginFrame();
        // ...
        situation.SituationEndFrame();
    }
}
```

## Maintenance

Do not hand-edit generated `.zig` files — changes will be lost. Edit `tools/generate_zig_bindings.py` or `tools/situation_api_parser.py`, then re-run the generator.

Patterns mirror the Odin generator (`tools/generate_odin_bindings.py`): curated ABI types, opaque stubs for internal structs, variadic symbols listed in `MANUAL_BINDINGS.md`.
