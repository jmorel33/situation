# Situation Python bindings

Python **ctypes** FFI for the Situation C library (stdlib only for bindings — **PyInstaller** only for building `.exe`).

## Regenerate bindings

```powershell
python tools\generate_python_bindings.py
```

Or run all generators:

```powershell
tools\run_all.bat
```

## Build hello_situation.exe

The Python wrapper build matches the other languages: **compile** (stage + bytecode check) → **link** (PyInstaller onefile) → optional **run**.

| Step | Script | What it does |
|------|--------|----------------|
| Generate bindings | `python tools\generate_python_bindings.py` | Regenerate `wrappers/Python/situation/` from `situation_api.h` |
| Build library | `build\build_situation.bat opengl` | Produce `build\dll\situation_opengl.dll` |
| **Compile** | `scripts\wrapper_compile_python.bat` | Stage to `build\obj\python\.../stage/`; `compileall` |
| **Link** | `scripts\wrapper_link_python.bat` | PyInstaller → `build\examples\python\hello_situation.exe` + DLL |
| Run | `build\build_python_example.bat opengl` | Full pipeline; runs the `.exe` |

**Prerequisites:** Python 3.10+, PyInstaller (`pip install pyinstaller`, or MSYS2: `pacman -S mingw-w64-x86_64-pyinstaller`)

```powershell
python -m pip install pyinstaller
& ".\build\build_situation.bat" opengl
python tools\generate_python_bindings.py
& ".\build\build_python_example.bat" opengl hello_situation --no-run   # build .exe only
& ".\build\build_python_example.bat" opengl hello_situation            # build + run
```

Output (same convention as Rust/Fortran/Zig):

```
build/examples/python/
├── hello_situation.exe
└── situation_opengl.dll
```

Run directly:

```powershell
Set-Location build\examples\python
.\hello_situation.exe
```

Vulkan:

```powershell
& ".\build\build_situation.bat" vulkan
& ".\build\build_python_example.bat" vulkan hello_situation
```

## Layout

```
wrappers/Python/
├── situation/           # import package
│   ├── _dll.py          # CDLL loader (hand)
│   ├── manual.py        # variadic / callback wrappers (hand)
│   ├── types.py         # generated ctypes.Structure + IntEnum
│   ├── foreign.py       # generated bind_all(dll)
│   ├── callbacks.py     # generated CFUNCTYPE
│   ├── constants.py     # generated SIT_* defines
│   └── helpers.py       # init_info_window, check(), begin_frame
├── examples/
│   └── hello_situation.py
├── API_INDEX.md
└── MANUAL_BINDINGS.md
```

## Usage

```python
from ctypes import byref
import situation
from situation import helpers as H

dll = situation.load_dll("opengl")
config = H.init_info_window(800, 600, "My App")
H.check(dll.SituationInit(0, None, byref(config)))
# ... main loop on the main thread ...
dll.SituationShutdown()
```

## Notes

- Call Situation from the **main thread** only.
- The frozen `.exe` loads `situation_opengl.dll` / `situation_vulkan.dll` from the same directory as the executable.
- Use `helpers.init_info_window()` for `SituationInitInfo` — do not hand-fill struct fields.
- Requires Python **3.10+** for development; the shipped `.exe` is standalone (no Python install required at runtime).
