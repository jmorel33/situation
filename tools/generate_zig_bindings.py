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
  python tools/generate_zig_bindings.py --jam
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
    filter_jam,
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
    load_jam_slice,
    read_version,
)

OUT_DIR = ROOT / "wrappers" / "zig" / "src"
JAM_SLICE_FILE = ROOT / "tools" / "jam_api_slice.txt"
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
            return "[*][*:0]const u8"
        return "?*" * (ptr_depth - 1) + "[*:0]const u8"

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
    lines = [f"pub const {e_name} = enum(c_int) {{"]
    for name, value in members:
        if value is None:
            lines.append(f"    {name},")
        else:
            resolved = resolve_enum_value(value, define_map)
            lines.append(f"    {name} = {resolved},")
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
    lines = [
        'const types = @import("situation_types.zig");',
        "pub usingnamespace types;",
        "",
        "// Callback typedefs from sit/situation_api.h — register with SituationSet* APIs.",
        "",
    ]
    for cb in parse_callbacks(text):
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
    lines = [
        'const types = @import("situation_types.zig");',
        "pub usingnamespace types;",
        "",
    ]
    seen_names: set[str] = set()
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
        param_str = ", ".join(f"{n}: {t}" for n, t in params)
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


def render_api_index(entries: list[ApiEntry], version: str, jam: bool) -> str:
    mode = "jam slice" if jam else "full"
    lines = [
        "# Situation Zig bindings — API index",
        "",
        f"_Generated {datetime.now(timezone.utc).strftime('%Y-%m-%d %H:%M UTC')} from `sit/situation_api.h` — Situation **{version}** ({mode})._",
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
        "ma_int64", "ma_seek_origin", "Mat4",
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
    return f'''//! Zig FFI for the Situation C library (auto-generated bindings).
//! Situation {version}
//!
//! Re-generate:
//!   python tools/generate_zig_bindings.py
//!
//! Requires a pre-built import library:
//!   build_situation.bat opengl  →  build/dll/situation_opengl.lib

pub const types = @import("situation_types.zig");
pub const callbacks = @import("situation_callbacks.zig");
pub const foreign = @import("situation_foreign.zig");
pub const constants = @import("situation_constants.zig");
pub const helpers = @import("situation_helpers.zig");

pub usingnamespace types;
pub usingnamespace callbacks;
pub usingnamespace foreign;
pub usingnamespace constants;
pub usingnamespace helpers;
'''


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate Zig FFI bindings for Situation")
    parser.add_argument("--jam", action="store_true", help="Emit jam API slice only")
    parser.add_argument(
        "--lib",
        default="situation_opengl",
        help="Library name for zig build (without .lib extension)",
    )
    args = parser.parse_args()

    version = read_version()
    all_entries = parse_api_header()
    jam_names = load_jam_slice(JAM_SLICE_FILE)
    entries = filter_jam(all_entries, jam_names) if args.jam else all_entries

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    (ROOT / "wrappers" / "zig").mkdir(parents=True, exist_ok=True)

    gen_header = (
        gen_banner("generate_zig_bindings.py", version, "//")
        + "const std = @import(\"std\");\n"
        + "const c = std.c;\n\n"
        + "const c_int = c.c_int;\n"
        + "const c_uint = c.c_uint;\n"
        + "const c_long = c.c_long;\n"
        + "const c_ulong = c.c_ulong;\n"
        + "const c_char = c.c_char;\n\n"
    )

    (OUT_DIR / "situation_types.zig").write_text(
        gen_header + render_manual_types() + "\n" + render_enums() + render_opaque_stubs(entries),
        encoding="utf-8",
    )
    (OUT_DIR / "situation_callbacks.zig").write_text(
        gen_header + render_callbacks(), encoding="utf-8"
    )
    foreign_name = "situation_foreign_jam.zig" if args.jam else "situation_foreign.zig"
    (OUT_DIR / foreign_name).write_text(
        gen_header + render_foreign(entries), encoding="utf-8"
    )
    (OUT_DIR / "situation_constants.zig").write_text(
        gen_header + render_constants(), encoding="utf-8"
    )
    (OUT_DIR / "situation_helpers.zig").write_text(
        gen_header + render_helpers(), encoding="utf-8"
    )
    index_name = "API_INDEX_JAM.md" if args.jam else "API_INDEX.md"
    (OUT_DIR.parent / index_name).write_text(
        render_api_index(entries, version, args.jam), encoding="utf-8"
    )
    (OUT_DIR.parent / "MANUAL_BINDINGS.md").write_text(
        render_manual_md(all_entries), encoding="utf-8"
    )

    if not PACKAGE_ROOT.exists():
        PACKAGE_ROOT.write_text(render_package_root(version), encoding="utf-8")

    build_zig = ROOT / "wrappers" / "zig" / "build.zig"
    build_zig.write_text(
        f'''const std = @import("std");

pub fn build(b: *std.Build) void {{
    const target = b.standardTargetOptions(.{{}});
    const optimize = b.standardOptimizeOption(.{{}});

    const situation_mod = b.createModule(.{{
        .root_source_file = b.path("src/situation.zig"),
        .target = target,
        .optimize = optimize,
    }});

    const exe = b.addExecutable(.{{
        .name = "hello_situation",
        .root_module = b.createModule(.{{
            .root_source_file = b.path("examples/hello_situation/main.zig"),
            .target = target,
            .optimize = optimize,
            .imports = &.{{ .{{ .name = "situation", .module = situation_mod }} }},
        }}),
    }});
    exe.linkLibC();
    exe.addLibraryPath(b.path("../../build/dll"));
    exe.linkSystemLibrary("{args.lib}");
    b.installArtifact(exe);
}}
''',
        encoding="utf-8",
    )

    auto = len(foreign_entries(entries))
    print(f"Situation {version}")
    print(f"Zig bindings: {auto} extern fn ({'jam' if args.jam else 'full'}), {len(all_entries)} total SITAPI")
    print(f"Wrote {OUT_DIR.relative_to(ROOT)}/{foreign_name}")
    print(f"Wrote wrappers/zig/{index_name}")


if __name__ == "__main__":
    main()
