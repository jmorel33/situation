# Build Scripts

All scripts are run from the **project root**, not from inside `build/`. They change directory internally.

For the full reference including compiler flags, include paths, and define tables, see [doc/COMPILATION_GUIDE.md](../doc/COMPILATION_GUIDE.md).

---

## Prerequisites

- **Compiler:** GCC via MSYS2/MinGW-w64 (`C:\msys64\mingw64\bin` or set `MINGW_PATH`)
- **GLFW:** must be built first — `ext\glfw\build\src\libglfw3.a`
- **Vulkan SDK:** auto-detected from `C:\VulkanSDK\*`, or set `VULKAN_SDK`
- **shaderc:** required for Vulkan DLL — run `build\build_shaderc.bat` once (see [COMPILATION_GUIDE.md](../doc/COMPILATION_GUIDE.md))

---

## Step 1 — Build the Library

Always do this before building tests or examples.

```bat
build\build_situation.bat static-opengl    → build/dll/situation_opengl.a   (recommended)
build\build_situation.bat static-vulkan    → build/dll/situation_vulkan.a
build\build_situation.bat opengl           → build/dll/situation_opengl.dll
build\build_situation.bat vulkan           → build/dll/situation_vulkan.dll
build\build_situation.bat all              → both DLLs
build\build_situation.bat clean            → remove .o / .dll artifacts
```

Static `.a` builds are self-contained — exes link everything in and need no DLL at runtime. DLL builds are faster to produce and useful for iteration.

Optional env overrides:
- `MINGW_PATH` — path to MinGW `bin/`
- `VULKAN_SDK` — Vulkan SDK root
- `SIT_OPTIMIZE_CFLAGS` — defaults to `-O2 -mfma -ffp-contract=fast`

---

## Step 2 — Build the Test Harness

```bat
build\build_tests.bat static-opengl    → build/tests/sit_test_opengl.exe
build\build_tests.bat static-vulkan    → build/tests/sit_test_vulkan.exe
build\build_tests.bat opengl           → build/tests/sit_test_opengl.exe  (DLL-linked)
build\build_tests.bat vulkan           → build/tests/sit_test_vulkan.exe  (DLL-linked)
```

Static modes produce a single portable exe. DLL modes build faster but require `build\dll\` on PATH or use `run_tests.bat`.

---

## Step 3 — Run Tests

```bat
build\run_tests.bat opengl
build\run_tests.bat vulkan
build\run_tests.bat vulkan --module graphics --filter spirv
build\run_tests.bat vulkan --module audio --verbose
```

Results are printed to the console and saved to a timestamped file in `build\tests\results\`.

Static exes can also be run directly without the launcher:

```bat
build\tests\sit_test_opengl.exe --module threading
build\tests\sit_test_vulkan.exe --filter buffer --verbose
build\tests\sit_test_vulkan.exe --list
build\tests\sit_test_vulkan.exe --stop-on-fail
```

---

## Building Examples

```bat
build\build_examples.bat static-opengl hello_situation
build\build_examples.bat opengl        quad_storm
build\build_examples.bat vulkan        demon_hunt        (Vulkan-only)
build\build_examples.bat static-vulkan demon_hunt
```

Output goes to `build\examples\`. DLL builds copy the matching `.dll` next to the exe automatically.

---

## Language Wrapper Examples

```bat
build\build_odin_example.bat    [backend] [example_name]
build\build_zig_example.bat     [backend] [example_name]
build\build_rust_example.bat    [backend] [example_name]
build\build_fortran_example.bat [backend] [example_name]
build\build_modula2_example.bat [backend] [example_name]
build\build_python_example.bat  [backend] [example_name]
build\build_lua_example.bat     [backend] [example_name]
```

`example_name` defaults to `hello_situation`. Same backend tokens as above.

**Lua** produces a single self-contained `.exe` (embedded Situation + lua51 DLLs). Only `opengl` / `vulkan` — see `wrappers/lua/README.md`. Dev iteration: `build\run_lua_dev.bat`.

---

## Output Directories

| Path | Contents |
| :--- | :--- |
| `build/dll/` | Library artifacts (`.dll`, `.a`, `.lib`, `.def`) |
| `build/tests/` | Test harness executables and `results/` |
| `build/examples/` | C example executables (from `examples/`) |
| `build/examples/odin/` | Odin wrapper executables |
| `build/examples/zig/` | Zig wrapper executables |
| `build/examples/rust/` | Rust wrapper executables |
| `build/examples/fortran/` | Fortran wrapper executables |
| `build/examples/modula2/` | Modula-2 wrapper executables |
| `build/examples/python/` | Python wrapper executables (PyInstaller) |
| `build/examples/lua/` | Lua wrapper executables (embedded — one `.exe` per demo) |
| `build/obj/fortran/` | Fortran intermediate `.o` / `.mod` (not shipped) |
| `build/obj/modula2/` | Modula-2 intermediate `.o` (not shipped) |
| `build/obj/python/` | Python PyInstaller stage + work dirs (not shipped) |
| `build/obj/lua/` | Lua embed blobs + host `.o` (not shipped) |
