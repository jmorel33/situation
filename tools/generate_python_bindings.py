#!/usr/bin/env python3
"""Generate Python ctypes bindings from Situation public C headers.

Outputs (under wrappers/Python/situation/):
  types.py       — ctypes.Structure, IntEnum, opaque aliases
  callbacks.py   — CFUNCTYPE definitions
  foreign.py     — bind_all(dll) with argtypes / restype
  constants.py   — selected SIT_* #defines
  helpers.py     — frame helpers, init_info_window, check()
  API_INDEX.md   — categorized binding index
  MANUAL_BINDINGS.md

Usage:
  python tools/generate_python_bindings.py
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

OUT_DIR = ROOT / "wrappers" / "Python" / "situation"
WRAPPER_ROOT = ROOT / "wrappers" / "Python"
CALLBACKS_H = ROOT / "sit" / "situation_base_callbacks.h"

CTYPE_MAP: dict[str, str] = {
    "void": "None",
    "bool": "c_bool",
    "char": "c_char",
    "int": "c_int",
    "unsigned int": "c_uint",
    "unsigned char": "c_ubyte",
    "signed char": "c_byte",
    "short": "c_short",
    "unsigned short": "c_ushort",
    "int16_t": "c_int16",
    "uint16_t": "c_uint16",
    "long": "c_long",
    "unsigned long": "c_ulong",
    "long long": "c_longlong",
    "unsigned long long": "c_ulonglong",
    "float": "c_float",
    "double": "c_double",
    "size_t": "c_size_t",
    "uint8_t": "c_uint8",
    "uint16_t": "c_uint16",
    "uint32_t": "c_uint32",
    "uint64_t": "c_uint64",
    "int8_t": "c_int8",
    "int16_t": "c_int16",
    "int32_t": "c_int32",
    "int64_t": "c_int64",
    "SituationError": "c_int",
    "SituationCommandBuffer": "c_void_p",
    "SituationInitState": "c_int",
    "SituationLogLevel": "c_int",
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
    "SituationThreadPool": "c_void_p",
    "SituationAudioGraph": "c_void_p",
    "SituationNode": "c_void_p",
    "SituationVirtualDisplay": "c_void_p",
    "SituationRenderPassInfo": "c_void_p",
    "SituationClearValue": "SituationClearValue",
    "SituationTimerSystem": "c_void_p",
    "SituationJobId": "c_uint32",
    "SituationNodeHandle": "c_uint32",
    "ma_result": "c_int",
    "ma_uint64": "c_uint64",
    "ma_int64": "c_int64",
    "ma_seek_origin": "c_int",
    "mat4": "Mat4",
    "FILE": "c_void_p",
}

VOID_RETURN = "None"
MANUAL_SKIP = MANUAL_FUNCTIONS | {"SituationSetLogCallback"}

CTYPES_PRIMITIVES = {
    "None",
    "c_bool",
    "c_char",
    "c_char_p",
    "c_void_p",
    "c_int",
    "c_int16",
    "c_int32",
    "c_int64",
    "c_uint",
    "c_uint8",
    "c_uint16",
    "c_uint32",
    "c_uint64",
    "c_float",
    "c_double",
    "c_size_t",
    "c_long",
    "c_ulong",
    "c_byte",
    "c_ubyte",
    "c_short",
    "c_ushort",
    "c_longlong",
    "c_ulonglong",
}


def format_ctype_for_bind(pt: str) -> str:
    if pt.endswith("Callback"):
        return f"CB.{pt}"
    if pt in CTYPES_PRIMITIVES:
        return pt
    if pt.startswith("POINTER("):
        inner = pt[len("POINTER(") : -1]
        return f"POINTER({format_ctype_for_bind(inner)})"
    if pt == "Mat4":
        return "T.Mat4"
    if pt in _ENUM_TYPE_NAMES:
        return "c_int"
    return f"T.{pt}"


_ENUM_TYPE_NAMES: set[str] = set()


def c_type_to_ctypes(ctype: str) -> str:
    ctype = normalize_c_type(ctype)

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
        return "c_void_p"

    if base == "char":
        if ptr_depth == 1:
            return "c_char_p"
        if ptr_depth >= 2:
            return "POINTER(c_char_p)"
        return "c_char"

    if base in CTYPE_MAP:
        inner = CTYPE_MAP[base]
    elif base.startswith("Situation") or base in ("ColorRGBA", "Vector2", "Vector3", "Vector4", "SitRectangle", "Mat4"):
        inner = base
    else:
        inner = "c_void_p"

    if ptr_depth == 0:
        return inner
    if inner in ("None", "c_void_p", "c_char_p"):
        return "c_void_p" if ptr_depth == 1 else "POINTER(c_void_p)"
    return f"POINTER({inner})"


def render_manual_types() -> str:
    return '''"""ABI-critical structs — layout matches sit/situation_api.h (MSVC x64 / MinGW, v2.4.336+)."""

from __future__ import annotations

from ctypes import (
    Structure,
    c_bool,
    c_char,
    c_char_p,
    c_double,
    c_float,
    c_int,
    c_int32,
    c_int64,
    c_uint8,
    c_uint32,
    c_uint64,
    c_void_p,
)
from enum import IntEnum


class ColorRGBA(Structure):
    _fields_ = [
        ("r", c_uint8),
        ("g", c_uint8),
        ("b", c_uint8),
        ("a", c_uint8),
    ]


class SitRectangle(Structure):
    _fields_ = [
        ("x", c_float),
        ("y", c_float),
        ("width", c_float),
        ("height", c_float),
    ]


class Vector2(Structure):
    _fields_ = [("x", c_float), ("y", c_float)]


class Vector3(Structure):
    _fields_ = [("x", c_float), ("y", c_float), ("z", c_float)]


class Vector4(Structure):
    _fields_ = [("x", c_float), ("y", c_float), ("z", c_float), ("w", c_float)]


class SituationTexture(Structure):
    _fields_ = [
        ("slot_index", c_uint32),
        ("generation", c_uint32),
        ("width", c_int),
        ("height", c_int),
    ]


class SituationShader(Structure):
    _fields_ = [
        ("slot_index", c_uint32),
        ("generation", c_uint32),
    ]


class SituationMesh(Structure):
    _fields_ = [
        ("slot_index", c_uint32),
        ("generation", c_uint32),
        ("index_count", c_int),
        ("vertex_count", c_int),
        ("vertex_stride", c_uint64),
    ]


class SituationFont(Structure):
    _fields_ = [
        ("font_data", c_void_p),
        ("stb_font_info", c_void_p),
        ("atlas_texture", SituationTexture),
        ("glyph_info", c_void_p),
        ("atlas_width", c_int),
        ("atlas_height", c_int),
        ("font_height_pixels", c_float),
        ("is_bitmap", c_bool),
        ("bitmap_data", c_void_p),
        ("bitmap_width", c_int),
        ("bitmap_height", c_int),
        ("bitmap_count", c_int),
        ("first_char", c_int),
        ("chars_per_row", c_int),
        ("chars_per_col", c_int),
        ("display_cell_width", c_int),
        ("display_cell_height", c_int),
        ("char_spacing", c_float),
        ("line_spacing", c_float),
    ]


class SituationPackedFont(Structure):
    _fields_ = [
        ("char_width", c_int),
        ("char_height", c_int),
        ("display_height", c_int),
        ("char_count", c_int),
        ("first_char", c_int),
        ("chars_per_row", c_int),
        ("bits_per_row", c_int),
        ("data_bits", c_int),
        ("data_bit_offset", c_int),
        ("bit_order_msb_first", c_bool),
        ("top_padding", c_int),
        ("bottom_padding", c_int),
        ("left_padding", c_int),
        ("right_padding", c_int),
        ("atlas_chars_per_row", c_int),
        ("atlas_chars_per_col", c_int),
        ("enable_outline", c_bool),
        ("outline_thickness", c_int),
        ("outline_r", c_uint8),
        ("outline_g", c_uint8),
        ("outline_b", c_uint8),
        ("outline_a", c_uint8),
        ("font_r", c_uint8),
        ("font_g", c_uint8),
        ("font_b", c_uint8),
        ("font_a", c_uint8),
    ]


class SituationSound(Structure):
    _fields_ = [("slot_index", c_uint32), ("generation", c_uint32)]


class SituationBuffer(Structure):
    _fields_ = [("slot_index", c_uint32), ("generation", c_uint32)]


class SituationComputePipeline(Structure):
    _fields_ = [("slot_index", c_uint32), ("generation", c_uint32)]


class SituationTextureBlitRegion(Structure):
    _fields_ = [
        ("x", c_int),
        ("y", c_int),
        ("width", c_int),
        ("height", c_int),
    ]


class SituationClearValue(Structure):
    _fields_ = [
        ("color", ColorRGBA),
        ("depth", c_float),
        ("stencil", c_uint32),
    ]


class SituationImage(Structure):
    _fields_ = [
        ("data", c_void_p),
        ("width", c_int),
        ("height", c_int),
        ("channels", c_int),
        ("color_encoding", c_int),
    ]


Mat4 = c_float * 16


class SituationInitInfo(Structure):
    """Use helpers.init_info_window() — do not hand-fill field-by-field in demos."""

    _fields_ = [
        ("window_width", c_int32),
        ("window_height", c_int32),
        ("window_title", c_char_p),
        ("initial_active_window_flags", c_uint32),
        ("initial_inactive_window_flags", c_uint32),
        ("enable_vulkan_validation", c_uint8),
        ("force_single_queue", c_uint8),
        ("_pad_after_vulkan_bools", c_uint8 * 2),
        ("max_frames_in_flight", c_uint32),
        ("required_vulkan_extensions", c_void_p),
        ("required_vulkan_extension_count", c_uint32),
        ("flags", c_uint32),
        ("output_color_depth", c_int32),
        ("max_audio_voices", c_uint32),
        ("render_thread_count", c_int32),
        ("backpressure_policy", c_int32),
        ("io_queue_capacity", c_uint32),
        ("disable_io_thread", c_uint8),
        ("_pad_before_hot_reload", c_uint8 * 7),
        ("hot_reload_poll_rate", c_double),
        ("staging_buffer_size", c_uint64),
        ("main_thread_name", c_char_p),
        ("thread_affinity_main", c_uint64),
        ("thread_affinity_render", c_uint64),
        ("thread_affinity_audio", c_uint64),
        ("numa_prefer_local", c_uint8),
        ("worker_numa_spread", c_uint8),
        ("_pad_before_io_numa", c_uint8 * 2),
        ("io_thread_numa_node", c_int32),
        ("thread_pool_use_physical_cores", c_uint8),
        ("_pad_before_pool_reserved", c_uint8 * 3),
        ("thread_pool_reserved_threads", c_uint32),
    ]


SIT_OUTPUT_COLOR_AUTO = 0
'''


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


def render_enum_class(e_name: str, members: list[tuple[str, str | None]], define_map: dict[str, str]) -> str:
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

    lines = [f"class {e_name}(IntEnum):"]
    for name, _value in members:
        lines.append(f"    {name} = {member_values[name]}")
    if len(lines) == 1:
        lines.append("    pass")
    return "\n".join(lines)


def render_enums() -> str:
    define_map = build_full_define_map()
    parts: list[str] = ["from enum import IntEnum", ""]
    errno = parse_errno_enum()
    parts.append(render_enum_class(errno.name, errno.members, define_map))
    parts.append("")
    for e in parse_enums():
        if e.name == "SituationError":
            continue
        if len(e.members) < 2:
            continue
        parts.append(render_enum_class(e.name, e.members, define_map))
        parts.append("")
    return "\n".join(parts).rstrip() + "\n"


def render_callbacks() -> str:
    text = CALLBACKS_H.read_text(encoding="utf-8", errors="replace")
    cbs = parse_callbacks(text)

    lines = [
        '"""Callback CFUNCTYPE aliases from sit/situation_base_callbacks.h."""',
        "",
        "from ctypes import (",
        "    CFUNCTYPE,",
        "    POINTER,",
        "    c_bool,",
        "    c_char,",
        "    c_char_p,",
        "    c_double,",
        "    c_float,",
        "    c_int,",
        "    c_int64,",
        "    c_size_t,",
        "    c_uint32,",
        "    c_uint64,",
        "    c_void_p,",
        ")",
        "",
        "from . import types as T",
        "",
    ]

    for cb in cbs:
        cb_name = cb.name if cb.name.endswith("Callback") else cb.name + "Callback"
        m = re.match(r"(.+?)\s*\(\*\s*\w+\s*\)\s*\((.*)\)", cb.signature)
        params: list[str] = []
        ret = "None"
        if m:
            ret_str = m.group(1).strip()
            if ret_str != "void":
                ret = c_type_to_ctypes(ret_str)
                if ret not in ("c_int", "c_void_p", "c_bool", "c_float", "None"):
                    ret = "c_int"
            for p in split_params(m.group(2)):
                parsed = parse_param(p, c_type_to_ctypes)
                if parsed:
                    pt = parsed[1]
                    if pt.endswith("Callback"):
                        pt = pt
                    elif pt not in (
                        "c_int", "c_void_p", "c_char_p", "c_bool", "c_float", "c_size_t",
                        "c_uint32", "POINTER(c_char_p)",
                    ) and not pt.startswith("POINTER("):
                        pt = "c_void_p"
                    params.append(pt)
        param_str = ", ".join(params)
        ret_py = "None" if ret == "None" else ret
        lines.append(f"{cb_name} = CFUNCTYPE({ret_py}, {param_str})" if param_str else f"{cb_name} = CFUNCTYPE({ret_py})")
        if cb.comment:
            lines.append(f"# {cb.comment}")
        lines.append("")

    return "\n".join(lines).rstrip() + "\n"


def ctypes_imports_for_type(t: str) -> set[str]:
    needed = {"POINTER", "c_int", "c_void_p", "c_char_p", "c_bool", "c_float", "c_double", "c_uint32"}
    if t.startswith("POINTER("):
        needed.add("POINTER")
    for name in (
        "c_int32", "c_int64", "c_uint8", "c_uint16", "c_uint32", "c_uint64",
        "c_size_t", "c_long", "c_ulong", "c_byte", "c_ubyte", "c_short", "c_ushort",
    ):
        if name in t:
            needed.add(name)
    return needed


def render_foreign(entries: list[ApiEntry]) -> str:
    NULLABLE_OVERRIDES: dict[str, str] = {
        "SituationSetActiveGraph.0": "c_void_p",
    }

    bind_lines: list[str] = []

    seen_names: set[str] = set()
    for e in sorted(entries, key=lambda x: x.name):
        if e.manual_only or e.name in MANUAL_SKIP:
            continue
        if e.name in seen_names:
            continue
        seen_names.add(e.name)

        parsed = parse_function_signature(e.signature, c_type_to_ctypes)
        if not parsed:
            bind_lines.append(f"    # SKIP (unparsed): {e.name}")
            continue

        ret, name, params = parsed
        for idx, (pn, pt) in enumerate(params):
            override_key = f"{name}.{idx}"
            if override_key in NULLABLE_OVERRIDES:
                params[idx] = (pn, NULLABLE_OVERRIDES[override_key])

        argtypes: list[str] = []
        for _pn, pt in params:
            pt = format_ctype_for_bind(pt)
            argtypes.append(pt)

        restype = "None" if ret == VOID_RETURN else format_ctype_for_bind(ret)
        if restype == "T.SituationCommandBuffer":
            restype = "c_void_p"

        fn = f"dll.{name}"
        bind_lines.append(f"    if hasattr(dll, {name!r}):")
        if argtypes:
            bind_lines.append(f"        {fn}.argtypes = [{', '.join(argtypes)}]")
        else:
            bind_lines.append(f"        {fn}.argtypes = []")
        bind_lines.append(f"        {fn}.restype = {restype}")
        bind_lines.append("")

    lines = [
        '"""ctypes function bindings — call bind_all(dll) after loading the Situation DLL."""',
        "",
        "from ctypes import (",
        "    POINTER,",
        "    c_bool,",
        "    c_char,",
        "    c_char_p,",
        "    c_double,",
        "    c_float,",
        "    c_int,",
        "    c_int16,",
        "    c_int32,",
        "    c_int64,",
        "    c_long,",
        "    c_longlong,",
        "    c_short,",
        "    c_size_t,",
        "    c_uint,",
        "    c_uint8,",
        "    c_uint16,",
        "    c_uint32,",
        "    c_uint64,",
        "    c_ulong,",
        "    c_ulonglong,",
        "    c_ubyte,",
        "    c_void_p,",
        ")",
        "",
        "from . import callbacks as CB",
        "from . import types as T",
        "",
        "",
        "def bind_all(dll) -> None:",
        '    """Attach argtypes/restype to every exported Situation C function."""',
        "",
    ]
    lines.extend(bind_lines)
    return "\n".join(lines)


def render_constants() -> str:
    lines = ['"""Selected constants from sit/situation_base_etc.h."""', ""]
    for d in parse_defines():
        val = re.sub(r"\s*/\*.*?\*/\s*$", "", d.value).strip()
        val = re.sub(r"\s*//.*$", "", val).strip()
        if val.endswith("f"):
            val = val[:-1]
        lines.append(f"{d.name} = {val}")
    return "\n".join(lines) + "\n"


def render_helpers() -> str:
    return '''"""Helpers replacing C preprocessor macros and safe init patterns."""

from __future__ import annotations

from ctypes import byref, c_char_p, c_int, c_void_p

from . import types as T

SIT_OUTPUT_COLOR_AUTO = 0


class SituationError(Exception):
    """Raised when a Situation API call returns a non-success SituationError code."""

    def __init__(self, code: int):
        self.code = code
        super().__init__(f"SituationError({code})")


def check(err: int) -> None:
    if err != 0:
        raise SituationError(err)


def situation_success(err: int) -> bool:
    return err == 0


def situation_begin_frame(dll) -> None:
    dll.SituationPollInputEvents()
    dll.SituationUpdateTimers()


def init_info_zero(info: T.SituationInitInfo) -> None:
    """Zero-fill init info and set output_color_depth = AUTO."""
    from ctypes import addressof, memset, sizeof

    memset(addressof(info), 0, sizeof(info))
    info.output_color_depth = SIT_OUTPUT_COLOR_AUTO


def init_info_window(width: int, height: int, title: str | bytes) -> T.SituationInitInfo:
    """Populate SituationInitInfo the same way as SituationM2InitInfoWindow / Rust Default."""
    info = T.SituationInitInfo()
    init_info_zero(info)
    info.window_width = width
    info.window_height = height
    if isinstance(title, str):
        title = title.encode("utf-8")
    info.window_title = c_char_p(title)
    return info
'''


def render_manual_md(entries: list[ApiEntry]) -> str:
    manual = [e for e in entries if e.manual_only or e.name in MANUAL_FUNCTIONS or e.name == "SituationSetLogCallback"]
    lines = [
        "# Situation Python — Manual bindings",
        "",
        "These symbols are **not** bound in `foreign.py`. See `situation/manual.py`.",
        "",
        "| Function | Reason |",
        "|----------|--------|",
    ]
    for e in sorted(manual, key=lambda x: x.name):
        reason = e.manual_reason or "listed in MANUAL_FUNCTIONS"
        lines.append(f"| `{e.name}` | {reason} |")
    lines.append("")
    return "\n".join(lines)


def render_api_index(entries: list[ApiEntry], version: str) -> str:
    lines = [
        "# Situation Python bindings — API index",
        "",
        f"_Generated {datetime.now(timezone.utc).strftime('%Y-%m-%d %H:%M UTC')} from `sit/situation_api.h` — Situation **{version}**._",
        "",
        f"**Foreign bindings:** {len(foreign_entries(entries))}",
        "",
        "| Function | Section | Python | Notes |",
        "|----------|---------|--------|-------|",
    ]
    for e in sorted(entries, key=lambda x: x.name):
        if e.manual_only or e.name in MANUAL_SKIP:
            py = "manual"
            note = e.manual_reason or "manual wrapper"
        else:
            py = "auto"
            note = e.comment[:80] + ("..." if len(e.comment) > 80 else "")
        lines.append(f"| `{e.name}` | {e.section} | {py} | {note or '—'} |")
    lines.append("")
    return "\n".join(lines)


def render_opaque_stubs(entries: list[ApiEntry]) -> str:
    type_pattern = re.compile(r"\b([A-Z][A-Za-z0-9_]*)\b")
    referenced: set[str] = set()
    for e in foreign_entries(entries):
        parsed = parse_function_signature(e.signature, c_type_to_ctypes)
        if not parsed:
            continue
        ret, _name, params = parsed
        if ret != VOID_RETURN:
            for m in type_pattern.finditer(ret):
                referenced.add(m.group(1))
        for _pn, pt in params:
            for m in type_pattern.finditer(pt):
                referenced.add(m.group(1))

    known = set(CTYPE_MAP.keys()) | {
        "ColorRGBA", "Vector2", "Vector3", "Vector4", "SitRectangle", "Mat4",
        "SituationInitInfo", "SituationTexture", "SituationShader", "SituationFont",
        "SituationMesh", "SituationBuffer", "SituationSound", "SituationImage",
        "SituationClearValue", "SituationPackedFont", "SituationComputePipeline",
        "SituationTextureBlitRegion",
    }
    for e in parse_enums():
        known.add(e.name)
    known.add(parse_errno_enum().name)

    missing = sorted(referenced - known)
    if not missing:
        return ""

    lines = ["", "# Opaque type aliases (layout internal to Situation)", ""]
    for t in missing:
        if len(t) <= 2:
            continue
        lines.append(f"{t} = c_void_p")
    lines.append("")
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate Python ctypes bindings for Situation")
    parser.parse_args()

    global _ENUM_TYPE_NAMES
    _ENUM_TYPE_NAMES = {parse_errno_enum().name}
    for e in parse_enums():
        _ENUM_TYPE_NAMES.add(e.name)

    version = read_version()
    entries = parse_api_header()

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    WRAPPER_ROOT.mkdir(parents=True, exist_ok=True)

    header = gen_banner("generate_python_bindings.py", version, "#")

    types_body = render_manual_types() + "\n" + render_enums() + render_opaque_stubs(entries)
    (OUT_DIR / "types.py").write_text(header + types_body, encoding="utf-8")
    (OUT_DIR / "callbacks.py").write_text(header + render_callbacks(), encoding="utf-8")
    (OUT_DIR / "foreign.py").write_text(header + render_foreign(entries), encoding="utf-8")
    (OUT_DIR / "constants.py").write_text(header + render_constants(), encoding="utf-8")
    (OUT_DIR / "helpers.py").write_text(header + render_helpers(), encoding="utf-8")
    (WRAPPER_ROOT / "API_INDEX.md").write_text(render_api_index(entries, version), encoding="utf-8")
    (WRAPPER_ROOT / "MANUAL_BINDINGS.md").write_text(render_manual_md(entries), encoding="utf-8")

    print(f"Generated Python bindings in {OUT_DIR.relative_to(ROOT)}")
    print(f"  foreign: {len(foreign_entries(entries))} functions")


if __name__ == "__main__":
    main()
