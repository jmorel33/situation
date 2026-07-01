#!/usr/bin/env python3
"""Generate LuaJIT FFI bindings from Situation public C headers.

Outputs (under wrappers/lua/situation/):
  ffi_cdef.lua   — M.cdef string (structs, callback typedefs, extern decls)
  types.lua      — ffi.new helpers and type name table
  constants.lua  — SIT_* #defines and enum numeric values
  callbacks.lua  — ffi.typeof strings for callback typedefs
  foreign.lua    — M.bind(lib) metatable proxy + function name list
  helpers.lua    — check(), begin_frame(), init_info_window(), etc.
  API_INDEX.md   — categorized binding index (under wrappers/lua/)
  MANUAL_BINDINGS.md

Usage:
  python tools/generate_lua_bindings.py
"""

from __future__ import annotations

import argparse
import re
from datetime import datetime, timezone

from binding_common import (
    MANUAL_FUNCTIONS,
    build_define_map,
    foreign_entries,
    gen_banner,
    normalize_c_type,
    parse_function_signature,
    parse_param,
    resolve_enum_value,
    split_params,
)
from situation_api_parser import (
    ROOT,
    ApiEntry,
    parse_api_header,
    parse_callbacks,
    parse_defines,
    parse_enums,
    parse_errno_enum,
    read_version,
)

OUT_DIR = ROOT / "wrappers" / "lua" / "situation"
WRAPPER_ROOT = ROOT / "wrappers" / "lua"
LAUNCHER_DIR = WRAPPER_ROOT / "launcher"
CALLBACKS_H = ROOT / "sit" / "situation_base_callbacks.h"

CTYPE_MAP: dict[str, str] = {
    "void": "void",
    "bool": "bool",
    "char": "char",
    "int": "int",
    "unsigned int": "unsigned int",
    "unsigned char": "uint8_t",
    "signed char": "int8_t",
    "short": "int16_t",
    "unsigned short": "uint16_t",
    "int16_t": "int16_t",
    "uint16_t": "uint16_t",
    "long": "long",
    "unsigned long": "unsigned long",
    "long long": "int64_t",
    "unsigned long long": "uint64_t",
    "float": "float",
    "double": "double",
    "size_t": "size_t",
    "uint8_t": "uint8_t",
    "uint16_t": "uint16_t",
    "uint32_t": "uint32_t",
    "uint64_t": "uint64_t",
    "int8_t": "int8_t",
    "int32_t": "int32_t",
    "int64_t": "int64_t",
    "SituationError": "int",
    "SituationCommandBuffer": "void*",
    "SituationInitState": "int",
    "SituationLogLevel": "int",
    "ColorRGBA": "ColorRGBA",
    "Color": "ColorRGBA",
    "Vector2": "Vector2",
    "Vector3": "Vector3",
    "Vector4": "Vector4",
    "SitRectangle": "SitRectangle",
    "SituationInitInfo": "SituationInitInfo",
    "SituationImage": "SituationImage",
    "SituationFont": "SituationFont",
    "SituationPackedFont": "SituationPackedFont",
    "SituationTexture": "SituationTexture",
    "SituationShader": "SituationShader",
    "SituationMesh": "SituationMesh",
    "SituationBuffer": "SituationBuffer",
    "SituationSound": "SituationSound",
    "SituationThreadPool": "void*",
    "SituationAudioGraph": "void*",
    "SituationNode": "void*",
    "SituationVirtualDisplay": "void*",
    "SituationRenderPassInfo": "void*",
    "SituationClearValue": "SituationClearValue",
    "SituationTimerSystem": "void*",
    "SituationJobId": "uint32_t",
    "SituationNodeHandle": "uint32_t",
    "ma_result": "int",
    "ma_uint64": "uint64_t",
    "ma_int64": "int64_t",
    "ma_seek_origin": "int",
    "mat4": "mat4",
    "FILE": "void*",
}

VOID_RETURN = "void"
MANUAL_SKIP = MANUAL_FUNCTIONS | {"SituationSetLogCallback"}

STRUCT_TYPES: list[str] = [
    "ColorRGBA",
    "SitRectangle",
    "Vector2",
    "Vector3",
    "Vector4",
    "SituationTexture",
    "SituationShader",
    "SituationMesh",
    "SituationFont",
    "SituationPackedFont",
    "SituationSound",
    "SituationBuffer",
    "SituationComputePipeline",
    "SituationTextureBlitRegion",
    "SituationClearValue",
    "SituationImage",
    "SituationInitInfo",
]

_ENUM_TYPE_NAMES: set[str] = set()


def c_type_to_ffi(ctype: str) -> str:
    original = ctype.strip()
    is_const = bool(re.search(r"\bconst\b", original))
    ctype = normalize_c_type(original)

    if "(*" in ctype or ctype.endswith("Callback"):
        base = ctype.split("(")[0].strip().replace("*", "").strip()
        if not base.endswith("Callback"):
            base += "Callback"
        return base

    ptr_depth = ctype.count("*")
    base = ctype.replace("*", "").strip()

    if base == "void":
        if ptr_depth == 0:
            return VOID_RETURN
        return "void" + ("*" * ptr_depth)

    if base == "char":
        if ptr_depth == 1:
            return ("const char*" if is_const else "char*")
        if ptr_depth >= 2:
            inner = "const char*" if is_const else "char*"
            return inner + ("*" * (ptr_depth - 1))
        return "char"

    if base in CTYPE_MAP:
        inner = CTYPE_MAP[base]
    elif base.startswith("Situation") or base in (
        "ColorRGBA",
        "Vector2",
        "Vector3",
        "Vector4",
        "SitRectangle",
        "mat4",
    ):
        inner = base
    else:
        inner = "void*"

    if base in _ENUM_TYPE_NAMES:
        if ptr_depth == 0:
            return "int"
        return "int" + ("*" * ptr_depth)

    if ptr_depth == 0:
        return inner

    if inner == VOID_RETURN:
        return "void" + ("*" * ptr_depth)
    if inner == "void*":
        return "void" + ("*" * ptr_depth)
    return inner + ("*" * ptr_depth)


def render_stdint_typedefs() -> str:
    return """\
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef long long int64_t;
typedef unsigned long long uint64_t;
typedef unsigned long size_t;

/* Scalar aliases from sit/situation_base_types.h (referenced by callbacks) */
typedef int SituationModifiers;
typedef uint64_t SituationStreamSize;
typedef int SituationSeekOrigin;
"""


def render_manual_structs_cdef() -> str:
    return """\
typedef struct ColorRGBA {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} ColorRGBA;

typedef struct SitRectangle {
    float x;
    float y;
    float width;
    float height;
} SitRectangle;

typedef struct Vector2 {
    float x;
    float y;
} Vector2;

typedef struct Vector3 {
    float x;
    float y;
    float z;
} Vector3;

typedef struct Vector4 {
    float x;
    float y;
    float z;
    float w;
} Vector4;

typedef struct SituationTexture {
    uint32_t slot_index;
    uint32_t generation;
    int width;
    int height;
} SituationTexture;

typedef struct SituationShader {
    uint32_t slot_index;
    uint32_t generation;
} SituationShader;

typedef struct SituationMesh {
    uint32_t slot_index;
    uint32_t generation;
    int index_count;
    int vertex_count;
    uint64_t vertex_stride;
} SituationMesh;

typedef struct SituationFont {
    void* font_data;
    void* stb_font_info;
    SituationTexture atlas_texture;
    void* glyph_info;
    int atlas_width;
    int atlas_height;
    float font_height_pixels;
    bool is_bitmap;
    void* bitmap_data;
    int bitmap_width;
    int bitmap_height;
    int bitmap_count;
    int first_char;
    int chars_per_row;
    int chars_per_col;
    int display_cell_width;
    int display_cell_height;
    float char_spacing;
    float line_spacing;
} SituationFont;

typedef struct SituationPackedFont {
    int char_width;
    int char_height;
    int display_height;
    int char_count;
    int first_char;
    int chars_per_row;
    int bits_per_row;
    int data_bits;
    int data_bit_offset;
    bool bit_order_msb_first;
    int top_padding;
    int bottom_padding;
    int left_padding;
    int right_padding;
    int atlas_chars_per_row;
    int atlas_chars_per_col;
    bool enable_outline;
    int outline_thickness;
    uint8_t outline_r;
    uint8_t outline_g;
    uint8_t outline_b;
    uint8_t outline_a;
    uint8_t font_r;
    uint8_t font_g;
    uint8_t font_b;
    uint8_t font_a;
} SituationPackedFont;

typedef struct SituationSound {
    uint32_t slot_index;
    uint32_t generation;
} SituationSound;

typedef struct SituationBuffer {
    uint32_t slot_index;
    uint32_t generation;
} SituationBuffer;

typedef struct SituationComputePipeline {
    uint32_t slot_index;
    uint32_t generation;
} SituationComputePipeline;

typedef struct SituationTextureBlitRegion {
    int x;
    int y;
    int width;
    int height;
} SituationTextureBlitRegion;

typedef struct SituationClearValue {
    ColorRGBA color;
    float depth;
    uint32_t stencil;
} SituationClearValue;

typedef struct SituationImage {
    void* data;
    int width;
    int height;
    int channels;
    int color_encoding;
} SituationImage;

typedef float mat4[16];

typedef struct SituationInitInfo {
    int32_t window_width;
    int32_t window_height;
    const char* window_title;
    uint32_t initial_active_window_flags;
    uint32_t initial_inactive_window_flags;
    uint8_t enable_vulkan_validation;
    uint8_t force_single_queue;
    uint8_t _pad_after_vulkan_bools[2];
    uint32_t max_frames_in_flight;
    const char** required_vulkan_extensions;
    uint32_t required_vulkan_extension_count;
    uint32_t flags;
    int32_t output_color_depth;
    uint32_t max_audio_voices;
    int32_t render_thread_count;
    int32_t backpressure_policy;
    uint32_t io_queue_capacity;
    uint8_t disable_io_thread;
    uint8_t _pad_before_hot_reload[7];
    double hot_reload_poll_rate;
    uint64_t staging_buffer_size;
    const char* main_thread_name;
    uint64_t thread_affinity_main;
    uint64_t thread_affinity_render;
    uint64_t thread_affinity_audio;
    uint8_t numa_prefer_local;
    uint8_t worker_numa_spread;
    uint8_t _pad_before_io_numa[2];
    int32_t io_thread_numa_node;
    uint8_t thread_pool_use_physical_cores;
    uint8_t _pad_before_pool_reserved[3];
    uint32_t thread_pool_reserved_threads;
} SituationInitInfo;
"""


def build_full_define_map() -> dict[str, str]:
    dmap = build_define_map()
    for rel in (
        "sit/situation_base_etc.h",
        "sit/situation_api_types_system.h",
        "sit/situation_api_platform.h",
    ):
        text = (ROOT / rel).read_text(encoding="utf-8", errors="replace")
        for m in re.finditer(r"^#define\s+(\w+)\s+(0x[\da-fA-F]+|\d+)", text, re.MULTILINE):
            dmap[m.group(1)] = m.group(2)
    for d in parse_defines():
        val = re.sub(r"\s*//.*$", "", d.value).strip()
        if re.match(r"^(0x[\da-fA-F]+|\d+)", val):
            dmap.setdefault(d.name, val.split()[0])
    return dmap


def expand_enum_expr(expr: str, define_map: dict[str, str], member_values: dict[str, int]) -> str:
    expanded = resolve_enum_value(expr.strip(), define_map)
    for dname, dval in define_map.items():
        expanded = re.sub(r"\b" + re.escape(dname) + r"\b", dval, expanded)
    for mname, mval in member_values.items():
        expanded = re.sub(r"\b" + re.escape(mname) + r"\b", str(mval), expanded)
    return expanded


def resolve_enum_members(
    members: list[tuple[str, str | None]],
    define_map: dict[str, str],
) -> dict[str, int]:
    member_values: dict[str, int] = {}
    next_auto = 0
    for name, value in members:
        if value is not None:
            try:
                expr = expand_enum_expr(value, define_map, member_values)
                numeric = eval(expr)  # noqa: S307 — controlled input
                member_values[name] = numeric
                next_auto = numeric + 1
            except Exception:
                member_values[name] = next_auto
                next_auto += 1
        else:
            member_values[name] = next_auto
            next_auto += 1
    return member_values


def callback_param_c_type(param: str) -> str:
    """Extract the C type from a callback parameter declaration (keep const / char** intact)."""
    param = re.sub(r"\s+", " ", param.strip())
    m = re.match(r"^(.+?)\s+\w+$", param)
    if not m:
        return param
    return m.group(1).strip()


def callback_cdecl(cb_sig: str, cb_name: str) -> str:
    m = re.match(r"(.+?)\s*\(\*\s*\w+\s*\)\s*\((.*)\)", cb_sig)
    if not m:
        return f"typedef void (*{cb_name})(void);"
    ret_str = m.group(1).strip()
    ret = VOID_RETURN if ret_str == "void" else c_type_to_ffi(ret_str)
    if ret not in (VOID_RETURN, "int", "void*", "bool", "float", "double"):
        ret = "int"
    params = [callback_param_c_type(p) for p in split_params(m.group(2))]
    if params:
        return f"typedef {ret} (*{cb_name})({', '.join(params)});"
    return f"typedef {ret} (*{cb_name})(void);"


def render_cdef_callbacks() -> str:
    text = CALLBACKS_H.read_text(encoding="utf-8", errors="replace")
    cbs = parse_callbacks(text)
    lines: list[str] = []
    for cb in cbs:
        cb_name = cb.name if cb.name.endswith("Callback") else cb.name + "Callback"
        lines.append(callback_cdecl(cb.signature, cb_name))
    return "\n".join(lines) + ("\n" if lines else "")


def parse_param_lua_cdef(param: str) -> tuple[str, str] | None:
    """Parse a C param without stripping const (LuaJIT needs const char* for strings)."""
    param = param.strip()
    if not param or param == "..." or "(*" in param:
        return None
    m = re.match(r"^(.+?)\s+(\w+)$", param)
    if not m:
        return None
    return m.group(2).strip(), c_type_to_ffi(m.group(1).strip())


def parse_function_signature_lua_cdef(
    sig: str,
) -> tuple[str, str, list[tuple[str, str]]] | None:
    from binding_common import split_params, strip_c_api_attributes

    m = re.match(
        r"^(const\s+)?(.+?)\s+(\w+)\s*\((.*)\)\s*;?\s*$",
        strip_c_api_attributes(sig),
    )
    if not m:
        return None
    ret = c_type_to_ffi((m.group(1) or "") + m.group(2).strip())
    name = m.group(3)
    params: list[tuple[str, str]] = []
    for p in split_params(m.group(4)):
        parsed = parse_param_lua_cdef(p)
        if parsed:
            params.append(parsed)
    return ret, name, params


def render_cdef_functions(entries: list[ApiEntry]) -> str:
    lines: list[str] = []
    seen_names: set[str] = set()
    for e in sorted(entries, key=lambda x: x.name):
        if e.manual_only or e.name in MANUAL_SKIP:
            continue
        if e.name in seen_names:
            continue
        seen_names.add(e.name)

        parsed = parse_function_signature_lua_cdef(e.signature)
        if not parsed:
            lines.append(f"/* SKIP (unparsed): {e.name} */")
            continue

        ret, name, params = parsed
        if ret == "void*":
            ret = "void*"
        # Windows DLL bool returns are unreliable in LuaJIT FFI; use uint8_t.
        if ret == "bool":
            ret = "uint8_t"
        param_str = ", ".join(
            f"{'uint8_t' if pt == 'bool' else pt} {pn}" for pn, pt in params
        )
        if not param_str:
            lines.append(f"{ret} {name}(void);")
        else:
            lines.append(f"{ret} {name}({param_str});")
    return "\n".join(lines) + ("\n" if lines else "")


def collect_referenced_opaque_types(entries: list[ApiEntry]) -> list[str]:
    type_pattern = re.compile(r"\b([A-Z][A-Za-z0-9_]*)\b")
    referenced: set[str] = set()
    for e in foreign_entries(entries):
        parsed = parse_function_signature(e.signature, c_type_to_ffi)
        if not parsed:
            continue
        ret, _name, params = parsed
        if ret != VOID_RETURN:
            for m in type_pattern.finditer(ret):
                referenced.add(m.group(1))
        for _pn, pt in params:
            for m in type_pattern.finditer(pt):
                referenced.add(m.group(1))

    known = set(CTYPE_MAP.keys()) | set(STRUCT_TYPES) | {"mat4", "Mat4"}
    for e in parse_enums():
        known.add(e.name)
    known.add(parse_errno_enum().name)

    missing = sorted(referenced - known)
    return [t for t in missing if len(t) > 2 and not t.endswith("Callback")]


def render_opaque_forward_decls(entries: list[ApiEntry]) -> str:
    missing = collect_referenced_opaque_types(entries)
    if not missing:
        return ""
    lines = ["/* Opaque forward declarations */"]
    for t in missing:
        lines.append(f"typedef struct {t} {t};")
    return "\n".join(lines) + "\n"


def render_ffi_cdef(entries: list[ApiEntry]) -> str:
    parts = [
        render_stdint_typedefs(),
        render_manual_structs_cdef(),
        render_opaque_forward_decls(entries),
        render_cdef_callbacks(),
        render_cdef_functions(entries),
    ]
    cdef_body = "\n".join(p for p in parts if p).rstrip() + "\n"
    return f"""local M = {{}}

M.cdef = [[
{cdef_body}]]

return M
"""


def render_types_lua() -> str:
    lines = [
        "local ffi = require('ffi')",
        "",
        "local M = {}",
        "",
        "M.types = {",
    ]
    for name in STRUCT_TYPES:
        lines.append(f'    {name} = "{name}",')
    lines.append("}")
    lines.append("")

    for name in STRUCT_TYPES:
        lines.append(f'function M.new_{name}(...)')
        lines.append(f'    local obj = ffi.new("{name}")')
        lines.append("    if select('#', ...) > 0 then")
        lines.append("        local src = select(1, ...)")
        lines.append("        if type(src) == 'table' then")
        lines.append("            for k, v in pairs(src) do")
        lines.append("                obj[k] = v")
        lines.append("            end")
        lines.append("        end")
        lines.append("    end")
        lines.append("    return obj")
        lines.append("end")
        lines.append("")

    lines.append('function M.new_mat4()')
    lines.append('    return ffi.new("mat4")')
    lines.append("end")
    lines.append("")
    lines.append("return M")
    lines.append("")
    return "\n".join(lines)


def render_constants_lua() -> str:
    define_map = build_full_define_map()
    lines = [
        "local M = {}",
        "",
        "-- #defines from sit/situation_base_etc.h",
    ]
    for d in parse_defines():
        val = re.sub(r"\s*/\*.*?\*/\s*$", "", d.value).strip()
        val = re.sub(r"\s*//.*$", "", val).strip()
        if val.endswith("f"):
            val = val[:-1]
        lines.append(f"M.{d.name} = {val}")

    lines.append("")
    lines.append("-- Enum numeric values")
    errno = parse_errno_enum()
    for name, value in resolve_enum_members(errno.members, define_map).items():
        lines.append(f"M.{name} = {value}")

    for e in parse_enums():
        if e.name == "SituationError":
            continue
        if len(e.members) < 2:
            continue
        lines.append("")
        lines.append(f"-- {e.name}")
        for name, value in resolve_enum_members(e.members, define_map).items():
            lines.append(f"M.{name} = {value}")

    lines.append("")
    lines.append("M.SIT_OUTPUT_COLOR_AUTO = 0")
    lines.append("")
    lines.append("return M")
    lines.append("")
    return "\n".join(lines)


def render_callbacks_lua() -> str:
    text = CALLBACKS_H.read_text(encoding="utf-8", errors="replace")
    cbs = parse_callbacks(text)
    lines = [
        "local ffi = require('ffi')",
        "",
        "local M = {}",
        "",
    ]
    for cb in cbs:
        cb_name = cb.name if cb.name.endswith("Callback") else cb.name + "Callback"
        lines.append(f'M.{cb_name} = ffi.typeof("{cb_name}")')
        if cb.comment:
            lines.append(f"-- {cb.comment}")
        lines.append("")
    lines.append("return M")
    lines.append("")
    return "\n".join(lines)


def render_foreign_lua(entries: list[ApiEntry]) -> str:
    names: list[str] = []
    seen: set[str] = set()
    for e in sorted(entries, key=lambda x: x.name):
        if e.manual_only or e.name in MANUAL_SKIP:
            continue
        if e.name in seen:
            continue
        seen.add(e.name)
        names.append(e.name)

    fn_lines = ",\n".join(f'    "{n}"' for n in names)
    return f"""local M = {{}}

M.functions = {{
{fn_lines}
}}

function M.bind(lib)
    M._lib = lib
    setmetatable(M, {{ __index = lib }})
    return M
end

return M
"""


def render_helpers_lua() -> str:
    return """local ffi = require('ffi')

local M = {}

M.SIT_OUTPUT_COLOR_AUTO = 0
M.SIT_OUTPUT_COLOR_8BIT = 1


function M.check(err)
    if err ~= 0 then
        error("SituationError(" .. tostring(err) .. ")", 2)
    end
end


function M.situation_success(err)
    return err == 0
end


function M.begin_frame(lib)
    lib.SituationPollInputEvents()
    lib.SituationUpdateTimers()
end


function M.window_should_close(lib)
    return lib.SituationWindowShouldClose() ~= 0
end


function M.key_pressed(lib, key)
    return lib.SituationIsKeyPressed(key) ~= 0
end


function M.init_info_window(width, height, title)
    local info = ffi.new("SituationInitInfo")
    ffi.fill(info, 0)
    -- 8-bit avoids stale-DLL glfwGetWindowAttrib(GLFW_*_BITS) spam during 10-bit probe.
    info.output_color_depth = M.SIT_OUTPUT_COLOR_8BIT
    info.window_width = width
    info.window_height = height
    info.window_title = title
    return info
end


return M
"""


def render_manual_md(entries: list[ApiEntry]) -> str:
    manual = [
        e
        for e in entries
        if e.manual_only or e.name in MANUAL_FUNCTIONS or e.name == "SituationSetLogCallback"
    ]
    lines = [
        "# Situation Lua — Manual bindings",
        "",
        "These symbols are **not** in `ffi_cdef.lua` / `foreign.lua`. See `situation/manual.lua`.",
        "",
        "| Function | Reason |",
        "|----------|--------|",
    ]
    for e in sorted(manual, key=lambda x: x.name):
        reason = e.manual_reason or "listed in MANUAL_FUNCTIONS"
        lines.append(f"| `{e.name}` | {reason} |")
    lines.append("")
    return "\n".join(lines)


def _foreign_names(entries: list[ApiEntry]) -> list[str]:
    names: list[str] = []
    seen: set[str] = set()
    for e in sorted(entries, key=lambda x: x.name):
        if e.manual_only or e.name in MANUAL_SKIP:
            continue
        if e.name in seen:
            continue
        seen.add(e.name)
        names.append(e.name)
    return names


def render_force_link_c(entries: list[ApiEntry], version: str) -> str:
    names = _foreign_names(entries)
    lines = [
        gen_banner("generate_lua_bindings.py", version, "/*"),
        "/* Force-link Situation API symbols for static Lua host builds. */",
        "",
        "#include <stdint.h>",
        "",
    ]
    for name in names:
        lines.append(f"extern void {name}(void);")
    lines.append("")
    lines.append("void sit_lua_force_link_anchor(void)")
    lines.append("{")
    for name in names:
        lines.append(f"    (void)(uintptr_t)&{name};")
    lines.append("}")
    lines.append("")
    return "\n".join(lines)


def render_exports_def(entries: list[ApiEntry]) -> str:
    names = _foreign_names(entries)
    lines = ["EXPORTS"]
    lines.extend(names)
    lines.append("")
    return "\n".join(lines)


def render_api_index(entries: list[ApiEntry], version: str) -> str:
    lines = [
        "# Situation Lua bindings — API index",
        "",
        f"_Generated {datetime.now(timezone.utc).strftime('%Y-%m-%d %H:%M UTC')} from `sit/situation_api.h` — Situation **{version}**._",
        "",
        f"**Foreign bindings:** {len(foreign_entries(entries))}",
        "",
        "| Function | Section | Lua | Notes |",
        "|----------|---------|-----|-------|",
    ]
    for e in sorted(entries, key=lambda x: x.name):
        if e.manual_only or e.name in MANUAL_SKIP:
            lua = "manual"
            note = e.manual_reason or "manual wrapper"
        else:
            lua = "auto"
            note = e.comment[:80] + ("..." if len(e.comment) > 80 else "")
        lines.append(f"| `{e.name}` | {e.section} | {lua} | {note or '—'} |")
    lines.append("")
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate LuaJIT FFI bindings for Situation")
    parser.parse_args()

    global _ENUM_TYPE_NAMES
    _ENUM_TYPE_NAMES = {parse_errno_enum().name}
    for e in parse_enums():
        _ENUM_TYPE_NAMES.add(e.name)

    version = read_version()
    entries = parse_api_header()

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    WRAPPER_ROOT.mkdir(parents=True, exist_ok=True)

    header = gen_banner("generate_lua_bindings.py", version, "--")

    (OUT_DIR / "ffi_cdef.lua").write_text(header + render_ffi_cdef(entries), encoding="utf-8")
    (OUT_DIR / "types.lua").write_text(header + render_types_lua(), encoding="utf-8")
    (OUT_DIR / "constants.lua").write_text(header + render_constants_lua(), encoding="utf-8")
    (OUT_DIR / "callbacks.lua").write_text(header + render_callbacks_lua(), encoding="utf-8")
    (OUT_DIR / "foreign.lua").write_text(header + render_foreign_lua(entries), encoding="utf-8")
    (OUT_DIR / "helpers.lua").write_text(header + render_helpers_lua(), encoding="utf-8")
    (WRAPPER_ROOT / "API_INDEX.md").write_text(render_api_index(entries, version), encoding="utf-8")
    (WRAPPER_ROOT / "MANUAL_BINDINGS.md").write_text(render_manual_md(entries), encoding="utf-8")

    LAUNCHER_DIR.mkdir(parents=True, exist_ok=True)
    (LAUNCHER_DIR / "sit_lua_force_link.c").write_text(
        render_force_link_c(entries, version), encoding="utf-8"
    )
    (LAUNCHER_DIR / "sit_lua_exports.def").write_text(
        render_exports_def(entries), encoding="utf-8"
    )

    foreign_count = len(foreign_entries(entries))
    print(f"Generated Lua bindings in {OUT_DIR.relative_to(ROOT)}")
    print(f"  foreign: {foreign_count} functions")
    print(f"  launcher: {LAUNCHER_DIR.relative_to(ROOT)}/sit_lua_force_link.c")
    print(f"  launcher: {LAUNCHER_DIR.relative_to(ROOT)}/sit_lua_exports.def")


if __name__ == "__main__":
    main()