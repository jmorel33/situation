# Situation — Modula-2 bindings

Auto-generated FFI for the Situation C library (`situation_opengl.dll` / `situation_vulkan.dll`, or static `.a` archives). Targets **GNU Modula-2** (`gm2`).

## Toolchain

`gm2` is **not** an MSYS2 binary package. Install options:

| Option | Path / notes |
|--------|----------------|
| **A. Bundled** (recommended) | `_languages/gm2/bin/gm2.exe` |
| **B. System build** | Build GCC with `--enable-languages=m2` — see [GNU_M2.md](https://github.com/michelou/m2-examples/blob/main/GNU_M2.md) |
| **C. MSYS2 custom** | `C:\msys64\mingw64\bin\gm2.exe` if you built gm2 into MinGW |

Full plan: `doc/plan/FORTRAN_MODULA2_BINDINGS_PLAN.md` (toolchain section).

Bindings can be regenerated without gm2. Building/running examples **requires** gm2 plus MinGW `gcc`/`g++`.

## Generate

```bat
build\build_situation.bat opengl
python tools\generate_modula2_bindings.py
```

Vulkan import lib (optional):

```bat
python tools\generate_modula2_bindings.py --lib situation_vulkan
```

## Output

Generated files live in `wrappers/Modula2/src/`:

| File | Role |
|------|------|
| `SituationTypes.def` | `RECORD` types, opaque `ADDRESS` handles, enum constants |
| `SituationForeign.def` | `EXTERN` procedure declarations |
| `SituationCallbacks.def` | `TYPE … = PROCEDURE(…)` callback aliases |
| `SituationConstants.def` | `SIT_KEY_*` and related constants |
| `SituationHelpers.def` / `SituationHelpers.mod` | Frame macro replacements |
| `API_INDEX.md` | Per-symbol index |
| `MANUAL_BINDINGS.md` | Variadic / hand-wrap symbols |

Definition modules (`.def`) are parsed by `gm2` via `-Iwrappers/Modula2/src`; only `SituationHelpers.mod` has a separate implementation compile step.

## Build example

From the repo root:

```bat
build\build_modula2_example.bat opengl
build\build_modula2_example.bat static-opengl hello_situation
```

Backends: `opengl`, `vulkan`, `static-opengl`, `static-vulkan` — same as `build_examples.bat` and other wrapper builders.

**Output:** `build/examples/modula2/<name>.exe` — uses shared `scripts/wrapper_*.bat` helpers; intermediate objects under `build/obj/modula2/<name>_<backend>/`.

DLL modes copy `situation_*.dll` next to the exe. Static modes link `build/dll/situation_*.a` (no DLL at runtime). See `doc/COMPILATION_GUIDE.md`.

## Example: `hello_situation`

`wrappers/Modula2/examples/hello_situation/Main.mod` — full port of the Rust/Odin/Zig demo:

- Raymarched torus + raster-bar backdrop (Vulkan push constants / OpenGL uniforms)
- Audio graph: `ToneSynth → Echo → Reverb`
- Virtual MIDI loopback, pentatonic auto-notes, interactive FX keys
- HUD via `SituationCmdDrawTextEx` (title: **Situation+Modula2**)

## Use from Modula-2

```modula2
MODULE myapp;

FROM SYSTEM IMPORT ADR, CAST;
FROM SituationForeign IMPORT SituationInit, SituationShutdown, SituationWindowShouldClose;
FROM SituationTypes IMPORT SituationInitInfo, SITUATION_SUCCESS;

VAR config: SituationInitInfo;

BEGIN
  config.window_width := 1280;
  config.window_height := 720;
  config.window_title := ADR("Hello from Modula-2");
  IF SituationInit(0, CAST(ADDRESS, 0), ADR(config)) = SITUATION_SUCCESS THEN
    WHILE NOT SituationWindowShouldClose() DO END;
    SituationShutdown()
  END
END myapp.
```

## Maintenance

Do not hand-edit generated `.def` files. Edit `tools/generate_modula2_bindings.py` or `tools/situation_api_parser.py`, then re-run the generator.

The raw `SituationForeign.def` layer maps directly to C — no automatic error-to-exception wrapping. Check `SituationError` return codes explicitly (see `SituationHelpers.situationSuccess`).