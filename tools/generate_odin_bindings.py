#!/usr/bin/env python3
"""Generate Odin FFI bindings from Situation public C headers.

Outputs (under wrappers/Odin/):
  situation_types.odin      — enums, records, opaque handles
  situation_callbacks.odin  — proc \"c\" callback aliases
  situation_foreign.odin    — foreign situation {{ ... }} imports
  situation_constants.odin  — selected SIT_* #defines
  situation_helpers.odin    — frame macro helpers
  API_INDEX.md              — categorized binding index
  MANUAL_BINDINGS.md        — symbols requiring hand-written Odin wrappers

Usage:
  python tools/generate_odin_bindings.py
  python tools/generate_odin_bindings.py --jam
  python tools/generate_odin_bindings.py --dll build/dll/situation_opengl.dll

Odin compiler: _languages/odin/dist/odin.exe
Build example: build_odin_example.bat hello_situation
"""

from __future__ import annotations

import argparse
import re
from datetime import datetime, timezone
from pathlib import Path

from situation_api_parser import (
    ROOT,
    ApiEntry,
    parse_api_header,
    parse_callbacks,
    parse_defines,
    parse_enums,
    parse_errno_enum,
    parse_structs,
    load_jam_slice,
    read_version,
)

OUT_DIR = ROOT / "wrappers" / "odin"
JAM_SLICE_FILE = ROOT / "tools" / "jam_api_slice.txt"
PACKAGE_ROOT = ROOT / "wrappers" / "odin" / "situation.odin"

MANUAL_FUNCTIONS = {
    "SituationLog",
    "SituationLogWarning",
    "SituationImageDrawTextFormatted",
}

# C types that appear in SITAPI signatures → Odin types
CTYPE_MAP: dict[str, str] = {
    "void": "---",
    "bool": "b8",
    "char": "c.char",
    "int": "c.int",
    "unsigned int": "c.uint",
    "unsigned char": "u8",
    "signed char": "i8",
    "short": "i16",
    "unsigned short": "u16",
    "long": "c.long",
    "unsigned long": "c.ulong",
    "long long": "i64",
    "unsigned long long": "u64",
    "float": "f32",
    "double": "f64",
    "size_t": "uint",
    "uint8_t": "u8",
    "uint16_t": "u16",
    "uint32_t": "u32",
    "uint64_t": "u64",
    "int8_t": "i8",
    "int16_t": "i16",
    "int32_t": "c.int",
    "int64_t": "i64",
    "SituationError": "Situation_Error",
    "SituationCommandBuffer": "Situation_Command_Buffer",
    "SituationInitState": "Situation_Init_State",
    "SituationLogLevel": "Situation_Log_Level",
    "ColorRGBA": "Color_RGBA",
    "Color": "Color_RGBA",
    "Vector2": "Vector2",
    "Vector3": "Vector3",
    "Vector4": "Vector4",
    "SitRectangle": "Sit_Rectangle",
    "SituationInitInfo": "Situation_Init_Info",
    "SituationImage": "Situation_Image",
    "SituationFont": "Situation_Font",
    "SituationTexture": "Situation_Texture",
    "SituationShader": "Situation_Shader",
    "SituationMesh": "Situation_Mesh",
    "SituationBuffer": "Situation_Buffer",
    "SituationSound": "Situation_Sound",
    "SituationThreadPool": "Situation_Thread_Pool",
    "SituationAudioGraph": "Situation_Audio_Graph",
    "SituationNode": "Situation_Node",
    "SituationVirtualDisplay": "Situation_Virtual_Display",
    "SituationRenderPassInfo": "Situation_Render_Pass_Info",
    "SituationClearValue": "Situation_Clear_Value",
    "SituationTimerSystem": "Situation_Timer_System",
    "ma_result": "c.int",
    "ma_uint64": "u64",
}


def snake_type(name: str) -> str:
    s = re.sub(r"(.)([A-Z][a-z]+)", r"\1_\2", name)
    s = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", s)
    return s


def c_type_to_odin(ctype: str) -> str:
    ctype = ctype.strip()
    # Preserve const info for char* handling before stripping
    had_const = "const" in ctype
    ctype = re.sub(r"\bconst\s+", "", ctype)
    ctype = re.sub(r"\bvolatile\s+", "", ctype)
    # Strip GCC/MSVC attributes
    ctype = re.sub(r"__attribute__\s*\(\([^)]*\)\)", "", ctype)
    ctype = re.sub(r"__declspec\s*\([^)]*\)", "", ctype)
    ctype = re.sub(r"\s+", " ", ctype).strip()

    if "(*" in ctype or ctype.endswith("Callback"):
        base = ctype.split("(")[0].strip().replace("*", "").strip()
        odin_name = snake_type(base)
        # Don't double-suffix if the name already ends with _Callback
        if not odin_name.endswith("_Callback"):
            odin_name += "_Callback"
        return odin_name

    ptr_depth = ctype.count("*")
    base = ctype.replace("*", "").strip()

    # void* is rawptr in Odin, void** is ^rawptr, void (no ptr) is --- (no return)
    if base == "void":
        if ptr_depth == 0:
            return "---"
        if ptr_depth == 1:
            return "rawptr"
        return "^" * (ptr_depth - 1) + "rawptr"

    # char* is cstring, char** is [^]cstring
    if base == "char":
        if ptr_depth == 0:
            return "c.char"
        if ptr_depth == 1:
            return "cstring"
        if ptr_depth == 2:
            return "[^]cstring"
        return "^" * (ptr_depth - 1) + "cstring"

    if base in CTYPE_MAP:
        odin = CTYPE_MAP[base]
    elif base.startswith("Situation") or base in ("ColorRGBA", "Vector2", "Vector3", "Vector4", "SitRectangle"):
        odin = snake_type(base)
    else:
        odin = snake_type(base) if base else "rawptr"

    if ptr_depth == 0:
        return odin
    if ptr_depth == 1:
        return f"^{odin}"
    return "^" * ptr_depth + odin


def split_params(param_str: str) -> list[str]:
    params: list[str] = []
    depth = 0
    current: list[str] = []
    for ch in param_str:
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        if ch == "," and depth == 0:
            p = "".join(current).strip()
            if p and p != "void":
                params.append(p)
            current = []
        else:
            current.append(ch)
    tail = "".join(current).strip()
    if tail and tail != "void":
        params.append(tail)
    return params


def parse_param(param: str) -> tuple[str, str] | None:
    param = param.strip()
    if not param or param == "...":
        return None
    if "(*" in param:
        return None
    m = re.match(r"^(?:const\s+)?(.+?)\s+(\w+)$", param)
    if not m:
        m = re.match(r"^(.+?)\s+(\w+)$", param)
    if not m:
        return None
    ctype, pname = m.group(1).strip(), m.group(2).strip()
    return pname, c_type_to_odin(ctype)


def parse_function_signature(sig: str) -> tuple[str, str, list[tuple[str, str]]] | None:
    m = re.match(r"^(const\s+)?(.+?)\s+(\w+)\s*\((.*)\)\s*;?\s*$", sig.strip())
    if not m:
        return None
    ret = c_type_to_odin(m.group(2).strip())
    name = m.group(3)
    params: list[tuple[str, str]] = []
    for p in split_params(m.group(4)):
        parsed = parse_param(p)
        if parsed:
            params.append(parsed)
    return ret, name, params


def render_manual_types() -> str:
    return '''// Core math & color (ABI must match sit/situation_api.h)
Color_RGBA :: struct #packed {
    r, g, b, a: u8,
}

Sit_Rectangle :: struct {
    x, y, width, height: f32,
}

Vector2 :: struct {
    x, y: f32,
}

Vector3 :: struct {
    x, y, z: f32,
}

Vector4 :: struct {
    x, y, z, w: f32,
}

// Generational GPU handles
Situation_Texture :: struct {
    slot_index: u32,
    generation: u32,
    width: c.int,
    height: c.int,
}

Situation_Shader :: struct {
    slot_index: u32,
    generation: u32,
}

Situation_Font :: struct {
    texture: Situation_Texture,
    font_size: f32,
    ascent: f32,
    descent: f32,
    line_gap: f32,
    glyph_count: c.int,
    is_bitmap: b32,
}

// Opaque handles (layout unknown — pass as pointer)
Situation_Command_Buffer :: distinct rawptr
Situation_Thread_Pool :: struct {}
Situation_Audio_Graph :: struct {}
Situation_Node :: struct {}

// Scalar type aliases for C/miniaudio interop
Situation_Job_Id :: distinct u32
Situation_Node_Handle :: distinct u32
ma_int64 :: i64
ma_seek_origin :: c.int

// Additional opaque/resource handles
Situation_Sound :: struct { slot_index: u32, generation: u32 }
Situation_Buffer :: struct { slot_index: u32, generation: u32 }
Situation_Compute_Pipeline :: struct { slot_index: u32, generation: u32 }

// Structs referenced by the API but defined internally
Situation_Camera_Desc :: struct {}        // Opaque camera description (pass as pointer)
Situation_Render_Pass_Info :: struct {}   // Render pass configuration (pass as pointer)
Situation_Texture_Blit_Region :: struct {
    x, y, width, height: c.int,
}

// cglm mat4 equivalent
mat4 :: matrix[4, 4]f32

Situation_Init_Info :: struct {
    window_width: c.int,
    window_height: c.int,
    window_title: cstring,
    initial_active_window_flags: u32,
    initial_inactive_window_flags: u32,
    enable_vulkan_validation: b32,
    force_single_queue: b32,
    max_frames_in_flight: u32,
    required_vulkan_extensions: [^]cstring,
    required_vulkan_extension_count: u32,
    flags: u32,
    max_audio_voices: u32,
    io_queue_capacity: u32,
    disable_io_thread: b32,
    hot_reload_poll_rate: f64,
    staging_buffer_size: u64,
    thread_affinity_main: u64,
    thread_affinity_render: u64,
    thread_affinity_audio: u64,
    numa_prefer_local: b32,
    worker_numa_spread: b32,
    io_thread_numa_node: i32,
    thread_pool_use_physical_cores: b32,
    thread_pool_reserved_threads: u32,
}
'''


def render_enum(e_name: str, members: list[tuple[str, str | None]], define_map: dict[str, str] = None) -> str:
    lines = [f"{snake_type(e_name)} :: enum c.int {{"]
    for name, value in members:
        if value is None:
            lines.append(f"    {name},")
        else:
            # Resolve macro references to literal values
            resolved = value.strip()
            if define_map and resolved in define_map:
                resolved = define_map[resolved]
            # Handle bitwise OR expressions like (A | B)
            if define_map and "|" in resolved:
                parts = [p.strip() for p in resolved.split("|")]
                parts = [define_map.get(p, p) for p in parts]
                resolved = " | ".join(parts)
            lines.append(f"    {name} = {resolved},")
    lines.append("}")
    return "\n".join(lines)


def _build_define_map() -> dict[str, str]:
    """Parse all #define constants from situation_api.h to resolve enum values."""
    text = (ROOT / "sit" / "situation_api.h").read_text(encoding="utf-8", errors="replace")
    dmap: dict[str, str] = {}
    for m in re.finditer(r"^#define\s+(\w+)\s+(0x[\da-fA-F]+|\d+)", text, re.MULTILINE):
        dmap[m.group(1)] = m.group(2)
    return dmap


def render_enums() -> str:
    define_map = _build_define_map()
    parts: list[str] = []
    errno = parse_errno_enum()
    parts.append(render_enum(errno.name, errno.members))
    parts.append("")
    for e in parse_enums():
        if e.name == "SituationError":
            continue
        if len(e.members) < 2:
            continue
        parts.append(render_enum(e.name, e.members, define_map))
        parts.append("")
    return "\n".join(parts).rstrip() + "\n"


def render_callbacks() -> str:
    text = (ROOT / "sit" / "situation_api.h").read_text(encoding="utf-8", errors="replace")
    lines = [
        "// Callback typedefs from sit/situation_api.h — register with SituationSet* APIs.",
        "",
    ]
    for cb in parse_callbacks(text):
        odin_name = snake_type(cb.name)
        sig = cb.signature
        # Signature format: "void (*Name)(int count, const char** paths, void* user_data)"
        m = re.match(r"(.+?)\s*\(\*\s*\w+\s*\)\s*\((.*)\)", sig)
        params: list[str] = []
        ret = "---"
        if m:
            ret_str = m.group(1).strip()
            if ret_str != "void":
                ret = c_type_to_odin(ret_str)
            for p in split_params(m.group(2)):
                parsed = parse_param(p)
                if parsed:
                    params.append(f"{parsed[0]}: {parsed[1]}")
        param_str = ", ".join(params)
        if ret == "---":
            lines.append(f"{odin_name} :: proc \"c\" ({param_str})")
        else:
            lines.append(f"{odin_name} :: proc \"c\" ({param_str}) -> {ret}")
        if cb.comment:
            lines.append(f"// {cb.comment}")
        lines.append("")
    return "\n".join(lines).rstrip() + "\n"


def render_foreign(entries: list[ApiEntry], dll_name: str) -> str:
    lines = [
        f'foreign import situation "{dll_name}"',
        "",
        "foreign situation {",
    ]
    seen_names: set[str] = set()
    for e in sorted(entries, key=lambda x: x.name):
        if e.manual_only or e.name in MANUAL_FUNCTIONS:
            continue
        if e.name in seen_names:
            continue
        seen_names.add(e.name)
        parsed = parse_function_signature(e.signature)
        if not parsed:
            lines.append(f"    // SKIP (unparsed): {e.name}")
            continue
        ret, name, params = parsed
        param_str = ", ".join(f"{n}: {t}" for n, t in params)
        if e.comment:
            lines.append(f"    // {e.comment}")
        if ret == "---":
            lines.append(f"    {name} :: proc({param_str}) ---")
        else:
            lines.append(f"    {name} :: proc({param_str}) -> {ret} ---")
        if e.comment:
            lines.append("")
    lines.append("}")
    return "\n".join(lines) + "\n"


def render_constants() -> str:
    lines = ["// Selected constants from sit/situation_base_etc.h"]
    for d in parse_defines():
        val = d.value
        if val.endswith("f"):
            val = val[:-1]
        lines.append(f"{d.name} :: {val}")
    return "\n".join(lines) + "\n"


def render_helpers() -> str:
    return '''// Helpers replacing C preprocessor macros from situation_api.h

situation_begin_frame :: proc() {
    SituationPollInputEvents()
    SituationUpdateTimers()
}

situation_success :: proc(err: Situation_Error) -> bool {
    return err == .SITUATION_SUCCESS
}
'''


def render_manual_md(entries: list[ApiEntry]) -> str:
    manual = [e for e in entries if e.manual_only or e.name in MANUAL_FUNCTIONS]
    lines = [
        "# Situation Odin — Manual bindings",
        "",
        "These symbols are **not** emitted in `situation_foreign.odin`. Add hand-written Odin wrappers as needed.",
        "",
        "| Function | Reason |",
        "|----------|--------|",
    ]
    for e in sorted(manual, key=lambda x: x.name):
        reason = e.manual_reason or "listed in MANUAL_FUNCTIONS"
        lines.append(f"| `{e.name}` | {reason} |")
    lines.append("")
    return "\n".join(lines)


def render_api_index(entries: list[ApiEntry], version: str, jam: bool) -> str:
    mode = "jam slice" if jam else "full"
    lines = [
        "# Situation Odin bindings — API index",
        "",
        f"_Generated {datetime.now(timezone.utc).strftime('%Y-%m-%d %H:%M UTC')} from `sit/situation_api.h` — Situation **{version}** ({mode})._",
        "",
        f"**Foreign imports:** {sum(1 for e in entries if not e.manual_only and e.name not in MANUAL_FUNCTIONS)}",
        "",
        "| Function | Section | Odin | Notes |",
        "|----------|---------|------|-------|",
    ]
    for e in sorted(entries, key=lambda x: x.name):
        if e.manual_only or e.name in MANUAL_FUNCTIONS:
            odin = "manual"
            note = e.manual_reason or "manual wrapper"
        else:
            odin = "auto"
            note = e.comment[:80] + ("..." if len(e.comment) > 80 else "")
        lines.append(f"| `{e.name}` | {e.section} | {odin} | {note or '—'} |")
    lines.append("")
    return "\n".join(lines)


def render_package_root(version: str) -> str:
    return f'''package situation

// Odin FFI for the Situation C library (auto-generated bindings).
// Situation {version}
//
// Re-generate:
//   python tools/generate_odin_bindings.py
//
// Requires a pre-built DLL, e.g.:
//   build_situation.bat opengl  →  build/dll/situation_opengl.dll

import "core:c"

// Generated modules (same package)
// situation_types.odin, situation_foreign.odin, situation_callbacks.odin,
// situation_constants.odin, situation_helpers.odin live in generated/
'''


def filter_jam(entries: list[ApiEntry], jam_names: set[str]) -> list[ApiEntry]:
    if not jam_names:
        return entries
    return [e for e in entries if e.name in jam_names]


def collect_referenced_types(entries: list[ApiEntry]) -> set[str]:
    """Collect all Odin type names referenced in foreign proc signatures."""
    referenced: set[str] = set()
    # Pattern matches PascalCase/Snake_Case identifiers used as types
    type_pattern = re.compile(r"\b([A-Z][A-Za-z0-9_]*)\b")
    for e in entries:
        if e.manual_only or e.name in MANUAL_FUNCTIONS:
            continue
        parsed = parse_function_signature(e.signature)
        if not parsed:
            continue
        ret, name, params = parsed
        # Scan return type and param types for type references
        if ret != "---":
            for m in type_pattern.finditer(ret):
                referenced.add(m.group(1))
        for pname, ptype in params:
            for m in type_pattern.finditer(ptype):
                referenced.add(m.group(1))
    return referenced


def render_opaque_stubs(entries: list[ApiEntry]) -> str:
    """Generate opaque struct stubs for types referenced but not manually defined."""
    referenced = collect_referenced_types(entries)
    # Types we define in render_manual_types or as built-in Odin types
    known_types = {
        # Built-in / import "core:c"
        "c", "b8", "b32", "u8", "u16", "u32", "u64", "i8", "i16", "i32", "i64",
        "f32", "f64", "uint", "int", "rawptr", "cstring", "bool",
        # Manual types we define
        "Color_RGBA", "Sit_Rectangle", "Vector2", "Vector3", "Vector4",
        "Situation_Texture", "Situation_Shader", "Situation_Font",
        "Situation_Command_Buffer", "Situation_Thread_Pool",
        "Situation_Audio_Graph", "Situation_Node",
        "Situation_Job_Id", "Situation_Node_Handle",
        "Situation_Sound", "Situation_Buffer", "Situation_Compute_Pipeline",
        "Situation_Camera_Desc", "Situation_Render_Pass_Info",
        "Situation_Texture_Blit_Region",
        "Situation_Init_Info", "Situation_Error",
        "ma_int64", "ma_seek_origin", "mat4",
        # Odin keywords/builtins that look like types
        "Situation_Log_Level", "Situation_Init_State",
    }
    # Also add all enum names we generate
    for e in parse_enums():
        known_types.add(snake_type(e.name))
    errno = parse_errno_enum()
    known_types.add(snake_type(errno.name))

    # Also add callback names
    text = (ROOT / "sit" / "situation_api.h").read_text(encoding="utf-8", errors="replace")
    for cb in parse_callbacks(text):
        odin_name = snake_type(cb.name)
        if not odin_name.endswith("_Callback"):
            odin_name += "_Callback"
        known_types.add(odin_name)

    missing = sorted(referenced - known_types)
    if not missing:
        return ""

    lines = [
        "",
        "// --- Auto-generated opaque type stubs ---",
        "// These types are referenced by the API but their layout is internal.",
        "// Pass them by pointer. If you need field access, define them manually.",
        "",
    ]
    for t in missing:
        # Skip things that are obviously not types (single chars, Odin keywords)
        if len(t) <= 2 or t in ("Select", "First", "String"):
            continue
        lines.append(f"{t} :: struct {{}}")
    lines.append("")
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate Odin FFI bindings for Situation")
    parser.add_argument("--jam", action="store_true", help="Emit jam API slice only")
    parser.add_argument(
        "--dll",
        default="../../build/dll/situation_opengl.lib",
        help="Import lib path for foreign import (relative to wrappers/odin/ or absolute)",
    )
    args = parser.parse_args()

    version = read_version()
    all_entries = parse_api_header()
    jam_names = load_jam_slice(JAM_SLICE_FILE)
    entries = filter_jam(all_entries, jam_names) if args.jam else all_entries

    OUT_DIR.mkdir(parents=True, exist_ok=True)

    gen_header = (
        f"// Auto-generated by tools/generate_odin_bindings.py — Situation {version}\n"
        f"// Do not edit by hand.\n"
        f"package situation\n\n"
        f'import "core:c"\n\n'
    )

    (OUT_DIR / "situation_types.odin").write_text(
        gen_header + render_manual_types() + "\n" + render_enums() + render_opaque_stubs(entries),
        encoding="utf-8",
    )
    (OUT_DIR / "situation_callbacks.odin").write_text(
        gen_header + render_callbacks(), encoding="utf-8"
    )
    foreign_name = "situation_foreign_jam.odin" if args.jam else "situation_foreign.odin"
    (OUT_DIR / foreign_name).write_text(
        gen_header + render_foreign(entries, args.dll.replace("\\", "/")),
        encoding="utf-8",
    )
    (OUT_DIR / "situation_constants.odin").write_text(
        gen_header + render_constants(), encoding="utf-8"
    )
    (OUT_DIR / "situation_helpers.odin").write_text(
        gen_header + render_helpers(), encoding="utf-8"
    )
    index_name = "API_INDEX_JAM.md" if args.jam else "API_INDEX.md"
    (OUT_DIR / index_name).write_text(
        render_api_index(entries, version, args.jam), encoding="utf-8"
    )
    (OUT_DIR / "MANUAL_BINDINGS.md").write_text(
        render_manual_md(all_entries), encoding="utf-8"
    )

    # situation.odin is the hand-editable package entry (not overwritten if present)
    if not PACKAGE_ROOT.exists():
        PACKAGE_ROOT.write_text(render_package_root(version), encoding="utf-8")

    auto = sum(1 for e in entries if not e.manual_only and e.name not in MANUAL_FUNCTIONS)
    print(f"Situation {version}")
    print(f"Odin bindings: {auto} foreign procs ({'jam' if args.jam else 'full'}), {len(all_entries)} total SITAPI")
    print(f"Wrote {OUT_DIR.relative_to(ROOT)}/{foreign_name}")
    print(f"Wrote {OUT_DIR.relative_to(ROOT)}/{index_name}")
    print(f"Wrote {PACKAGE_ROOT.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
