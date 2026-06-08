# Situation — Zig bindings

Auto-generated FFI for the Situation C library (`situation_opengl.dll` / `situation_vulkan.dll`).

## Generate

```bat
build_situation.bat opengl
python tools\generate_zig_bindings.py
```

Jam-tier only:

```bat
python tools\generate_zig_bindings.py --jam
```

Vulkan import lib:

```bat
python tools\generate_zig_bindings.py --lib situation_vulkan
```

## Output

Generated files live in `wrappers/zig/src/`:

| File | Role |
|------|------|
| `situation.zig` | Package root (re-exports; created once) |
| `situation_types.zig` | `extern struct`, enums, opaque handles |
| `situation_foreign.zig` | `pub extern fn` declarations (~531 procs) |
| `situation_foreign_jam.zig` | Jam-tier imports (`--jam`) |
| `situation_callbacks.zig` | `*const fn(...) callconv(.C)` aliases |
| `situation_constants.zig` | `SIT_KEY_*`, etc. |
| `situation_helpers.zig` | `situationBeginFrame()`, etc. |
| `API_INDEX.md` | Per-symbol index |
| `MANUAL_BINDINGS.md` | Variadic / hand-wrap symbols |

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

Link via `build.zig` (see `wrappers/zig/build.zig`). Copy `situation_opengl.dll` next to your executable.

## Maintenance

Do not hand-edit generated `.zig` files — changes will be lost. Edit `tools/generate_zig_bindings.py` or `tools/situation_api_parser.py`, then re-run the generator.

Patterns mirror the Odin generator (`tools/generate_odin_bindings.py`): curated ABI types, opaque stubs for internal structs, variadic symbols listed in `MANUAL_BINDINGS.md`.
