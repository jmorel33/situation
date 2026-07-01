# Situation — Fortran bindings

Auto-generated `ISO_C_BINDING` FFI for the Situation C library (`situation_opengl.dll` / `situation_vulkan.dll`, or static `.a` archives).

## Generate

```bat
build\build_situation.bat opengl
python tools\generate_fortran_bindings.py
```

## Structure

Generated sources live in `wrappers/Fortran/src/`:

| File | Role |
|------|------|
| `situation_types.f90` | `bind(C)` structs and enum constants |
| `situation_foreign.f90` | `bind(C, name=...)` interface block |
| `situation_callbacks.f90` | `abstract interface` for C callbacks |
| `situation_constants.f90` | `SIT_KEY_*`, etc. |
| `situation_helpers.f90` | `situation_begin_frame()`, `sit_ok()` |
| `situation.f90` | Umbrella module re-exporting the above |
| `API_INDEX.md` | Per-symbol index |
| `MANUAL_BINDINGS.md` | Variadic / hand-wrap symbols |

Examples: `wrappers/Fortran/examples/<name>/main.f90` (+ optional `demo_helpers.f90` for example-specific modules)

Build uses shared `scripts/wrapper_*.bat` helpers (see `doc/COMPILATION_GUIDE.md`). Intermediate artifacts live under `build/`, not `wrappers/`:

| Path | Contents |
|------|----------|
| `build/obj/fortran/mod/` | Fortran `.mod` module files |
| `build/obj/fortran/bindings/` | Shared binding `.o` files |
| `build/obj/fortran/<example>_<backend>/` | Per-example `main.o` |

## Build example

From the repo root:

```bat
build\build_fortran_example.bat opengl
build\build_fortran_example.bat static-opengl hello_situation
```

**Compiler:** MSYS2 `gfortran` (`pacman -S mingw-w64-x86_64-gcc-fortran`)

**Backends:** `opengl`, `vulkan`, `static-opengl`, `static-vulkan` — same as other wrapper builders.

**Output:** `build/examples/fortran/<name>.exe`

DLL modes copy `situation_*.dll` next to the exe. Static modes link via `scripts/wrapper_gcc_link_static.bat` (self-contained, no DLL).

## Use from Fortran

```fortran
program demo
  use, intrinsic :: iso_c_binding
  use situation
  implicit none
  type(SituationInitInfo), target :: config
  character(kind=c_char,len=32), target :: title
  integer(c_int) :: err

  call ascii_to_c('Hello from Fortran', title)
  config%window_width = 1280
  config%window_height = 720
  config%window_title = c_loc(title(1))

  err = SituationInit(0, c_null_ptr, c_loc(config))
  if (.not. sit_ok(err)) stop

  do while (.not. SituationWindowShouldClose())
    call situation_begin_frame()
  end do

  call SituationShutdown()
contains
  subroutine ascii_to_c(ascii, cstr)
    character(len=*), intent(in) :: ascii
    character(kind=c_char, len=*), intent(out) :: cstr
    integer :: i, n
    n = min(len_trim(ascii), len(cstr) - 1)
    do i = 1, n
      cstr(i:i) = ascii(i:i)
    end do
    cstr(n + 1:n + 1) = c_null_char
  end subroutine ascii_to_c
end program demo
```

## Maintenance

Do not hand-edit generated `.f90` files under `src/`. Edit `tools/generate_fortran_bindings.py` or `tools/situation_api_parser.py`, then re-run the generator.

The raw `situation_foreign.f90` layer is low-level by design. Add optional safe wrappers in a separate hand-written module if needed.