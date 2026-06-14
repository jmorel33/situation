#!/usr/bin/env python3
"""Generate Zig FFI bindings from Situation public C headers.

Outputs (under wrappers/zig/src/):
  situation_types.zig      — enums, extern structs, opaque handles
  situation_callbacks.zig  — callconv(.C) callback aliases
  situation_foreign.zig    — extern "c" fn declarations
  situation_constants.zig  — selected SIT_* #defines
  situation_helpers.zig    — frame macro helpers
  API_INDEX.md             — categorized binding index
  MANUAL_BINDINGS.md       — symbols requiring hand-written Zig wrappers

Usage:
  python tools/generate_zig_bindings.py
  python tools/generate_zig_bindings.py --lib situation_opengl
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

OUT_DIR = ROOT / "wrappers" / "zig" / "src"
PACKAGE_ROOT = ROOT / "wrappers" / "zig" / "src" / "situation.zig"

CTYPE_MAP: dict[str, str] = {
    "void": "void",
    "bool": "bool",
    "char": "c_char",
    "int": "c_int",
    "unsigned int": "c_uint",
    "unsigned char": "u8",
    "signed char": "i8",
    "short": "i16",
    "unsigned short": "u16",
    "long": "c_long",
    "unsigned long": "c_ulong",
    "long long": "i64",
    "unsigned long long": "u64",
    "float": "f32",
    "double": "f64",
    "size_t": "usize",
    "uint8_t": "u8",
    "uint16_t": "u16",
    "uint32_t": "u32",
    "uint64_t": "u64",
    "int8_t": "i8",
    "int16_t": "i16",
    "int32_t": "c_int",
    "int64_t": "i64",
    "SituationError": "SituationError",
    "SituationCommandBuffer": "SituationCommandBuffer",
    "SituationInitState": "SituationInitState",
    "SituationLogLevel": "SituationLogLevel",
    "ColorRGBA": "ColorRGBA",
    "Color": "ColorRGBA",
    "Vector2": "Vector2",
    "Vector3": "Vector3",
    "Vector4": "Vector4",
    "SitRectangle": "SitRectangle",
    "SituationInitInfo": "SituationInitInfo",
    "SituationImage": "SituationImage",
    "SituationFont": "SituationFont",
    "SituationTexture": "SituationTexture",
    "SituationShader": "SituationShader",
    "SituationMesh": "SituationMesh",
    "SituationBuffer": "SituationBuffer",
    "SituationSound": "SituationSound",
    "SituationThreadPool": "SituationThreadPool",
    "SituationAudioGraph": "SituationAudioGraph",
    "SituationNode": "SituationNode",
    "SituationVirtualDisplay": "SituationVirtualDisplay",
    "SituationRenderPassInfo": "SituationRenderPassInfo",
    "SituationClearValue": "SituationClearValue",
    "SituationTimerSystem": "SituationTimerSystem",
    "ma_result": "c_int",
    "ma_uint64": "u64",
    "mat4": "Mat4",
}


def c_type_to_zig(ctype: str) -> str:
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
            return "void"
        if ptr_depth == 1:
            return "?*anyopaque"
        return "?*" * ptr_depth + "anyopaque"

    if base == "char":
        if ptr_depth == 0:
            return "c_char"
        if ptr_depth == 1:
            return "[*:0]const u8"
        if ptr_depth == 2:
            return "?[*][*:0]const u8"
        return "?[*][*:0]const u8"

    if base in CTYPE_MAP:
        zig = CTYPE_MAP[base]
    elif base.startswith("Situation") or base in ("ColorRGBA", "Vector2", "Vector3", "Vector4", "SitRectangle"):
        zig = base
    else:
        zig = base if base else "?*anyopaque"

    if ptr_depth == 0:
        return zig
    prefix = "[*" + "]" * (ptr_depth - 1) if ptr_depth > 1 else "*"
    if ptr_depth == 1:
        return "*" + zig
    return prefix + zig


def render_manual_types() -> str:
    return '''// Core math & color (ABI must match sit/situation_api.h)
pub const ColorRGBA = extern struct {
    r: u8,
    g: u8,
    b: u8,
    a: u8,
};

pub const SitRectangle = extern struct {
    x: f32,
    y: f32,
    width: f32,
    height: f32,
};

pub const Vector2 = extern struct {
    x: f32,
    y: f32,
};

pub const Vector3 = extern struct {
    x: f32,
    y: f32,
    z: f32,
};

pub const Vector4 = extern struct {
    x: f32,
    y: f32,
    z: f32,
    w: f32,
};

// Generational GPU handles
pub const SituationTexture = extern struct {
    slot_index: u32,
    generation: u32,
    width: c_int,
    height: c_int,
};

pub const SituationShader = extern struct {
    slot_index: u32,
    generation: u32,
};

pub const SituationFont = extern struct {
    texture: SituationTexture,
    font_size: f32,
    ascent: f32,
    descent: f32,
    line_gap: f32,
    glyph_count: c_int,
    is_bitmap: bool,
};

// Opaque handles (layout unknown — pass as pointer)
pub const SituationCommandBuffer = *anyopaque;
pub const SituationThreadPool = opaque {};
pub const SituationAudioGraph = opaque {};
pub const SituationNode = opaque {};

// Scalar type aliases for C/miniaudio interop
pub const SituationJobId = u32;
pub const SituationNodeHandle = u32;
pub const ma_int64 = i64;
pub const ma_seek_origin = c_int;

// Additional opaque/resource handles
pub const SituationSound = extern struct {
    slot_index: u32,
    generation: u32,
};

pub const SituationBuffer = extern struct {
    slot_index: u32,
    generation: u32,
};

pub const SituationComputePipeline = extern struct {
    slot_index: u32,
    generation: u32,
};

// Structs referenced by the API but defined internally
pub const SituationCameraDesc = opaque {};
pub const SituationRenderPassInfo = opaque {};
pub const SituationTextureBlitRegion = extern struct {
    x: c_int,
    y: c_int,
    width: c_int,
    height: c_int,
};

// cglm mat4 equivalent
pub const Mat4 = [4][4]f32;
/// C-naming alias (cglm uses mat4, so FFI uses this name).
pub const mat4 = Mat4;

pub const SituationInitInfo = extern struct {
    window_width: c_int,
    window_height: c_int,
    window_title: [*:0]const u8,
    initial_active_window_flags: u32,
    initial_inactive_window_flags: u32,
    enable_vulkan_validation: bool,
    force_single_queue: bool,
    max_frames_in_flight: u32,
    required_vulkan_extensions: [*][*:0]const u8,
    required_vulkan_extension_count: u32,
    flags: u32,
    max_audio_voices: u32,
    io_queue_capacity: u32,
    disable_io_thread: bool,
    hot_reload_poll_rate: f64,
    staging_buffer_size: u64,
    thread_affinity_main: u64,
    thread_affinity_render: u64,
    thread_affinity_audio: u64,
    numa_prefer_local: bool,
    worker_numa_spread: bool,
    io_thread_numa_node: i32,
    thread_pool_use_physical_cores: bool,
    thread_pool_reserved_threads: u32,
};
'''


def render_enum(e_name: str, members: list[tuple[str, str | None]], define_map: dict[str, str]) -> str:
    # First pass: build a map of member name → resolved integer value for compound resolution.
    member_values: dict[str, int] = {}
    for name, value in members:
        if value is None:
            # Sequentially assigned; we don't need to track these for compound resolution
            # since Zig handles plain sequential members fine.
            pass
        else:
            try:
                resolved = resolve_enum_value(value, define_map)
                # Try to evaluate arithmetic/bitwise expressions using already-resolved members
                expr = resolved
                for mname, mval in member_values.items():
                    expr = re.sub(r'\b' + re.escape(mname) + r'\b', str(mval), expr)
                member_values[name] = eval(expr)  # noqa: S307 — controlled input
            except Exception:
                pass

    lines = [f"pub const {e_name} = enum(c_int) {{"]
    for name, value in members:
        if value is None:
            lines.append(f"    {name},")
        else:
            resolved = resolve_enum_value(value, define_map)
            # Replace any enum member references with their integer values
            expanded = resolved
            for mname, mval in member_values.items():
                expanded = re.sub(r'\b' + re.escape(mname) + r'\b', str(mval), expanded)
            # Evaluate if it's a pure arithmetic/bitwise expression
            try:
                numeric = eval(expanded)  # noqa: S307 — controlled input
                lines.append(f"    {name} = {numeric},")
            except Exception:
                lines.append(f"    {name} = {expanded},")
    lines.append("};")
    return "\n".join(lines)


def render_enums() -> str:
    define_map = build_define_map()
    parts: list[str] = []
    errno = parse_errno_enum()
    parts.append(render_enum(errno.name, errno.members, define_map))
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
    cbs = parse_callbacks(text)

    # Collect all non-primitive types referenced by callbacks.
    type_pattern_cb = re.compile(r"\b([A-Z][A-Za-z0-9_]*)\b")
    primitive_names = {
        "void", "bool", "u8", "u16", "u32", "u64", "i8", "i16", "i32", "i64",
        "f32", "f64", "usize", "anyopaque", "c_int", "c_uint", "c_long",
        "c_ulong", "c_char", "c_short", "c_ushort",
    }
    referenced: set[str] = set()
    for cb in cbs:
        m = re.match(r"(.+?)\s*\(\*\s*\w+\s*\)\s*\((.*)\)", cb.signature)
        if m:
            ret_str = m.group(1).strip()
            if ret_str != "void":
                for tm in type_pattern_cb.finditer(c_type_to_zig(ret_str)):
                    referenced.add(tm.group(1))
            for p in split_params(m.group(2)):
                parsed = parse_param(p, c_type_to_zig)
                if parsed:
                    for tm in type_pattern_cb.finditer(parsed[1]):
                        referenced.add(tm.group(1))
    # Always import lowercase alias types that live in situation_types.zig but
    # won't be caught by the uppercase [A-Z] regex scanner.
    always_import_cb = {"ma_int64", "ma_seek_origin"}
    type_imports = sorted((referenced - primitive_names - {""}) | always_import_cb)

    lines = ['const types = @import("situation_types.zig");', ""]
    for t in type_imports:
        lines.append(f"const {t} = types.{t};")
    if type_imports:
        lines.append("")
    lines.append("// Callback typedefs from sit/situation_api.h — register with SituationSet* APIs.")
    lines.append("")
    for cb in cbs:
        zig_name = cb.name if cb.name.endswith("Callback") else cb.name + "Callback"
        m = re.match(r"(.+?)\s*\(\*\s*\w+\s*\)\s*\((.*)\)", cb.signature)
        params: list[str] = []
        ret = "void"
        if m:
            ret_str = m.group(1).strip()
            if ret_str != "void":
                ret = c_type_to_zig(ret_str)
            for p in split_params(m.group(2)):
                parsed = parse_param(p, c_type_to_zig)
                if parsed:
                    params.append(f"{parsed[0]}: {parsed[1]}")
        param_str = ", ".join(params)
        lines.append(f"pub const {zig_name} = *const fn ({param_str}) callconv(.C) {ret};")
        if cb.comment:
            lines.append(f"/// {cb.comment}")
        lines.append("")
    return "\n".join(lines).rstrip() + "\n"


def render_foreign(entries: list[ApiEntry]) -> str:
    # Per-function parameter type overrides for cases where the C API accepts NULL
    # but the parser emits a non-nullable pointer.  key = "FuncName.param_index" (0-based).
    NULLABLE_OVERRIDES: dict[str, str] = {
        "SituationSetActiveGraph.0": "?*SituationAudioGraph",
    }
    # Collect all types referenced by the foreign functions and import them explicitly.
    type_pattern = re.compile(r"\b([A-Z][A-Za-z0-9_]*)\b")
    referenced: set[str] = set()
    seen_names: set[str] = set()
    for e in sorted(entries, key=lambda x: x.name):
        if e.manual_only or e.name in MANUAL_FUNCTIONS:
            continue
        if e.name in seen_names:
            continue
        seen_names.add(e.name)
        parsed = parse_function_signature(e.signature, c_type_to_zig)
        if not parsed:
            continue
        ret, name, params = parsed
        for m in type_pattern.finditer(ret):
            referenced.add(m.group(1))
        for _pn, pt in params:
            for m in type_pattern.finditer(pt):
                referenced.add(m.group(1))

    # Only import types that live in situation_types.zig
    primitive_names = {
        "void", "bool", "u8", "u16", "u32", "u64", "i8", "i16", "i32", "i64",
        "f32", "f64", "usize", "anyopaque", "c_int", "c_uint", "c_long",
        "c_ulong", "c_char", "c_short", "c_ushort",
    }
    # Always import these lowercase aliases that live in situation_types but won't
    # be caught by the uppercase regex.
    always_import = {"mat4", "ma_int64", "ma_seek_origin", "FILE"}
    type_imports = sorted((referenced - primitive_names - {""}) | always_import)

    lines = ['const types = @import("situation_types.zig");', ""]
    for t in type_imports:
        lines.append(f"const {t} = types.{t};")
    if type_imports:
        lines.append("")

    seen_names = set()
    for e in sorted(entries, key=lambda x: x.name):
        if e.manual_only or e.name in MANUAL_FUNCTIONS:
            continue
        if e.name in seen_names:
            continue
        seen_names.add(e.name)
        parsed = parse_function_signature(e.signature, c_type_to_zig)
        if not parsed:
            lines.append(f"// SKIP (unparsed): {e.name}")
            continue
        ret, name, params = parsed
        param_parts = []
        for idx, (pn, pt) in enumerate(params):
            override_key = f"{name}.{idx}"
            if override_key in NULLABLE_OVERRIDES:
                pt = NULLABLE_OVERRIDES[override_key]
            param_parts.append(f"{pn}: {pt}")
        param_str = ", ".join(param_parts)
        if e.comment:
            lines.append(f"/// {e.comment}")
        if ret == "void":
            lines.append(f"pub extern fn {name}({param_str}) void;")
        else:
            lines.append(f"pub extern fn {name}({param_str}) {ret};")
        if e.comment:
            lines.append("")
    return "\n".join(lines) + "\n"


def render_constants() -> str:
    lines = ["// Selected constants from sit/situation_base_etc.h"]
    for d in parse_defines():
        val = d.value
        if val.endswith("f"):
            val = val[:-1]
        # Strip any trailing C-style block or line comment from the value
        val = re.sub(r"\s*/\*.*?\*/\s*$", "", val).strip()
        val = re.sub(r"\s*//.*$", "", val).strip()
        lines.append(f"pub const {d.name} = {val};")
    return "\n".join(lines) + "\n"


def render_helpers() -> str:
    return '''const foreign = @import("situation_foreign.zig");
const types = @import("situation_types.zig");

// Helpers replacing C preprocessor macros from situation_api.h

pub fn situationBeginFrame() void {
    foreign.SituationPollInputEvents();
    foreign.SituationUpdateTimers();
}

pub fn situationSuccess(err: types.SituationError) bool {
    return err == .SITUATION_SUCCESS;
}
'''


def render_manual_md(entries: list[ApiEntry]) -> str:
    manual = [e for e in entries if e.manual_only or e.name in MANUAL_FUNCTIONS]
    lines = [
        "# Situation Zig — Manual bindings",
        "",
        "These symbols are **not** emitted in `situation_foreign.zig`. Add hand-written Zig wrappers as needed.",
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
        "# Situation Zig bindings — API index",
        "",
        f"_Generated {datetime.now(timezone.utc).strftime('%Y-%m-%d %H:%M UTC')} from `sit/situation_api.h` — Situation **{version}**._",
        "",
        f"**Foreign imports:** {len(foreign_entries(entries))}",
        "",
        "| Function | Section | Zig | Notes |",
        "|----------|---------|-----|-------|",
    ]
    for e in sorted(entries, key=lambda x: x.name):
        if e.manual_only or e.name in MANUAL_FUNCTIONS:
            zig = "manual"
            note = e.manual_reason or "manual wrapper"
        else:
            zig = "auto"
            note = e.comment[:80] + ("..." if len(e.comment) > 80 else "")
        lines.append(f"| `{e.name}` | {e.section} | {zig} | {note or '—'} |")
    lines.append("")
    return "\n".join(lines)


def collect_referenced_types(entries: list[ApiEntry]) -> set[str]:
    referenced: set[str] = set()
    type_pattern = re.compile(r"\b([A-Z][A-Za-z0-9_]*)\b")
    for e in foreign_entries(entries):
        parsed = parse_function_signature(e.signature, c_type_to_zig)
        if not parsed:
            continue
        ret, _name, params = parsed
        if ret != "void":
            for m in type_pattern.finditer(ret):
                referenced.add(m.group(1))
        for _pname, ptype in params:
            for m in type_pattern.finditer(ptype):
                referenced.add(m.group(1))
    return referenced


def render_opaque_stubs(entries: list[ApiEntry]) -> str:
    referenced = collect_referenced_types(entries)
    known_types = {
        "c_int", "c_uint", "c_long", "c_ulong", "c_char",
        "u8", "u16", "u32", "u64", "i8", "i16", "i32", "i64",
        "f32", "f64", "usize", "bool", "anyopaque", "void",
        "ColorRGBA", "SitRectangle", "Vector2", "Vector3", "Vector4",
        "SituationTexture", "SituationShader", "SituationFont",
        "SituationCommandBuffer", "SituationThreadPool",
        "SituationAudioGraph", "SituationNode",
        "SituationJobId", "SituationNodeHandle",
        "SituationSound", "SituationBuffer", "SituationComputePipeline",
        "SituationCameraDesc", "SituationRenderPassInfo",
        "SituationTextureBlitRegion",
        "SituationInitInfo", "SituationError",
        "ma_int64", "ma_seek_origin", "Mat4", "mat4",
        "SituationLogLevel", "SituationInitState",
    }
    for e in parse_enums():
        known_types.add(e.name)
    errno = parse_errno_enum()
    known_types.add(errno.name)

    text = (ROOT / "sit" / "situation_api.h").read_text(encoding="utf-8", errors="replace")
    for cb in parse_callbacks(text):
        name = cb.name if cb.name.endswith("Callback") else cb.name + "Callback"
        known_types.add(name)

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
        if len(t) <= 2 or t in ("Select", "First", "String"):
            continue
        lines.append(f"pub const {t} = opaque {{}};")
    lines.append("")
    return "\n".join(lines)


def render_package_root(version: str) -> str:
    # usingnamespace was removed as a top-level pub declaration in Zig 0.17-dev.
    # We expose each sub-module as a named namespace so callers can do either:
    #   const situation = @import("situation");
    #   situation.foreign.SituationInit(...)           -- namespaced
    #   const sit = situation; sit.SituationInit(...)  -- via the flat re-export const below
    # The flat re-export consts mirror every sub-module so the hello example keeps working.
    return f'''//! Zig FFI for the Situation C library (auto-generated bindings).
//! Situation {version}
//!
//! Re-generate:
//!   python tools/generate_zig_bindings.py
//!
//! Requires a pre-built import library:
//!   build_situation.bat opengl  →  build/dll/situation_opengl.lib
//!
//! Usage (namespaced — recommended):
//!   const sit = @import("situation");
//!   sit.foreign.SituationInit(0, null, &config);
//!
//! Usage (flat — mirrors Odin / C style):
//!   const sit = @import("situation");
//!   sit.SituationInit(0, null, &config);   // via flat re-exports below

pub const types     = @import("situation_types.zig");
pub const callbacks = @import("situation_callbacks.zig");
pub const foreign   = @import("situation_foreign.zig");
pub const constants = @import("situation_constants.zig");
pub const helpers   = @import("situation_helpers.zig");

// --- Flat re-exports (types) ---
pub const ColorRGBA              = types.ColorRGBA;
pub const SitRectangle           = types.SitRectangle;
pub const Vector2                = types.Vector2;
pub const Vector3                = types.Vector3;
pub const Vector4                = types.Vector4;
pub const SituationTexture       = types.SituationTexture;
pub const SituationShader        = types.SituationShader;
pub const SituationFont          = types.SituationFont;
pub const SituationCommandBuffer = types.SituationCommandBuffer;
pub const SituationThreadPool    = types.SituationThreadPool;
pub const SituationAudioGraph    = types.SituationAudioGraph;
pub const SituationNode          = types.SituationNode;
pub const SituationJobId         = types.SituationJobId;
pub const SituationNodeHandle    = types.SituationNodeHandle;
pub const SituationSound         = types.SituationSound;
pub const SituationBuffer        = types.SituationBuffer;
pub const SituationComputePipeline = types.SituationComputePipeline;
pub const SituationInitInfo      = types.SituationInitInfo;
pub const SituationError         = types.SituationError;
pub const Mat4                   = types.Mat4;
pub const mat4                   = types.mat4;

// --- Flat re-exports (helpers) ---
pub const situationBeginFrame = helpers.situationBeginFrame;
pub const situationSuccess    = helpers.situationSuccess;
'''


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate Zig FFI bindings for Situation")
    parser.add_argument(
        "--lib",
        default="situation_opengl",
        help="Library name for zig build (without .lib extension)",
    )
    args = parser.parse_args()

    version = read_version()
    entries = parse_api_header()

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    (ROOT / "wrappers" / "zig").mkdir(parents=True, exist_ok=True)

    # Zig 0.14 used c_int as a local alias from std.c; Zig 0.17-dev made c_int etc. language primitives.
    # We no longer alias them — they're available globally as primitives.
    gen_header = (
        gen_banner("generate_zig_bindings.py", version, "//")
    )

    (OUT_DIR / "situation_types.zig").write_text(
        gen_header + render_manual_types() + "\n" + render_enums() + render_opaque_stubs(entries),
        encoding="utf-8",
    )
    (OUT_DIR / "situation_callbacks.zig").write_text(
        gen_header + render_callbacks(), encoding="utf-8"
    )
    (OUT_DIR / "situation_foreign.zig").write_text(
        gen_header + render_foreign(entries), encoding="utf-8"
    )
    (OUT_DIR / "situation_constants.zig").write_text(
        gen_header + render_constants(), encoding="utf-8"
    )
    (OUT_DIR / "situation_helpers.zig").write_text(
        gen_header + render_helpers(), encoding="utf-8"
    )
    (OUT_DIR.parent / "API_INDEX.md").write_text(
        render_api_index(entries, version), encoding="utf-8"
    )
    (OUT_DIR.parent / "MANUAL_BINDINGS.md").write_text(
        render_manual_md(entries), encoding="utf-8"
    )

    if not PACKAGE_ROOT.exists():
        PACKAGE_ROOT.write_text(render_package_root(version), encoding="utf-8")

    build_zig = ROOT / "wrappers" / "zig" / "build.zig"
    build_zig.write_text(
        '''const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    // -Dexample=<name>  — which example subfolder to build (default: hello_situation)
    const example_name = b.option([]const u8, "example", "Example to build") orelse "hello_situation";

    // -Dlink=<opengl|vulkan|static-opengl|static-vulkan>  — backend/link mode
    const link_mode = b.option([]const u8, "link", "Link mode: opengl, vulkan, static-opengl, static-vulkan") orelse "opengl";

    // -Dmingw_lib=<path>  — MinGW lib dir (for static builds)
    const mingw_lib = b.option([]const u8, "mingw_lib", "Path to MinGW lib directory");

    // -Dmingw_gcc_lib=<path>  — MinGW GCC lib dir (for static builds)
    const mingw_gcc_lib = b.option([]const u8, "mingw_gcc_lib", "Path to MinGW GCC lib directory");

    // -Dvk_sdk=<path>  — Vulkan SDK root (for static Vulkan builds)
    const vk_sdk = b.option([]const u8, "vk_sdk", "Path to Vulkan SDK root");

    const is_vulkan = std.mem.startsWith(u8, link_mode, "vulkan") or
                      std.mem.eql(u8, link_mode, "static-vulkan");
    const is_static = std.mem.startsWith(u8, link_mode, "static");

    const src_path = b.fmt("examples/{s}/main.zig", .{example_name});

    const situation_mod = b.createModule(.{
        .root_source_file = b.path("src/situation.zig"),
        .target = target,
        .optimize = optimize,
    });

    const exe_mod = b.createModule(.{
        .root_source_file = b.path(src_path),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
        .imports = &.{.{ .name = "situation", .module = situation_mod }},
    });

    exe_mod.addLibraryPath(b.path("../../build/dll"));

    if (is_static) {
        const lib_name = if (is_vulkan) "situation_vulkan" else "situation_opengl";
        exe_mod.linkSystemLibrary(lib_name, .{});
        if (mingw_lib) |ml| exe_mod.addLibraryPath(.{ .cwd_relative = ml });
        if (mingw_gcc_lib) |gl| exe_mod.addLibraryPath(.{ .cwd_relative = gl });
        exe_mod.linkSystemLibrary("gdi32", .{});
        exe_mod.linkSystemLibrary("winmm", .{});
        exe_mod.linkSystemLibrary("user32", .{});
        exe_mod.linkSystemLibrary("shell32", .{});
        exe_mod.linkSystemLibrary("ole32", .{});
        exe_mod.linkSystemLibrary("iphlpapi", .{});
        exe_mod.linkSystemLibrary("setupapi", .{});
        exe_mod.linkSystemLibrary("dxgi", .{});
        exe_mod.linkSystemLibrary("propsys", .{});
        exe_mod.linkSystemLibrary("shlwapi", .{});
        exe_mod.linkSystemLibrary("uuid", .{});
        exe_mod.linkSystemLibrary("xinput", .{});
        exe_mod.linkSystemLibrary("ws2_32", .{});
        exe_mod.linkSystemLibrary("psapi", .{});
        if (is_vulkan) {
            if (vk_sdk) |sdk| exe_mod.addLibraryPath(.{ .cwd_relative = b.fmt("{s}/Lib", .{sdk}) });
            exe_mod.linkSystemLibrary("vulkan-1", .{});
            exe_mod.linkSystemLibrary("shaderc_combined", .{});
        } else {
            exe_mod.linkSystemLibrary("opengl32", .{});
        }
    } else {
        const lib_name = if (is_vulkan) "situation_vulkan" else "situation_opengl";
        exe_mod.linkSystemLibrary(lib_name, .{});
    }

    const exe = b.addExecutable(.{
        .name = example_name,
        .root_module = exe_mod,
    });
    b.installArtifact(exe);
}
''',
        encoding="utf-8",
    )

    auto = len(foreign_entries(entries))
    print(f"Situation {version}")
    print(f"Zig bindings: {auto} extern fn, {len(entries)} total SITAPI")
    print(f"Wrote {OUT_DIR.relative_to(ROOT)}/situation_foreign.zig")
    print(f"Wrote wrappers/zig/API_INDEX.md")


if __name__ == "__main__":
    main()
