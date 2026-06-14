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

OUT_DIR = ROOT / "wrappers" / "rust" / "src"
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
    "mat4": "Mat4",
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

// cglm mat4 equivalent (repr(C) struct — raw [[f32; 4]; 4] is not FFI-safe in extern blocks)
#[repr(C)]
#[derive(Copy, Clone, Debug, Default)]
pub struct Mat4(pub [[f32; 4]; 4]);

#[repr(C)]
#[derive(Copy, Clone, Debug, Default)]
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

// --- Types referenced by the API that require explicit layout ---
// These are not auto-generated because the parser cannot always infer their layout.

#[repr(C)]
#[derive(Copy, Clone, Debug, Default)]
pub struct ColorYPQA {
    pub y: u8,
    pub p: u8,
    pub q: u8,
    pub a: u8,
}

#[repr(C)]
#[derive(Copy, Clone, Debug, Default)]
pub struct ColorYPQf {
    pub y: f32,
    pub p: f32,
    pub q: f32,
    pub a: f32,
}

#[repr(C)]
#[derive(Copy, Clone, Debug, Default)]
pub struct ColorHSV {
    pub h: f32,
    pub s: f32,
    pub v: f32,
}

#[repr(C)]
#[derive(Copy, Clone, Debug, Default)]
pub struct SituationImage {
    pub data: *mut c_void,
    pub width: c_int,
    pub height: c_int,
    pub channels: c_int,
    pub color_encoding: c_int,
}

#[repr(C)]
#[derive(Copy, Clone, Debug, Default)]
pub struct SituationMesh {
    pub slot_index: u32,
    pub generation: u32,
    pub index_count: c_int,
    pub vertex_count: c_int,
    pub vertex_stride: usize,
}

#[repr(C)]
#[derive(Copy, Clone, Debug, Default)]
pub struct SituationTextureRect {
    pub x: c_int,
    pub y: c_int,
    pub width: c_int,
    pub height: c_int,
}

#[repr(C)]
#[derive(Copy, Clone, Debug, Default)]
pub struct SituationTextureCopyRegion {
    pub src_rect: SituationTextureRect,
    pub dst_x: c_int,
    pub dst_y: c_int,
    pub src_mip_level: u32,
    pub dst_mip_level: u32,
    pub src_array_layer: u32,
    pub dst_array_layer: u32,
}

#[repr(C)]
#[derive(Copy, Clone, Debug, Default)]
pub struct SituationClearValue {
    pub color: ColorRGBA,
    pub depth: f32,
    pub stencil: u32,
}

#[repr(C)]
#[derive(Copy, Clone, Debug, Default)]
pub struct SituationPipelineBarrierDesc {
    pub src_stages: u32,
    pub src_access: u32,
    pub dst_stages: u32,
    pub dst_access: u32,
}

#[repr(C)]
#[derive(Copy, Clone, Debug, Default)]
pub struct SituationBufferBarrierDesc {
    pub buffer: SituationBuffer,
    pub offset: usize,
    pub size: usize,
    pub src_stages: u32,
    pub src_access: u32,
    pub dst_stages: u32,
    pub dst_access: u32,
}

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub struct SituationTextureBarrierDesc {
    pub old_layout: c_int,
    pub new_layout: c_int,
    pub base_mip_level: u32,
    pub mip_level_count: u32,
    pub base_array_layer: u32,
    pub array_layer_count: u32,
}

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub struct SituationStencilState {
    pub compare_op: c_int,
    pub fail_op: c_int,
    pub depth_fail_op: c_int,
    pub pass_op: c_int,
    pub compare_mask: u32,
    pub write_mask: u32,
    pub reference: u32,
}

#[repr(C)]
#[derive(Copy, Clone, Debug, Default)]
pub struct SituationMultisampleState {
    pub sample_shading_enable: bool,
    pub min_sample_shading: f32,
    pub sample_mask: u32,
    pub alpha_to_coverage_enable: bool,
}

// Opaque types — layout not exposed in public API, passed by pointer only.
#[repr(C)] pub struct SituationModel { _private: [u8; 0], }
#[repr(C)] pub struct SituationAudioDeviceInfo { _private: [u8; 0], }
#[repr(C)] pub struct SituationDisplayInfo { _private: [u8; 0], }
#[repr(C)] pub struct SituationDisplayMode { _private: [u8; 0], }
#[repr(C)] pub struct SituationProcessInfo { _private: [u8; 0], }
#[repr(C)] pub struct SituationCpuTopology { _private: [u8; 0], }
#[repr(C)] pub struct SituationCPUInfo { _private: [u8; 0], }
#[repr(C)] pub struct SituationDeviceInfo { _private: [u8; 0], }
#[repr(C)] pub struct SituationDeviceMetadata { _private: [u8; 0], }
#[repr(C)] pub struct SituationDeviceFunctions { _private: [u8; 0], }
#[repr(C)] pub struct SituationAudioFormat { _private: [u8; 0], }
#[repr(C)] pub struct SituationGPUInfo { _private: [u8; 0], }
#[repr(C)] pub struct SituationGraphicsCaps { _private: [u8; 0], }
#[repr(C)] pub struct SituationMemoryInfo { _private: [u8; 0], }
#[repr(C)] pub struct SituationNumaTopology { _private: [u8; 0], }
#[repr(C)] pub struct SituationOSInfo { _private: [u8; 0], }
#[repr(C)] pub struct SituationTextureInfo { _private: [u8; 0], }
#[repr(C)] pub struct GLFWwindow { _private: [u8; 0], }
#[repr(C)] pub struct VkRenderPass { _private: [u8; 0], }
#[repr(C)] pub struct SituationMidiDeviceInfo { _private: [u8; 0], }
#[repr(C)] pub struct SituationReadPixelsDesc { _private: [u8; 0], }
#[repr(C)] pub struct SituationRenderList { _private: [u8; 0], }
#[repr(C)] pub struct SituationShaderStorageBlockInfo { _private: [u8; 0], }
#[repr(C)] pub struct SituationSoundHandle { _private: [u8; 0], }
#[repr(C)] pub struct SituationTextureReadbackDesc { _private: [u8; 0], }
#[repr(C)] pub struct SituationThreadingStatus { _private: [u8; 0], }
#[repr(C)] pub struct SituationThreadPoolMetrics { _private: [u8; 0], }
#[repr(C)] pub struct SituationThreadPoolSnapshot { _private: [u8; 0], }
#[repr(C)] pub struct SituationUniformExpectation { _private: [u8; 0], }
#[repr(C)] pub struct SituationVirtualDisplay { _private: [u8; 0], }
#[repr(C)] pub struct SituationYpqRgbMappingStats { _private: [u8; 0], }
#[repr(C)] pub struct VkDevice { _private: [u8; 0], }
#[repr(C)] pub struct VkInstance { _private: [u8; 0], }
#[repr(C)] pub struct VkPhysicalDevice { _private: [u8; 0], }
// Scalar typedef aliases from situation_base_types.h
pub type SituationModifiers = c_int;
pub type SituationStreamSize = u64;
pub type SituationStreamResult = c_int;
#[repr(i32)]
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum SituationSeekOrigin {
    SIT_SEEK_FROM_START = 0,
    SIT_SEEK_FROM_CURRENT = 1,
    SIT_SEEK_FROM_END = 2,
}
// FILE is the C standard library file handle — map to opaque pointer.
pub type FILE = c_void;
'''


def escape_rust_keyword(name: str) -> str:
    keywords = {
        "as", "break", "const", "continue", "crate", "else", "enum", "extern",
        "false", "fn", "for", "if", "impl", "in", "let", "loop", "match", "mod",
        "move", "mut", "pub", "ref", "return", "self", "Self", "static", "struct",
        "super", "trait", "true", "type", "unsafe", "use", "where", "while",
        "async", "await", "dyn", "try"
    }
    if name in keywords:
        return f"r#{name}"
    return name


def to_snake_case(name: str) -> str:
    s1 = re.sub('(.)([A-Z][a-z]+)', r'\1_\2', name)
    s2 = re.sub('([a-z0-9])([A-Z])', r'\1_\2', s1).lower()
    # Clean up some common abbreviations
    s2 = s2.replace("v_sync", "vsync")
    s2 = s2.replace("f_p_s", "fps")
    s2 = s2.replace("m_i_d_i", "midi")
    s2 = s2.replace("p_c_m", "pcm")
    s2 = s2.replace("g_p_u", "gpu")
    s2 = s2.replace("f_b_o", "fbo")
    s2 = s2.replace("s_p_i_r_v", "spirv")
    s2 = s2.replace("g_l_f_w", "glfw")
    s2 = s2.replace("d_x_g_i", "dxgi")
    s2 = s2.replace("c_o_m", "com")
    s2 = s2.replace("i_o", "io")
    s2 = s2.replace("v_d_flags", "vd_flags")
    s2 = s2.replace("v_d", "vd")
    s2 = s2.replace("s_s_b_o", "ssbo")
    s2 = s2.replace("u_b_o", "ubo")
    return s2


def resolve_rust_enum_value(value: str, define_map: dict[str, str]) -> str:
    resolved = value.strip()
    # Recursively resolve any sub-tokens in expressions that exist in define_map
    def replace_word(match: re.Match) -> str:
        word = match.group(0)
        # Avoid infinite loops by not re-resolving the exact same word
        val = define_map.get(word, word)
        if val != word:
            return resolve_rust_enum_value(val, define_map)
        return val
    return re.sub(r"\b\w+\b", replace_word, resolved)


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
            resolved = resolve_rust_enum_value(value, define_map)
            lines.append(f"    {name} = {resolved},")
    lines.append("}")
    return "\n".join(lines)


def render_enums() -> str:
    define_map = build_define_map()
    for e in parse_enums():
        for name, value in e.members:
            if value is not None:
                define_map[name] = value.strip()
    
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
    api_text = (ROOT / "sit" / "situation_api.h").read_text(encoding="utf-8", errors="replace")
    base_text = (ROOT / "sit" / "situation_base_callbacks.h").read_text(encoding="utf-8", errors="replace")
    combined_text = api_text + "\n" + base_text
    lines = [
        "use crate::situation_types::*;",
        "",
        "// Callback typedefs from sit/situation_api.h and sit/situation_base_callbacks.h",
        "",
    ]
    seen: set[str] = set()
    for cb in parse_callbacks(combined_text):
        rust_name = cb.name if cb.name.endswith("Callback") else cb.name + "Callback"
        if rust_name in seen:
            continue
        seen.add(rust_name)
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
                    params.append(f"{escape_rust_keyword(parsed[0])}: {parsed[1]}")
        param_str = ", ".join(params)
        ret_str = "()" if ret == VOID_RETURN else ret
        lines.append(f"pub type {rust_name} = Option<unsafe extern \"C\" fn({param_str}) -> {ret_str}>;")
        if cb.comment:
            lines.append(f"/// {cb.comment}")
        lines.append("")
    return "\n".join(lines).rstrip() + "\n"


def render_ffi(entries: list[ApiEntry], lib_name: str) -> str:
    _ = lib_name  # Linking is configured in wrappers/Rust/build.rs (DLL + static modes).
    lines = [
        "use crate::situation_types::*;",
        "use crate::situation_callbacks::*;",
        "",
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
        param_str = ", ".join(f"{escape_rust_keyword(n)}: {t}" for n, t in params)
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
        val = d.value.split("//")[0].strip()
        if val.endswith("f"):
            val = val[:-1]
        if val.startswith("0x"):
            lines.append(f"pub const {d.name}: u32 = {val};")
        else:
            lines.append(f"pub const {d.name}: i32 = {val};")
    return "\n".join(lines) + "\n"


def render_helpers(entries: list[ApiEntry]) -> str:
    lines = [
        "use crate::situation_ffi::*;",
        "use crate::situation_types::*;",
        "use crate::situation_callbacks::*;",
        "use std::ffi::CStr;",
        "",
        "// Helpers replacing C preprocessor macros & providing safe idiomatic wrappers",
        "",
    ]
    exclude_list = {
        "SituationGetVulkanDevice",
        "SituationGetVulkanInstance",
        "SituationLoadComputeShader",
        "SituationLoadModelFromSTL",
        "SituationGetVulkanPhysicalDevice",
        "SituationGetMainWindowRenderPass",
        "SituationLoadComputeShaderFromMemory",
    }
    seen_names = set()
    for e in sorted(entries, key=lambda x: x.name):
        if e.manual_only or e.name in MANUAL_FUNCTIONS or e.name in exclude_list or "Vulkan" in e.name or "ComputeShader" in e.name:
            continue
        if e.name in seen_names:
            continue
        seen_names.add(e.name)
        
        parsed = parse_function_signature(e.signature, c_type_to_rust)
        if not parsed:
            continue
            
        ret, name, params = parsed
        
        # Determine snake_case name
        safe_name = to_snake_case(name)
        
        # Prepare parameters
        safe_params = []
        call_args = []
        opaque_types = {
            "SituationAudioGraph",
            "SituationThreadPool",
            "SituationNode",
            "SituationCommandBuffer",
            "SituationCameraDesc",
            "SituationRenderPassInfo",
            "SituationMultisampleState"
        }
        for pname, ptype in params:
            pname_escaped = escape_rust_keyword(pname)
            is_opaque = False
            for ot in opaque_types:
                if ptype.endswith(ot):
                    is_opaque = True
                    break
            # Map types
            if ptype == "*const c_char":
                safe_params.append(f"{pname_escaped}: &CStr")
                call_args.append(f"{pname_escaped}.as_ptr()")
            elif ptype.startswith("*mut ") and not ptype.endswith("c_void") and not is_opaque:
                inner = ptype[5:]
                safe_params.append(f"{pname_escaped}: &mut {inner}")
                call_args.append(pname_escaped)
            elif ptype.startswith("*const ") and not ptype.endswith("c_char") and not ptype.endswith("*const c_char") and not is_opaque:
                inner = ptype[7:]
                safe_params.append(f"{pname_escaped}: &{inner}")
                call_args.append(pname_escaped)
            else:
                safe_params.append(f"{pname_escaped}: {ptype}")
                call_args.append(pname_escaped)
                
        param_str = ", ".join(safe_params)
        arg_str = ", ".join(call_args)
        
        # Check return type
        is_ptr = ret.startswith("*") or ret in ("SituationCommandBuffer", "SituationAudioGraph", "SituationThreadPool", "SituationNode")
        
        body = ""
        ret_type_str = ""
        
        if ret == "SituationError":
            ret_type_str = " -> Result<(), SituationError>"
            body = f"""    let err = unsafe {{ {name}({arg_str}) }};
    if err == SituationError::SITUATION_SUCCESS {{
        Ok(())
    }} else {{
        Err(err)
    }}"""
        elif is_ptr:
            ret_type_str = f" -> Option<{ret}>"
            body = f"""    let ptr = unsafe {{ {name}({arg_str}) }};
    if ptr.is_null() {{
        None
    }} else {{
        Some(ptr)
    }}"""
        elif ret == VOID_RETURN or ret == "()":
            ret_type_str = ""
            body = f"    unsafe {{ {name}({arg_str}) }};"
        else:
            ret_type_str = f" -> {ret}"
            body = f"    unsafe {{ {name}({arg_str}) }}"
            
        # Add comment if exists
        if e.comment:
            lines.append(f"/// {e.comment}")
        lines.append(f"pub fn {safe_name}({param_str}){ret_type_str} {{")
        lines.append(body)
        lines.append("}")
        lines.append("")
        
    # Additional compatibility/legacy helpers
    lines.append("pub fn situation_begin_frame() {")
    lines.append("    situation_poll_input_events();")
    lines.append("    situation_update_timers();")
    lines.append("}")
    lines.append("")
    lines.append("pub fn situation_success(err: SituationError) -> bool {")
    lines.append("    err == SituationError::SITUATION_SUCCESS")
    lines.append("}")
    
    return "\n".join(lines)


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


def render_api_index(entries: list[ApiEntry], version: str) -> str:
    lines = [
        "# Situation Rust bindings — API index",
        "",
        f"_Generated {datetime.now(timezone.utc).strftime('%Y-%m-%d %H:%M UTC')} from `sit/situation_api.h` — Situation **{version}**._",
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
        # Types now defined in render_manual_types — prevent opaque stub duplication:
        "ColorYPQA", "ColorYPQf", "ColorHSV",
        "SituationImage", "SituationMesh",
        "SituationTextureRect", "SituationTextureCopyRegion",
        "SituationClearValue", "SituationPipelineBarrierDesc",
        "SituationBufferBarrierDesc", "SituationTextureBarrierDesc",
        "SituationStencilState", "SituationMultisampleState",
        "SituationModel", "SituationAudioDeviceInfo", "SituationDisplayInfo",
        "SituationDisplayMode", "SituationProcessInfo", "SituationCpuTopology",
        "SituationCPUInfo", "SituationDeviceInfo", "SituationDeviceMetadata",
        "SituationDeviceFunctions", "SituationAudioFormat",
        "GLFWwindow", "FILE",
        # Additional opaque types emitted in render_manual_types:
        "SituationGPUInfo", "SituationGraphicsCaps", "SituationMemoryInfo",
        "SituationNumaTopology", "SituationOSInfo", "SituationTextureInfo",
        "VkRenderPass", "SituationMidiDeviceInfo", "SituationReadPixelsDesc",
        "SituationRenderList", "SituationShaderStorageBlockInfo", "SituationSoundHandle",
        "SituationTextureReadbackDesc", "SituationThreadingStatus",
        "SituationThreadPoolMetrics", "SituationThreadPoolSnapshot",
        "SituationUniformExpectation", "SituationVirtualDisplay",
        "SituationYpqRgbMappingStats", "VkDevice", "VkInstance", "VkPhysicalDevice",
        # Callbacks defined in situation_callbacks.rs (from base_callbacks.h):
        "SituationAudioProcessorCallback", "SituationAudioCaptureCallback",
        "SituationCursorPosCallback", "SituationFileDropCallback",
        "SituationFileLoadCallback", "SituationFileSaveCallback",
        "SituationFileTextLoadCallback", "SituationFocusCallback",
        "SituationJoystickCallback", "SituationKeyCallback",
        "SituationMaximizeCallback", "SituationMouseButtonCallback",
        "SituationScrollCallback", "SituationStreamReadCallback",
        "SituationStreamSeekCallback",
        # Simple typedef aliases from situation_base_types.h:
        "SituationModifiers", "SituationStreamSize", "SituationSeekOrigin",
        "SituationStreamResult",
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


RAW_IMPORT_TYPES = ("c_char", "c_int", "c_uint", "c_void", "c_long", "c_ulong")


def render_file_header(version: str, body: str) -> str:
    banner = gen_banner("generate_rust_bindings.py", version, "//")
    used = [t for t in RAW_IMPORT_TYPES if re.search(rf"\b{t}\b", body)]
    if not used:
        return banner + "\n"
    return banner + f"use std::os::raw::{{{', '.join(used)}}};\n\n"


def write_generated(path: Path, version: str, body: str) -> None:
    path.write_text(render_file_header(version, body) + body, encoding="utf-8")


def render_lib_rs(version: str) -> str:
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
pub mod situation_ffi;
pub mod situation_constants;
pub mod situation_helpers;

pub use situation_types::*;
pub use situation_callbacks::*;
pub use situation_ffi::*;
pub use situation_constants::*;
pub use situation_helpers::*;
'''


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate Rust FFI bindings for Situation")
    parser.add_argument(
        "--lib",
        default="situation_opengl",
        help="Library name for #[link(name = ...)] (without .lib extension)",
    )
    args = parser.parse_args()

    version = read_version()
    entries = parse_api_header()

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    (ROOT / "wrappers" / "rust").mkdir(parents=True, exist_ok=True)

    types_body = render_manual_types() + "\n" + render_enums() + render_opaque_stubs(entries)
    write_generated(OUT_DIR / "situation_types.rs", version, types_body)
    write_generated(OUT_DIR / "situation_callbacks.rs", version, render_callbacks())
    write_generated(OUT_DIR / "situation_constants.rs", version, render_constants())
    write_generated(OUT_DIR / "situation_ffi.rs", version, render_ffi(entries, args.lib))
    write_generated(OUT_DIR / "situation_helpers.rs", version, render_helpers(entries))
    (OUT_DIR.parent / "API_INDEX.md").write_text(
        render_api_index(entries, version), encoding="utf-8"
    )
    (OUT_DIR.parent / "MANUAL_BINDINGS.md").write_text(
        render_manual_md(entries), encoding="utf-8"
    )

    LIB_RS.write_text(render_lib_rs(version), encoding="utf-8")

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

    # build.rs handles DLL + static link modes — do not overwrite on regenerate.

    auto = len(foreign_entries(entries))
    print(f"Situation {version}")
    print(f"Rust bindings: {auto} extern fn, {len(entries)} total SITAPI")
    print(f"Wrote {OUT_DIR.relative_to(ROOT)}/situation_ffi.rs")
    print(f"Wrote wrappers/rust/API_INDEX.md")


if __name__ == "__main__":
    main()
