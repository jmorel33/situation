#!/usr/bin/env python3
"""Generate Fortran ISO_C_BINDING FFI bindings from Situation public C headers.

Outputs (under wrappers/Fortran/src/):
  situation_types.f90      — bind(C) structs, enum integer constants, opaque stubs
  situation_callbacks.f90  — abstract interface blocks (situation_base_callbacks.h)
  situation_foreign.f90    — bind(C, name=...) interface for foreign_entries
  situation_constants.f90  — integer/real parameters from parse_defines
  situation_helpers.f90    — situation_begin_frame, sit_ok
  situation.f90            — umbrella module re-exporting sub-modules
  API_INDEX.md             — categorized binding index
  MANUAL_BINDINGS.md       — symbols requiring hand-written Fortran wrappers

Usage:
  python tools/generate_fortran_bindings.py
  python tools/generate_fortran_bindings.py --lib situation_opengl
"""

from __future__ import annotations

import argparse
import re
from datetime import datetime, timezone
from pathlib import Path

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

OUT_DIR = ROOT / "wrappers" / "Fortran" / "src"
WRAPPER_ROOT = ROOT / "wrappers" / "Fortran"
CALLBACKS_H = ROOT / "sit" / "situation_base_callbacks.h"

CTYPE_MAP: dict[str, str] = {
    "void": "void",
    "bool": "logical(c_bool)",
    "char": "character(kind=c_char)",
    "int": "integer(c_int)",
    "unsigned int": "integer(c_int)",
    "unsigned char": "integer(c_int8_t)",
    "signed char": "integer(c_int8_t)",
    "short": "integer(c_short)",
    "unsigned short": "integer(c_short)",
    "long": "integer(c_long)",
    "unsigned long": "integer(c_long)",
    "long long": "integer(c_int64_t)",
    "unsigned long long": "integer(c_int64_t)",
    "float": "real(c_float)",
    "double": "real(c_double)",
    "size_t": "integer(c_size_t)",
    "uint8_t": "integer(c_int8_t)",
    "uint16_t": "integer(c_int16_t)",
    "uint32_t": "integer(c_int32_t)",
    "uint64_t": "integer(c_int64_t)",
    "int8_t": "integer(c_int8_t)",
    "int16_t": "integer(c_int16_t)",
    "int32_t": "integer(c_int32_t)",
    "int64_t": "integer(c_int64_t)",
    "SituationError": "integer(c_int)",
    "SituationCommandBuffer": "type(c_ptr)",
    "SituationInitState": "integer(c_int)",
    "SituationLogLevel": "integer(c_int)",
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
    "SituationSoundHandle": "SituationSound",
    "SituationThreadPool": "type(c_ptr)",
    "SituationAudioGraph": "type(c_ptr)",
    "SituationNode": "type(c_ptr)",
    "SituationVirtualDisplay": "type(c_ptr)",
    "SituationRenderPassInfo": "type(c_ptr)",
    "SituationClearValue": "SituationClearValue",
    "SituationTimerSystem": "type(c_ptr)",
    "SituationJobId": "integer(c_int32_t)",
    "SituationNodeHandle": "integer(c_int32_t)",
    "SituationToneHandle": "integer(c_int32_t)",
    "SituationRenderList": "type(c_ptr)",
    "VkRenderPass": "type(c_ptr)",
    "VkDevice": "type(c_ptr)",
    "VkInstance": "type(c_ptr)",
    "VkPhysicalDevice": "type(c_ptr)",
    "SituationModifiers": "integer(c_int)",
    "SituationStreamSize": "integer(c_int64_t)",
    "SituationStreamResult": "integer(c_int)",
    "SituationSeekOrigin": "integer(c_int)",
    "ma_result": "integer(c_int)",
    "ma_uint64": "integer(c_int64_t)",
    "ma_int64": "integer(c_int64_t)",
    "ma_seek_origin": "integer(c_int)",
    "mat4": "Mat4",
    "Mat4": "Mat4",
    "FILE": "type(c_ptr)",
}

VOID_RETURN = "__VOID__"
STRUCT_TYPES = {
    "ColorRGBA",
    "ColorHSV",
    "ColorYPQA",
    "ColorYPQf",
    "ColorRGBA10",
    "SitRectangle",
    "Vector2",
    "Vector3",
    "Vector4",
    "SituationTexture",
    "SituationShader",
    "SituationFont",
    "SituationPackedFont",
    "SituationSound",
    "SituationBuffer",
    "SituationMesh",
    "SituationImage",
    "SituationModel",
    "SituationComputePipeline",
    "SituationTextureBlitRegion",
    "SituationInitInfo",
    "SituationOSInfo",
    "SituationDeviceInfo",
    "Mat4",
}

# Returned structs / parameters that are not Fortran BIND(C)-safe (contain const char*, etc.)
FORTRAN_MANUAL_ONLY_FUNCTIONS = {
    "SituationGetThreadingStatus",
}

ISO_TYPES = {
    "c_int",
    "c_int8_t",
    "c_int16_t",
    "c_int32_t",
    "c_int64_t",
    "c_short",
    "c_long",
    "c_size_t",
    "c_float",
    "c_double",
    "c_bool",
    "c_char",
    "c_funptr",
    "c_ptr",
}


def enum_type_names() -> set[str]:
    names = {parse_errno_enum().name}
    for e in parse_enums():
        names.add(e.name)
    return names


def c_type_to_fortran(ctype: str) -> str:
    ctype = normalize_c_type(ctype)

    if "(*" in ctype or ctype.endswith("Callback") or ctype == "GLFWerrorfun":
        return "type(c_funptr)"

    ptr_depth = ctype.count("*")
    base = ctype.replace("*", "").strip()

    if base == "void":
        if ptr_depth == 0:
            return VOID_RETURN
        return "type(c_ptr)"

    if base == "char":
        if ptr_depth == 0:
            return "character(kind=c_char)"
        return "type(c_ptr)"

    if base in enum_type_names():
        return "integer(c_int)"

    if base in CTYPE_MAP:
        ftype = CTYPE_MAP[base]
    elif base.startswith("Situation") or base in ("ColorRGBA", "Vector2", "Vector3", "Vector4", "SitRectangle"):
        ftype = base
    else:
        ftype = base if base else "type(c_ptr)"

    if ptr_depth == 0:
        return ftype
    return "type(c_ptr)"


def is_fortran_intrinsic_type(ftype: str) -> bool:
    return (
        ftype.startswith("integer(")
        or ftype.startswith("real(")
        or ftype.startswith("logical(")
        or ftype.startswith("character(")
        or ftype in ("type(c_ptr)", "type(c_funptr)")
    )


def fortran_param_decl(name: str, ftype: str) -> str:
    if ftype == VOID_RETURN:
        return ""
    if is_fortran_intrinsic_type(ftype):
        return f"      {ftype}, value :: {name}"
    return f"      type({ftype}), value :: {name}"


def fortran_result_decl(ftype: str) -> str:
    if ftype == VOID_RETURN:
        return ""
    if is_fortran_intrinsic_type(ftype):
        return f"      {ftype} :: res"
    return f"      type({ftype}) :: res"


def collect_imports(types: list[str]) -> list[str]:
    imports: set[str] = set()
    for ftype in types:
        if ftype == VOID_RETURN:
            continue
        if not is_fortran_intrinsic_type(ftype):
            imports.add(ftype)
            continue
        if ftype == "type(c_ptr)":
            imports.add("c_ptr")
            continue
        if ftype == "type(c_funptr)":
            imports.add("c_funptr")
            continue
        inner_m = re.match(r"(?:integer|real|logical)\((.+)\)", ftype)
        if inner_m:
            imports.add(inner_m.group(1))
            continue
        if ftype.startswith("character("):
            imports.add("c_char")
    return sorted(imports)


def render_import_line(imports: list[str]) -> str:
    if not imports:
        return ""
    return "      import :: " + ", ".join(imports) + "\n"


def render_manual_types() -> str:
    return """  ! Core math & color (ABI must match sit/situation_api.h)
  type, bind(C) :: ColorRGBA
    integer(c_int8_t) :: r, g, b, a
  end type ColorRGBA

  type, bind(C) :: SitRectangle
    real(c_float) :: x, y, width, height
  end type SitRectangle

  type, bind(C) :: Vector2
    real(c_float) :: x, y
  end type Vector2

  type, bind(C) :: Vector3
    real(c_float) :: x, y, z
  end type Vector3

  type, bind(C) :: Vector4
    real(c_float) :: x, y, z, w
  end type Vector4

  ! Generational GPU handles
  type, bind(C) :: SituationTexture
    integer(c_int32_t) :: slot_index, generation
    integer(c_int) :: width, height
  end type SituationTexture

  type, bind(C) :: SituationShader
    integer(c_int32_t) :: slot_index, generation
  end type SituationShader

  type, bind(C) :: SituationFont
    type(c_ptr) :: fontData, stbFontInfo, glyph_info
    type(SituationTexture) :: atlas_texture
    integer(c_int) :: atlas_width, atlas_height
    real(c_float) :: font_height_pixels
    logical(c_bool) :: is_bitmap
    type(c_ptr) :: bitmap_data
    integer(c_int) :: bitmap_width, bitmap_height, bitmap_count
    integer(c_int) :: first_char, chars_per_row, chars_per_col
    integer(c_int) :: display_cell_width, display_cell_height
    real(c_float) :: char_spacing, line_spacing
  end type SituationFont

  type, bind(C) :: SituationPackedFont
    integer(c_int) :: char_width, char_height, display_height
    integer(c_int) :: char_count, first_char, chars_per_row
    integer(c_int) :: bits_per_row, data_bits, data_bit_offset
    logical(c_bool) :: bit_order_msb_first
    integer(c_int) :: top_padding, bottom_padding, left_padding, right_padding
    integer(c_int) :: atlas_chars_per_row, atlas_chars_per_col
    logical(c_bool) :: enable_outline
    integer(c_int) :: outline_thickness
    integer(c_int8_t) :: outline_r, outline_g, outline_b, outline_a
    integer(c_int8_t) :: font_r, font_g, font_b, font_a
  end type SituationPackedFont

  ! Additional opaque/resource handles
  type, bind(C) :: SituationSound
    integer(c_int32_t) :: slot_index, generation
  end type SituationSound

  type, bind(C) :: SituationBuffer
    integer(c_int32_t) :: slot_index, generation
    integer(c_size_t) :: size_in_bytes
    integer(c_int32_t) :: usage_flags
  end type SituationBuffer

  type, bind(C) :: SituationMesh
    integer(c_int32_t) :: slot_index, generation
    integer(c_int) :: index_count, vertex_count
    integer(c_size_t) :: vertex_stride
  end type SituationMesh

  type, bind(C) :: ColorHSV
    real(c_float) :: h, s, v
  end type ColorHSV

  type, bind(C) :: ColorYPQA
    integer(c_int8_t) :: y, p, q, a
  end type ColorYPQA

  type, bind(C) :: ColorYPQf
    real(c_float) :: y, p, q, a
  end type ColorYPQf

  type, bind(C) :: ColorRGBA10
    integer(c_int16_t) :: r, g, b, a
  end type ColorRGBA10

  type, bind(C) :: SituationImage
    type(c_ptr) :: data
    integer(c_int) :: width, height, channels, color_encoding
  end type SituationImage

  type, bind(C) :: SituationModel
    integer(c_int32_t) :: slot_index, generation
    integer(c_int) :: mesh_count
    type(c_ptr) :: meshes
  end type SituationModel

  type, bind(C) :: SituationOSInfo
    character(kind=c_char) :: name(64)
    character(kind=c_char) :: version(64)
    integer(c_int32_t) :: build_number
  end type SituationOSInfo

  type, bind(C) :: SituationDeviceInfo
    character(kind=c_char) :: cpu_name(64)
    integer(c_int) :: cpu_cores
    real(c_float) :: cpu_clock_speed_ghz
    character(kind=c_char) :: gpu_name(64)
    integer(c_int64_t) :: gpu_dedicated_memory_bytes
    integer(c_int64_t) :: total_ram_bytes
    integer(c_int64_t) :: available_ram_bytes
    integer(c_int) :: storage_device_count
    character(kind=c_char) :: storage_device_names(8, 64)
    integer(c_int64_t) :: storage_capacity_bytes(8)
    integer(c_int64_t) :: storage_free_bytes(8)
    integer(c_int) :: network_adapter_count
    character(kind=c_char) :: network_adapter_names(8, 64)
    integer(c_int) :: input_device_count
    character(kind=c_char) :: input_device_names(8, 64)
    integer(c_int) :: display_count
    character(kind=c_char) :: display_names(8, 64)
    integer(c_int) :: display_widths(8)
    integer(c_int) :: display_heights(8)
    integer(c_int) :: display_refresh_rates(8)
  end type SituationDeviceInfo

  type, bind(C) :: SituationComputePipeline
    integer(c_int32_t) :: slot_index, generation
  end type SituationComputePipeline

  type, bind(C) :: SituationTextureBlitRegion
    integer(c_int) :: x, y, width, height
  end type SituationTextureBlitRegion

  ! cglm mat4 equivalent (linear 16 floats — row-major C layout)
  type, bind(C) :: Mat4
    real(c_float) :: m(16)
  end type Mat4

  type, bind(C) :: SituationInitInfo
    integer(c_int) :: window_width, window_height
    type(c_ptr) :: window_title
    integer(c_int32_t) :: initial_active_window_flags, initial_inactive_window_flags
    logical(c_bool) :: enable_vulkan_validation, force_single_queue
    integer(c_int32_t) :: max_frames_in_flight
    type(c_ptr) :: required_vulkan_extensions
    integer(c_int32_t) :: required_vulkan_extension_count, flags, max_audio_voices, io_queue_capacity
    logical(c_bool) :: disable_io_thread
    real(c_double) :: hot_reload_poll_rate
    integer(c_int64_t) :: staging_buffer_size
    integer(c_int64_t) :: thread_affinity_main, thread_affinity_render, thread_affinity_audio
    logical(c_bool) :: numa_prefer_local, worker_numa_spread
    integer(c_int32_t) :: io_thread_numa_node
    logical(c_bool) :: thread_pool_use_physical_cores
    integer(c_int32_t) :: thread_pool_reserved_threads
  end type SituationInitInfo

  ! Opaque handles — layout unknown; pass as type(c_ptr) at FFI boundaries.
  type :: SituationThreadPool
    private
    integer(c_int) :: opaque_stub
  end type SituationThreadPool

  type :: SituationAudioGraph
    private
    integer(c_int) :: opaque_stub
  end type SituationAudioGraph

  type :: SituationNode
    private
    integer(c_int) :: opaque_stub
  end type SituationNode

  type :: SituationCameraDesc
    private
    integer(c_int) :: opaque_stub
  end type SituationCameraDesc

  type :: SituationRenderPassInfo
    private
    integer(c_int) :: opaque_stub
  end type SituationRenderPassInfo
"""


def render_enum_constants(e_name: str, members: list[tuple[str, str | None]], define_map: dict[str, str]) -> str:
    member_values: dict[str, int] = {}
    for name, value in members:
        if value is None:
            continue
        try:
            resolved = resolve_enum_value(value, define_map)
            expr = resolved
            for mname, mval in member_values.items():
                expr = re.sub(r"\b" + re.escape(mname) + r"\b", str(mval), expr)
            member_values[name] = eval(expr)  # noqa: S307 — controlled input
        except Exception:
            pass

    lines = [f"  ! --- {e_name} ---"]
    next_seq = 0
    for name, value in members:
        if value is None:
            numeric = next_seq
            lines.append(f"  integer(c_int), parameter :: {name} = {numeric}")
            member_values[name] = numeric
            next_seq = numeric + 1
        else:
            resolved = resolve_enum_value(value, define_map)
            expanded = resolved
            for mname, mval in member_values.items():
                expanded = re.sub(r"\b" + re.escape(mname) + r"\b", str(mval), expanded)
            try:
                numeric = eval(expanded)  # noqa: S307 — controlled input
                lines.append(f"  integer(c_int), parameter :: {name} = {numeric}")
                member_values[name] = numeric
                next_seq = numeric + 1
            except Exception:
                lines.append(f"  integer(c_int), parameter :: {name} = {expanded}")
                try:
                    numeric = int(expanded)
                    member_values[name] = numeric
                    next_seq = numeric + 1
                except Exception:
                    pass
    return "\n".join(lines)


def render_enums() -> str:
    define_map = build_define_map()
    parts: list[str] = []
    errno = parse_errno_enum()
    parts.append(render_enum_constants(errno.name, errno.members, define_map))
    parts.append("")
    for e in parse_enums():
        if e.name == "SituationError":
            continue
        if len(e.members) < 2:
            continue
        parts.append(render_enum_constants(e.name, e.members, define_map))
        parts.append("")
    return "\n".join(parts).rstrip() + "\n"


def render_callback_procedure(cb_name: str, signature: str, comment: str) -> list[str]:
    m = re.match(r"(.+?)\s*\(\*\s*\w+\s*\)\s*\((.*)\)", signature)
    if not m:
        return [f"    ! SKIP (unparsed callback): {cb_name}"]

    ret_str = m.group(1).strip()
    ret = VOID_RETURN if ret_str == "void" else c_type_to_fortran(ret_str)
    params: list[tuple[str, str]] = []
    for p in split_params(m.group(2)):
        parsed = parse_param(p, c_type_to_fortran)
        if parsed:
            params.append(parsed)

    param_types = [ret] if ret != VOID_RETURN else []
    param_types.extend(pt for _pn, pt in params)
    imports = collect_imports(param_types)

    lines: list[str] = []
    if comment:
        lines.append(f"    ! {comment}")

    if ret == VOID_RETURN:
        lines.append(f"    subroutine {cb_name}({', '.join(pn for pn, _ in params)}) bind(C)")
        lines.append(render_import_line(imports).rstrip())
        for pn, pt in params:
            decl = fortran_param_decl(pn, pt)
            if decl:
                lines.append(decl)
        lines.append(f"    end subroutine {cb_name}")
    else:
        lines.append(
            f"    function {cb_name}({', '.join(pn for pn, _ in params)}) bind(C) result(res)"
        )
        lines.append(render_import_line(imports).rstrip())
        lines.append(fortran_result_decl(ret))
        for pn, pt in params:
            decl = fortran_param_decl(pn, pt)
            if decl:
                lines.append(decl)
        lines.append(f"    end function {cb_name}")
    return lines


def render_callbacks() -> str:
    text = CALLBACKS_H.read_text(encoding="utf-8", errors="replace")
    cbs = parse_callbacks(text)

    lines = [
        "module situation_callbacks",
        "  use iso_c_binding",
        "  use situation_types",
        "  implicit none",
        "",
        "  ! Callback signatures from sit/situation_base_callbacks.h",
        "  ! Register with SituationSet* APIs using type(c_funptr).",
        "",
        "  abstract interface",
    ]

    for cb in cbs:
        cb_name = cb.name if cb.name.endswith("Callback") or cb.name == "GLFWerrorfun" else cb.name + "Callback"
        lines.extend(render_callback_procedure(cb_name, cb.signature, cb.comment))
        lines.append("")

    lines.append("  end interface")
    lines.append("")
    lines.append("end module situation_callbacks")
    return "\n".join(lines).rstrip() + "\n"


def render_foreign_procedure(name: str, ret: str, params: list[tuple[str, str]], comment: str) -> list[str]:
    param_types = [ret] if ret != VOID_RETURN else []
    param_types.extend(pt for _pn, pt in params)
    imports = collect_imports(param_types)

    lines: list[str] = []
    if comment:
        lines.append(f"    ! {comment}")

    if ret == VOID_RETURN:
        lines.append(f"    subroutine {name}({', '.join(pn for pn, _ in params)}) bind(C, name='{name}')")
        lines.append(render_import_line(imports).rstrip())
        for pn, pt in params:
            decl = fortran_param_decl(pn, pt)
            if decl:
                lines.append(decl)
        lines.append(f"    end subroutine {name}")
    else:
        lines.append(
            f"    function {name}({', '.join(pn for pn, _ in params)}) bind(C, name='{name}') result(res)"
        )
        lines.append(render_import_line(imports).rstrip())
        lines.append(fortran_result_decl(ret))
        for pn, pt in params:
            decl = fortran_param_decl(pn, pt)
            if decl:
                lines.append(decl)
        lines.append(f"    end function {name}")
    return lines


def render_foreign(entries: list[ApiEntry]) -> tuple[str, list[str]]:
    NULLABLE_OVERRIDES: dict[str, str] = {
        "SituationSetActiveGraph.0": "type(c_ptr)",
    }

    lines = [
        "module situation_foreign",
        "  use iso_c_binding",
        "  use situation_types",
        "  implicit none",
        "",
        "  interface",
    ]
    foreign_names: list[str] = []
    seen_names: set[str] = set()

    for e in sorted(entries, key=lambda x: x.name):
        if e.manual_only or e.name in MANUAL_FUNCTIONS or e.name in FORTRAN_MANUAL_ONLY_FUNCTIONS:
            continue
        if e.name in seen_names:
            continue
        seen_names.add(e.name)

        parsed = parse_function_signature(e.signature, c_type_to_fortran)
        if not parsed:
            lines.append(f"    ! SKIP (unparsed): {e.name}")
            continue

        ret, name, params = parsed
        for idx, (pn, pt) in enumerate(params):
            override_key = f"{name}.{idx}"
            if override_key in NULLABLE_OVERRIDES:
                params[idx] = (pn, NULLABLE_OVERRIDES[override_key])

        lines.extend(render_foreign_procedure(name, ret, params, e.comment))
        lines.append("")
        foreign_names.append(name)

    lines.append("  end interface")
    lines.append("")
    lines.append("end module situation_foreign")
    return "\n".join(lines).rstrip() + "\n", foreign_names


def to_fortran_define(name: str, raw_value: str) -> str:
    val = raw_value.strip()
    val = re.sub(r"\s*/\*.*?\*/\s*$", "", val).strip()
    val = re.sub(r"\s*//.*$", "", val).strip()
    if val.endswith("f"):
        return f"  real(c_float), parameter :: {name} = {val[:-1]}"
    if val.startswith("0x") or val.startswith("0X"):
        return f"  integer(c_int), parameter :: {name} = {int(val, 16)}"
    if "|" in val:
        define_map = build_define_map()
        resolved = resolve_enum_value(val, define_map)
        try:
            numeric = eval(resolved)  # noqa: S307 — controlled input
            return f"  integer(c_int), parameter :: {name} = {numeric}"
        except Exception:
            return f"  integer(c_int), parameter :: {name} = {resolved}"
    return f"  integer(c_int), parameter :: {name} = {val}"


def render_constants() -> tuple[str, list[str]]:
    lines = [
        "module situation_constants",
        "  use iso_c_binding",
        "  implicit none",
        "",
        "  ! Selected constants from sit/situation_base_etc.h",
    ]
    names: list[str] = []
    for d in parse_defines():
        lines.append(to_fortran_define(d.name, d.value))
        names.append(d.name)
    lines.append("")
    lines.append("end module situation_constants")
    return "\n".join(lines) + "\n", names


def render_helpers() -> str:
    return """module situation_helpers
  use iso_c_binding
  use situation_types
  use situation_foreign, only: SituationPollInputEvents, SituationUpdateTimers
  implicit none

contains

  subroutine situation_begin_frame()
    call SituationPollInputEvents()
    call SituationUpdateTimers()
  end subroutine situation_begin_frame

  function sit_ok(err) result(ok)
    integer(c_int), intent(in), value :: err
    logical :: ok
    ok = (err == SITUATION_SUCCESS)
  end function sit_ok

end module situation_helpers
"""


def render_types_module(entries: list[ApiEntry]) -> tuple[str, list[str]]:
    body = render_manual_types() + "\n" + render_enums() + render_opaque_stubs(entries)
    enum_names: list[str] = []
    errno = parse_errno_enum()
    enum_names.extend(name for name, _ in errno.members)
    for e in parse_enums():
        if e.name == "SituationError":
            continue
        if len(e.members) < 2:
            continue
        enum_names.extend(name for name, _ in e.members)

    struct_names = sorted(STRUCT_TYPES | {
        "SituationThreadPool",
        "SituationAudioGraph",
        "SituationNode",
        "SituationCameraDesc",
        "SituationRenderPassInfo",
        "SituationMesh",
        "SituationImage",
        "SituationModel",
        "ColorHSV",
        "ColorYPQA",
        "ColorYPQf",
        "SituationOSInfo",
        "SituationDeviceInfo",
    })

    lines = [
        "module situation_types",
        "  use iso_c_binding",
        "  implicit none",
        "",
        body.rstrip(),
        "",
        "end module situation_types",
    ]
    export_names = struct_names + sorted(set(enum_names))
    return "\n".join(lines) + "\n", export_names


def render_manual_md(entries: list[ApiEntry]) -> str:
    manual = [e for e in entries if e.manual_only or e.name in MANUAL_FUNCTIONS]
    lines = [
        "# Situation Fortran — Manual bindings",
        "",
        "These symbols are **not** emitted in `situation_foreign.f90`. Add hand-written Fortran wrappers as needed.",
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
        "# Situation Fortran bindings — API index",
        "",
        f"_Generated {datetime.now(timezone.utc).strftime('%Y-%m-%d %H:%M UTC')} from `sit/situation_api.h` — Situation **{version}**._",
        "",
        f"**Foreign imports:** {len(foreign_entries(entries))}",
        "",
        "| Function | Section | Fortran | Notes |",
        "|----------|---------|---------|-------|",
    ]
    for e in sorted(entries, key=lambda x: x.name):
        if e.manual_only or e.name in MANUAL_FUNCTIONS:
            fkind = "manual"
            note = e.manual_reason or "manual wrapper"
        else:
            fkind = "auto"
            note = e.comment[:80] + ("..." if len(e.comment) > 80 else "")
        lines.append(f"| `{e.name}` | {e.section} | {fkind} | {note or '—'} |")
    lines.append("")
    return "\n".join(lines)


def collect_referenced_types(entries: list[ApiEntry]) -> set[str]:
    referenced: set[str] = set()
    type_pattern = re.compile(r"\b([A-Z][A-Za-z0-9_]*)\b")
    for e in foreign_entries(entries):
        parsed = parse_function_signature(e.signature, c_type_to_fortran)
        if not parsed:
            continue
        ret, _name, params = parsed
        if ret != VOID_RETURN:
            for m in type_pattern.finditer(ret):
                referenced.add(m.group(1))
        for _pname, ptype in params:
            for m in type_pattern.finditer(ptype):
                referenced.add(m.group(1))
    return referenced


def render_opaque_stubs(entries: list[ApiEntry]) -> str:
    referenced = collect_referenced_types(entries)
    known_types = {
        *STRUCT_TYPES,
        *ISO_TYPES,
        "SituationThreadPool",
        "SituationAudioGraph",
        "SituationNode",
        "SituationCameraDesc",
        "SituationRenderPassInfo",
        "SituationJobId",
        "SituationNodeHandle",
        "SituationModifiers",
        "SituationStreamSize",
        "SituationStreamResult",
        "SituationSeekOrigin",
        "ma_int64",
        "ma_seek_origin",
        "SituationLogLevel",
        "SituationInitState",
        "FILE",
        "logical",
        "integer",
        "real",
        "type",
        "bind",
        "value",
        "import",
    }
    for e in parse_enums():
        known_types.add(e.name)
    errno = parse_errno_enum()
    known_types.add(errno.name)

    cb_text = CALLBACKS_H.read_text(encoding="utf-8", errors="replace")
    for cb in parse_callbacks(cb_text):
        name = cb.name if cb.name.endswith("Callback") or cb.name == "GLFWerrorfun" else cb.name + "Callback"
        known_types.add(name)

    missing = sorted(referenced - known_types)
    if not missing:
        return ""

    lines = [
        "",
        "  ! --- Auto-generated opaque type stubs ---",
        "  ! These types are referenced by the API but their layout is internal.",
        "  ! Pass them as type(c_ptr) at FFI boundaries.",
        "",
    ]
    for t in missing:
        if len(t) <= 2 or t in ("Select", "First", "String"):
            continue
        lines.append(f"  type :: {t}")
        lines.append("    private")
        lines.append("    integer(c_int) :: opaque_stub")
        lines.append(f"  end type {t}")
        lines.append("")
    return "\n".join(lines)


def render_package_root(
    version: str,
    type_exports: list[str],
    foreign_names: list[str],
    constant_names: list[str],
) -> str:
    type_only = ", &\n    ".join(type_exports)
    foreign_only = ", &\n    ".join(foreign_names)
    const_only = ", &\n    ".join(constant_names)
    return f"""! Fortran ISO_C_BINDING FFI for the Situation C library (auto-generated bindings).
! Situation {version}
!
! Re-generate:
!   python tools/generate_fortran_bindings.py
!
! Requires a pre-built import library:
!   build_situation.bat opengl  ->  build/dll/situation_opengl.lib
!
! Usage:
!   use situation
!   call situation_begin_frame()
!   if (sit_ok(err)) then ...

module situation
  use situation_types, only: &
    {type_only}
  use situation_callbacks
  use situation_foreign, only: &
    {foreign_only}
  use situation_constants, only: &
    {const_only}
  use situation_helpers, only: situation_begin_frame, sit_ok
  implicit none
  private
  public :: &
    {type_only}, &
    {foreign_only}, &
    {const_only}, &
    situation_begin_frame, sit_ok
end module situation
"""


def wrap_module_file(banner: str, module_body: str) -> str:
    return banner + module_body


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate Fortran ISO_C_BINDING bindings for Situation")
    parser.add_argument(
        "--lib",
        default="situation_opengl",
        help="Library name for linking (without .lib extension)",
    )
    args = parser.parse_args()
    _ = args.lib

    version = read_version()
    entries = parse_api_header()
    banner = gen_banner("generate_fortran_bindings.py", version, "!")

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    WRAPPER_ROOT.mkdir(parents=True, exist_ok=True)

    types_body, type_exports = render_types_module(entries)
    foreign_body, foreign_names = render_foreign(entries)
    constants_body, constant_names = render_constants()

    files_written = 0
    (OUT_DIR / "situation_types.f90").write_text(
        wrap_module_file(banner, types_body), encoding="utf-8"
    )
    files_written += 1
    (OUT_DIR / "situation_callbacks.f90").write_text(
        wrap_module_file(banner, render_callbacks()), encoding="utf-8"
    )
    files_written += 1
    (OUT_DIR / "situation_foreign.f90").write_text(
        wrap_module_file(banner, foreign_body), encoding="utf-8"
    )
    files_written += 1
    (OUT_DIR / "situation_constants.f90").write_text(
        wrap_module_file(banner, constants_body), encoding="utf-8"
    )
    files_written += 1
    (OUT_DIR / "situation_helpers.f90").write_text(
        wrap_module_file(banner, render_helpers()), encoding="utf-8"
    )
    files_written += 1
    (OUT_DIR / "situation.f90").write_text(
        wrap_module_file(banner, render_package_root(version, type_exports, foreign_names, constant_names)),
        encoding="utf-8",
    )
    files_written += 1
    (WRAPPER_ROOT / "API_INDEX.md").write_text(render_api_index(entries, version), encoding="utf-8")
    files_written += 1
    (WRAPPER_ROOT / "MANUAL_BINDINGS.md").write_text(render_manual_md(entries), encoding="utf-8")
    files_written += 1

    auto = len(foreign_entries(entries))
    cb_count = len(parse_callbacks(CALLBACKS_H.read_text(encoding="utf-8", errors="replace")))
    print(f"Situation {version}")
    print(f"Fortran bindings: {auto} bind(C) procedures, {cb_count} callback interfaces, {len(entries)} total SITAPI")
    print(f"Wrote {files_written} files under {OUT_DIR.relative_to(ROOT)}/")
    print(f"Wrote wrappers/Fortran/API_INDEX.md")


if __name__ == "__main__":
    main()