#!/usr/bin/env python3
"""Generate Rust FFI bindings from Situation public C headers.

Outputs (under wrappers/rust/src/):
  situation_types.rs      — #[repr(C)] structs, enums
  situation_callbacks.rs  — extern "C" fn pointer type aliases
  situation_ffi.rs        — extern "C" { fn ... } declarations
  situation_constants.rs  — selected SIT_* pub const
  situation_helpers.rs    — frame macro helpers
  API_INDEX.md            — categorized binding index
  MANUAL_BINDINGS.md      — symbols requiring hand-written Rust wrappers

Usage:
  python tools/generate_rust_bindings.py
  python tools/generate_rust_bindings.py --jam
  python tools/generate_rust_bindings.py --lib situation_opengl
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

OUT_DIR = ROOT / "wrappers" / "rust" / "src"
JAM_SLICE_FILE = ROOT / "tools" / "jam_api_slice.txt"
LIB_RS = ROOT / "wrappers" / "rust" / "src" / "lib.rs"

CTYPE_MAP: dict[str, str] = {
    "void": "()",
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

VOID_RETURN = "__VOID__"


def c_type_to_rust(ctype: str) -> str:
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
        if ptr_depth == 1:
            return "*mut c_void"
        inner = "*mut c_void"
        for _ in range(ptr_depth - 2):
            inner = f"*mut {inner}"
        return inner

    if base == "char":
        if ptr_depth == 0:
            return "c_char"
        if ptr_depth == 1:
            return "*const c_char"
        if ptr_depth == 2:
            return "*const *const c_char"
        inner = "*const c_char"
        for _ in range(ptr_depth - 2):
            inner = f"*const {inner}"
        return inner

    if base in CTYPE_MAP:
        rust = CTYPE_MAP[base]
    elif base.startswith("Situation") or base in ("ColorRGBA", "Vector2", "Vector3", "Vector4", "SitRectangle"):
        rust = base
    else:
        rust = base if base else "*mut c_void"

    if ptr_depth == 0:
        return rust
    if ptr_depth == 1:
        return f"*mut {rust}"
    inner = f"*mut {rust}"
    for _ in range(ptr_depth - 2):
        inner = f"*mut {inner}"
    return inner


def render_manual_types() -> str:
    return '''// Core math & color (ABI must match sit/situation_api.h)
#[repr(C)]
#[derive(Copy, Clone, Debug, Default)]
pub struct ColorRGBA {
    pub r: u8,
    pub g: u8,
    pub b: u8,
    pub a: u8,
}

#[repr(C)]
#[derive(Copy, Clone, Debug, Default)]
pub struct SitRectangle {
    pub x: f32,
    pub y: f32,
    pub width: f32,
    pub height: f32,
}

#[repr(C)]
#[derive(Copy, Clone, Debug, Default)]
pub struct Vector2 {
    pub x: f32,
    pub y: f32,
}

#[repr(C)]
#[derive(Copy, Clone, Debug, Default)]
pub struct Vector3 {
    pub x: f32,
    pub y: f32,
    pub z: f32,
}

#[repr(C)]
#[derive(Copy, Clone, Debug, Default)]
pub struct Vector4 {
    pub x: f32,
    pub y: f32,
    pub z: f32,
    pub w: f32,
}

// Generational GPU handles
#[repr(C)]
#[derive(Copy, Clone, Debug, Default)]
pub struct SituationTexture {
    pub slot_index: u32,
    pub generation: u32,
    pub width: c_int,
    pub height: c_int,
}

#[repr(C)]
#[derive(Copy, Clone, Debug, Default)]
pub struct SituationShader {
    pub slot_index: u32,
    pub generation: u32,
}

#[repr(C)]
#[derive(Copy, Clone, Debug, Default)]
pub struct SituationFont {
    pub texture: SituationTexture,
    pub font_size: f32,
    pub ascent: f32,
    pub descent: f32,
    pub line_gap: f32,
    pub glyph_count: c_int,
    pub is_bitmap: bool,
}

// Opaque handles (layout unknown — pass as pointer)
pub type SituationCommandBuffer = *mut c_void;

#[repr(C)]
pub struct SituationThreadPool {
    _private: [u8; 0],
}

#[repr(C)]
pub struct SituationAudioGraph {
    _private: [u8; 0],
}

#[repr(C)]
pub struct SituationNode {
    _private: [u8; 0],
}

// Scalar type aliases for C/miniaudio interop
pub type SituationJobId = u32;
pub type SituationNodeHandle = u32;
pub type ma_int64 = i64;
pub type ma_seek_origin = c_int;

// Additional opaque/resource handles
#[repr(C)]
#[derive(Copy, Clone, Debug, Default)]
pub struct SituationSound {
    pub slot_index: u32,
    pub generation: u32,
}

#[repr(C)]
#[derive(Copy, Clone, Debug, Default)]
pub struct SituationBuffer {
    pub slot_index: u32,
    pub generation: u32,
}

#[repr(C)]
#[derive(Copy, Clone, Debug, Default)]
pub struct SituationComputePipeline {
    pub slot_index: u32,
    pub generation: u32,
}

// Structs referenced by the API but defined internally
#[repr(C)]
pub struct SituationCameraDesc {
    _private: [u8; 0],
}

#[repr(C)]
pub struct SituationRenderPassInfo {
    _private: [u8; 0],
}

#[repr(C)]
#[derive(Copy, Clone, Debug, Default)]
pub struct SituationTextureBlitRegion {
    pub x: c_int,
    pub y: c_int,
    pub width: c_int,
    pub height: c_int,
}

// cglm mat4 equivalent
pub type Mat4 = [[f32; 4]; 4];

#[repr(C)]
pub struct SituationInitInfo {
    pub window_width: c_int,
    pub window_height: c_int,
    pub window_title: *const c_char,
    pub initial_active_window_flags: u32,
    pub initial_inactive_window_flags: u32,
    pub enable_vulkan_validation: bool,
    pub force_single_queue: bool,
    pub max_frames_in_flight: u32,
    pub required_vulkan_extensions: *const *const c_char,
    pub required_vulkan_extension_count: u32,
    pub flags: u32,
    pub max_audio_voices: u32,
    pub io_queue_capacity: u32,
    pub disable_io_thread: bool,
    pub hot_reload_poll_rate: f64,
    pub staging_buffer_size: u64,
    pub thread_affinity_main: u64,
    pub thread_affinity_render: u64,
    pub thread_affinity_audio: u64,
    pub numa_prefer_local: bool,
    pub worker_numa_spread: bool,
    pub io_thread_numa_node: i32,
    pub thread_pool_use_physical_cores: bool,
    pub thread_pool_reserved_threads: u32,
}
'''


def render_enum(e_name: str, members: list[tuple[str, str | None]], define_map: dict[str, str]) -> str:
    lines = [
        "#[repr(i32)]",
        "#[derive(Copy, Clone, Debug, PartialEq, Eq)]",
        f"pub enum {e_name} {{",
    ]
    for name, value in members:
        if value is None:
            lines.append(f"    {name},")
        else:
            resolved = resolve_enum_value(value, define_map)
            lines.append(f"    {name} = {resolved},")
    lines.append("}")
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
        "use crate::situation_types::*;",
        "use std::os::raw::{c_char, c_int, c_uint, c_void};",
        "",
        "// Callback typedefs from sit/situation_api.h — register with SituationSet* APIs.",
        "",
    ]
    for cb in parse_callbacks(text):
        rust_name = cb.name if cb.name.endswith("Callback") else cb.name + "Callback"
        m = re.match(r"(.+?)\s*\(\*\s*\w+\s*\)\s*\((.*)\)", cb.signature)
        params: list[str] = []
        ret = VOID_RETURN
        if m:
            ret_str = m.group(1).strip()
            if ret_str != "void":
                ret = c_type_to_rust(ret_str)
            for p in split_params(m.group(2)):
                parsed = parse_param(p, c_type_to_rust)
                if parsed:
                    params.append(f"{parsed[0]}: {parsed[1]}")
        param_str = ", ".join(params)
        ret_str = "()" if ret == VOID_RETURN else ret
        lines.append(f"pub type {rust_name} = Option<unsafe extern \"C\" fn({param_str}) -> {ret_str}>;")
        if cb.comment:
            lines.append(f"/// {cb.comment}")
        lines.append("")
    return "\n".join(lines).rstrip() + "\n"


def render_ffi(entries: list[ApiEntry], lib_name: str) -> str:
    lines = [
        "use crate::situation_types::*;",
        "use std::os::raw::{c_char, c_int, c_uint, c_void};",
        "",
        f'#[link(name = "{lib_name}")]',
        'extern "C" {',
    ]
    seen_names: set[str] = set()
    for e in sorted(entries, key=lambda x: x.name):
        if e.manual_only or e.name in MANUAL_FUNCTIONS:
            continue
        if e.name in seen_names:
            continue
        seen_names.add(e.name)
        parsed = parse_function_signature(e.signature, c_type_to_rust)
        if not parsed:
            lines.append(f"    // SKIP (unparsed): {e.name}")
            continue
        ret, name, params = parsed
        param_str = ", ".join(f"{n}: {t}" for n, t in params)
        if e.comment:
            lines.append(f"    /// {e.comment}")
        ret_str = "()" if ret == VOID_RETURN else ret
        lines.append(f"    pub fn {name}({param_str}) -> {ret_str};")
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
        if val.startswith("0x"):
            lines.append(f"pub const {d.name}: u32 = {val};")
        else:
            lines.append(f"pub const {d.name}: i32 = {val};")
    return "\n".join(lines) + "\n"


def render_helpers() -> str:
    return '''use crate::situation_ffi::*;
use crate::situation_types::SituationError;

// Helpers replacing C preprocessor macros from situation_api.h

pub fn situation_begin_frame() {
    unsafe {
        SituationPollInputEvents();
        SituationUpdateTimers();
    }
}

pub fn situation_success(err: SituationError) -> bool {
    err == SituationError::SITUATION_SUCCESS
}
'''


def render_manual_md(entries: list[ApiEntry]) -> str:
    manual = [e for e in entries if e.manual_only or e.name in MANUAL_FUNCTIONS]
    lines = [
        "# Situation Rust — Manual bindings",
        "",
        "These symbols are **not** emitted in `situation_ffi.rs`. Add hand-written Rust wrappers as needed.",
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
        "# Situation Rust bindings — API index",
        "",
        f"_Generated {datetime.now(timezone.utc).strftime('%Y-%m-%d %H:%M UTC')} from `sit/situation_api.h` — Situation **{version}** ({mode})._",
        "",
        f"**Foreign imports:** {len(foreign_entries(entries))}",
        "",
        "| Function | Section | Rust | Notes |",
        "|----------|---------|------|-------|",
    ]
    for e in sorted(entries, key=lambda x: x.name):
        if e.manual_only or e.name in MANUAL_FUNCTIONS:
            rust = "manual"
            note = e.manual_reason or "manual wrapper"
        else:
            rust = "auto"
            note = e.comment[:80] + ("..." if len(e.comment) > 80 else "")
        lines.append(f"| `{e.name}` | {e.section} | {rust} | {note or '—'} |")
    lines.append("")
    return "\n".join(lines)


def collect_referenced_types(entries: list[ApiEntry]) -> set[str]:
    referenced: set[str] = set()
    type_pattern = re.compile(r"\b([A-Z][A-Za-z0-9_]*)\b")
    for e in foreign_entries(entries):
        parsed = parse_function_signature(e.signature, c_type_to_rust)
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
        "c_int", "c_uint", "c_long", "c_ulong", "c_char", "c_void",
        "u8", "u16", "u32", "u64", "i8", "i16", "i32", "i64",
        "f32", "f64", "usize", "bool",
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
        "",
    ]
    for t in missing:
        if len(t) <= 2 or t in ("Select", "First", "String"):
            continue
        lines.append("#[repr(C)]")
        lines.append(f"pub struct {t} {{")
        lines.append("    _private: [u8; 0],")
        lines.append("}")
        lines.append("")
    return "\n".join(lines)


def render_lib_rs(version: str, jam: bool) -> str:
    ffi_mod = "situation_ffi_jam" if jam else "situation_ffi"
    return f'''//! Rust FFI for the Situation C library (auto-generated bindings).
//! Situation {version}
//!
//! Re-generate:
//!   python tools/generate_rust_bindings.py
//!
//! Requires a pre-built import library:
//!   build_situation.bat opengl  →  build/dll/situation_opengl.lib

#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![allow(dead_code)]

pub mod situation_types;
pub mod situation_callbacks;
pub mod {ffi_mod};
pub mod situation_constants;
pub mod situation_helpers;

pub use situation_types::*;
pub use situation_callbacks::*;
pub use {ffi_mod}::*;
pub use situation_constants::*;
pub use situation_helpers::*;
'''


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate Rust FFI bindings for Situation")
    parser.add_argument("--jam", action="store_true", help="Emit jam API slice only")
    parser.add_argument(
        "--lib",
        default="situation_opengl",
        help="Library name for #[link(name = ...)] (without .lib extension)",
    )
    args = parser.parse_args()

    version = read_version()
    all_entries = parse_api_header()
    jam_names = load_jam_slice(JAM_SLICE_FILE)
    entries = filter_jam(all_entries, jam_names) if args.jam else all_entries

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    (ROOT / "wrappers" / "rust").mkdir(parents=True, exist_ok=True)

    gen_header = gen_banner("generate_rust_bindings.py", version, "//") + "use std::os::raw::{c_char, c_int, c_uint, c_void};\n\n"

    (OUT_DIR / "situation_types.rs").write_text(
        gen_header + render_manual_types() + "\n" + render_enums() + render_opaque_stubs(entries),
        encoding="utf-8",
    )
    (OUT_DIR / "situation_callbacks.rs").write_text(
        gen_header + render_callbacks(), encoding="utf-8"
    )
    ffi_name = "situation_ffi_jam.rs" if args.jam else "situation_ffi.rs"
    ffi_content = render_ffi(entries, args.lib)
    (OUT_DIR / ffi_name).write_text(gen_header + ffi_content, encoding="utf-8")
    (OUT_DIR / "situation_constants.rs").write_text(
        gen_header + render_constants(), encoding="utf-8"
    )
    (OUT_DIR / "situation_helpers.rs").write_text(
        gen_header + render_helpers(), encoding="utf-8"
    )
    index_name = "API_INDEX_JAM.md" if args.jam else "API_INDEX.md"
    (OUT_DIR.parent / index_name).write_text(
        render_api_index(entries, version, args.jam), encoding="utf-8"
    )
    (OUT_DIR.parent / "MANUAL_BINDINGS.md").write_text(
        render_manual_md(all_entries), encoding="utf-8"
    )

    # lib.rs is regenerated to point at the correct ffi module (full vs jam overwrites mod name)
    if not args.jam:
        LIB_RS.write_text(render_lib_rs(version, jam=False), encoding="utf-8")

    cargo_toml = ROOT / "wrappers" / "rust" / "Cargo.toml"
    if not cargo_toml.exists():
        cargo_toml.write_text(
            '''[package]
name = "situation"
version = "0.1.0"
edition = "2021"
description = "Rust FFI bindings for the Situation library"
license = "MIT"

[lib]
name = "situation"
path = "src/lib.rs"

[[example]]
name = "hello_situation"
path = "examples/hello_situation.rs"
''',
            encoding="utf-8",
        )

    build_rs = ROOT / "wrappers" / "rust" / "build.rs"
    build_rs.write_text(
        f'''fn main() {{
    println!("cargo:rustc-link-search=native=../../build/dll");
    println!("cargo:rustc-link-lib={args.lib}");
}}
''',
        encoding="utf-8",
    )

    auto = len(foreign_entries(entries))
    print(f"Situation {version}")
    print(f"Rust bindings: {auto} extern fn ({'jam' if args.jam else 'full'}), {len(all_entries)} total SITAPI")
    print(f"Wrote {OUT_DIR.relative_to(ROOT)}/{ffi_name}")
    print(f"Wrote wrappers/rust/{index_name}")


if __name__ == "__main__":
    main()
