#!/usr/bin/env python3
"""
SPIR-V + GLSL shader debug helper for Situation / Demon Hunt.

Offline analysis after glslc compile — catches SPIR-V bloat and maps hot
functions before a 2+ minute GPU link attempt.

Usage (repo root):
  python scripts/spirv_shader_debug.py report examples/demon_hunt_sky.fs.spv
  python scripts/spirv_shader_debug.py glsl examples/demon_hunt_sky.fs
  python scripts/spirv_shader_debug.py demon_hunt
  python scripts/spirv_shader_debug.py compare before.fs.spv after.fs.spv
"""
from __future__ import annotations

import argparse
import re
import struct
import sys
from collections import Counter
from dataclasses import dataclass, field
from pathlib import Path

# --- SPIR-V constants (subset) ------------------------------------------------

SPIR_V_MAGIC = 0x07230203

OP_NAME = 5
OP_DECORATE = 71
OP_VARIABLE = 59
OP_FUNCTION = 54
OP_FUNCTION_END = 56
OP_ENTRY_POINT = 15
OP_EXECUTION_MODE = 16
OP_TYPE_FUNCTION = 33
OP_LABEL = 248

DEC_BINDING = 33
DEC_DESCRIPTOR_SET = 34
DEC_BLOCK = 2
DEC_BUFFER_BLOCK = 3

SC_UNIFORM = 2
SC_STORAGE = 12

# NVIDIA OpenGL SPIR-V link often fails when driver IR exceeds ~65536 instructions.
NVIDIA_ASM_INSTR_WARN = 65536
SPIR_V_INSTR_WARN = 12000
SPIR_V_BYTES_WARN = 512 * 1024

OPCODE_NAMES: dict[int, str] = {
    0: "OpNop",
    5: "OpName",
    15: "OpEntryPoint",
    16: "OpExecutionMode",
    33: "OpTypeFunction",
    54: "OpFunction",
    56: "OpFunctionEnd",
    59: "OpVariable",
    71: "OpDecorate",
    248: "OpLabel",
    249: "OpBranch",
    250: "OpBranchConditional",
    251: "OpSwitch",
    252: "OpKill",
    253: "OpReturn",
    254: "OpReturnValue",
    255: "OpUnreachable",
}


@dataclass
class SpirvModuleReport:
    path: Path
    bytes_size: int = 0
    version: tuple[int, int, int] = (0, 0, 0)
    generator: int = 0
    bound: int = 0
    schema: int = 0
    total_instructions: int = 0
    opcode_counts: Counter = field(default_factory=Counter)
    entry_points: list[str] = field(default_factory=list)
    resources: list[str] = field(default_factory=list)
    functions: list[tuple[str, int, int]] = field(default_factory=list)  # name, insn, labels
    warnings: list[str] = field(default_factory=list)


def parse_spirv_words(data: bytes) -> list[tuple[int, tuple[int, ...]]]:
    if len(data) < 20:
        raise ValueError(f"SPIR-V too short ({len(data)} bytes)")
    magic, version, generator, bound, schema = struct.unpack_from("<5I", data, 0)
    if magic != SPIR_V_MAGIC:
        raise ValueError("not SPIR-V (bad magic)")
    off = 20
    insns: list[tuple[int, tuple[int, ...]]] = []
    while off < len(data):
        w0 = struct.unpack_from("<I", data, off)[0]
        cnt = (w0 >> 16) & 0xFFFF
        op = w0 & 0xFFFF
        if cnt == 0:
            raise ValueError(f"invalid instruction word count at offset {off}")
        need = 4 * cnt
        if off + need > len(data):
            raise ValueError(f"truncated SPIR-V at offset {off}")
        words = struct.unpack_from("<" + "I" * cnt, data, off)
        insns.append((op, words))
        off += need
    return insns


def decode_spirv_string(words: tuple[int, ...], start: int) -> str:
    chars: list[str] = []
    for i in range(start, len(words)):
        w = words[i]
        for shift in (0, 8, 16, 24):
            c = (w >> shift) & 0xFF
            if c == 0:
                return "".join(chars)
            chars.append(chr(c))
    return "".join(chars)


def clean_spirv_fn_name(name: str) -> str:
    p = name.find("(")
    return name[:p] if p > 0 else name


def analyze_spirv(data: bytes, path: Path) -> SpirvModuleReport:
    rep = SpirvModuleReport(path=path, bytes_size=len(data))
    _, ver, gen, bound, schema = struct.unpack_from("<5I", data, 0)
    rep.version = ((ver >> 16) & 0xFF, (ver >> 8) & 0xFF, ver & 0xFF)
    rep.generator = gen
    rep.bound = bound
    rep.schema = schema

    insns = parse_spirv_words(data)
    rep.total_instructions = len(insns)
    for op, _ in insns:
        rep.opcode_counts[op] += 1

    id_names: dict[int, str] = {}
    decs: dict[int, dict] = {}
    var_to_type: dict[int, int] = {}
    type_to_name: dict[int, str] = {}
    func_ids: dict[int, str] = {}

    for op, words in insns:
        if op == OP_NAME and len(words) >= 3:
            id_names[words[1]] = decode_spirv_string(words, 2)
            type_to_name[words[1]] = id_names[words[1]]
        elif op == OP_DECORATE and len(words) >= 3:
            target, decoration = words[1], words[2]
            d = decs.setdefault(target, {})
            if decoration == DEC_BINDING and len(words) >= 4:
                d["binding"] = words[3]
            elif decoration == DEC_DESCRIPTOR_SET and len(words) >= 4:
                d["set"] = words[3]
            elif decoration in (DEC_BLOCK, DEC_BUFFER_BLOCK):
                d["block_kind"] = "BufferBlock" if decoration == DEC_BUFFER_BLOCK else "Block"
        elif op == OP_VARIABLE and len(words) >= 4:
            var_to_type[words[2]] = words[1]
        elif op == OP_ENTRY_POINT and len(words) >= 4:
            name = decode_spirv_string(words, 3)
            rep.entry_points.append(name)
            func_ids[words[2]] = name
        elif op == OP_FUNCTION and len(words) >= 3:
            func_ids.setdefault(words[2], id_names.get(words[2], f"fn_{words[2]:#x}"))

    for op, words in insns:
        if op == OP_VARIABLE and len(words) >= 4:
            _typ, var_id, storage = words[1], words[2], words[3]
            d = decs.get(var_id, {})
            if "set" not in d and "binding" not in d:
                continue
            type_id = var_to_type.get(var_id)
            name = type_to_name.get(type_id, id_names.get(type_id, f"var_{var_id:#x}"))
            sc = {SC_UNIFORM: "Uniform", SC_STORAGE: "StorageBuffer"}.get(storage, f"sc={storage}")
            rep.resources.append(
                f"{name}: {sc} set={d.get('set')} binding={d.get('binding')}"
            )

    # Per-function instruction spans
    current_fn: int | None = None
    current_name = "<module>"
    fn_insn = 0
    fn_labels = 0

    def flush_fn() -> None:
        nonlocal fn_insn, fn_labels
        if current_fn is not None:
            rep.functions.append((clean_spirv_fn_name(current_name), fn_insn, fn_labels))
        fn_insn = 0
        fn_labels = 0

    for op, words in insns:
        if op == OP_FUNCTION and len(words) >= 3:
            flush_fn()
            current_fn = words[2]
            current_name = func_ids.get(current_fn, id_names.get(current_fn, f"fn_{current_fn:#x}"))
            fn_insn = 1
            fn_labels = 0
            continue
        if op == OP_FUNCTION_END:
            flush_fn()
            current_fn = None
            current_name = "<module>"
            continue
        if current_fn is not None:
            fn_insn += 1
            if op == OP_LABEL:
                fn_labels += 1

    if rep.bytes_size >= SPIR_V_BYTES_WARN:
        rep.warnings.append(
            f"SPIR-V blob {rep.bytes_size} bytes >= {SPIR_V_BYTES_WARN} — NVIDIA link may OOM or hit instruction limits"
        )
    if rep.total_instructions >= SPIR_V_INSTR_WARN:
        rep.warnings.append(
            f"SPIR-V module {rep.total_instructions} instructions >= {SPIR_V_INSTR_WARN} — expect heavy driver lowering"
        )
    rep.warnings.append(
        f"NVIDIA OpenGL SPIR-V link often fails near ~{NVIDIA_ASM_INSTR_WARN} driver instructions "
        f"(glGetProgramInfoLog: too many instructions)"
    )

    rep.functions.sort(key=lambda x: x[1], reverse=True)
    return rep


@dataclass
class GlslReport:
    path: Path
    lines: int = 0
    defines: list[str] = field(default_factory=list)
    functions: list[tuple[str, int, int]] = field(default_factory=list)  # name, start, body_lines


FUNC_HEAD_RE = re.compile(
    r"^(?:"
    r"(?:void|float|double|bool|int|uint|"
    r"vec[234]|ivec[234]|uvec[234]|bvec[234]|mat[234]|"
    r"Dh[A-Za-z0-9_]+|Sprite[A-Za-z0-9_]+)"
    r")\s+([A-Za-z_][A-Za-z0-9_]*)\s*\("
)


def analyze_glsl(path: Path) -> GlslReport:
    text = path.read_text(encoding="utf-8", errors="replace")
    lines = text.splitlines()
    rep = GlslReport(path=path, lines=len(lines))

    for line in lines:
        s = line.strip()
        if s.startswith("#define "):
            rep.defines.append(s)

    depth = 0
    in_fn = False
    fn_name = ""
    fn_start = 0
    fn_body = 0
    saw_open = False

    for i, line in enumerate(lines, start=1):
        stripped = line.strip()
        if not in_fn:
            m = FUNC_HEAD_RE.match(stripped)
            if m and not stripped.startswith("//"):
                in_fn = True
                fn_name = m.group(1)
                fn_start = i
                fn_body = 0
                depth = 0
                saw_open = False
        if in_fn:
            fn_body += 1
            if "{" in line:
                saw_open = True
            depth += line.count("{") - line.count("}")
            if saw_open and depth <= 0:
                rep.functions.append((fn_name, fn_start, fn_body))
                in_fn = False

    rep.functions.sort(key=lambda x: x[2], reverse=True)
    return rep


def print_spirv_report(rep: SpirvModuleReport) -> None:
    print(f"=== SPIR-V: {rep.path} ===")
    print(f"  size:        {rep.bytes_size:,} bytes")
    print(f"  version:     {rep.version[0]}.{rep.version[1]}.{rep.version[2]}")
    print(f"  bound:       {rep.bound} IDs")
    print(f"  generator:   {rep.generator:#010x}")
    print(f"  instructions:{rep.total_instructions:,}")
    print(f"  functions:   {rep.opcode_counts.get(OP_FUNCTION, 0)}")
    print(f"  entry:       {', '.join(rep.entry_points) or '(none)'}")
    if rep.resources:
        print("  resources:")
        for r in rep.resources:
            print(f"    {r}")
    print("  top functions (SPIR-V instructions, basic blocks):")
    for name, insn, labels in rep.functions[:20]:
        print(f"    {insn:6d} insns  {labels:4d} labels  {name}")
    if len(rep.functions) > 20:
        print(f"    ... {len(rep.functions) - 20} more")
    top_ops = rep.opcode_counts.most_common(12)
    print("  top opcodes:")
    for op, count in top_ops:
        label = OPCODE_NAMES.get(op, f"Op{op}")
        print(f"    {label:24s} {count:6d}")
    for w in rep.warnings:
        print(f"  WARN: {w}")


def print_glsl_report(rep: GlslReport) -> None:
    print(f"=== GLSL: {rep.path.name} ({rep.lines} lines) ===")
    if rep.defines:
        print("  recovery toggles / defines:")
        for d in rep.defines:
            if any(k in d for k in ("DH_ENABLE", "ENABLE_", "DH_MAX_", "DH_SPRITE")):
                print(f"    {d}")
    print("  top functions (source lines incl. signature):")
    for name, start, body in rep.functions[:25]:
        print(f"    {body:4d} lines  L{start:5d}  {name}")
    if len(rep.functions) > 25:
        print(f"    ... {len(rep.functions) - 25} more")


def compare_spirv(a: SpirvModuleReport, b: SpirvModuleReport) -> None:
    print(f"=== compare: {a.path.name} -> {b.path.name} ===")
    print(f"  bytes:  {a.bytes_size:,} -> {b.bytes_size:,}  ({b.bytes_size - a.bytes_size:+,})")
    print(f"  insns:  {a.total_instructions:,} -> {b.total_instructions:,}  ({b.total_instructions - a.total_instructions:+,})")
    amap = {n: (i, l) for n, i, l in a.functions}
    bmap = {n: (i, l) for n, i, l in b.functions}
    deltas: list[tuple[int, str, int, int]] = []
    for name in set(amap) | set(bmap):
        ai, al = amap.get(name, (0, 0))
        bi, bl = bmap.get(name, (0, 0))
        deltas.append((bi - ai, name, ai, bi))
    deltas.sort(reverse=True)
    print("  largest SPIR-V instruction deltas:")
    for delta, name, ai, bi in deltas[:15]:
        if delta == 0:
            continue
        print(f"    {delta:+6d}  {name}  ({ai} -> {bi})")


def cmd_report(paths: list[Path]) -> int:
    rc = 0
    for p in paths:
        if not p.is_file():
            print(f"MISSING {p}", file=sys.stderr)
            rc = 1
            continue
        try:
            rep = analyze_spirv(p.read_bytes(), p)
            print_spirv_report(rep)
            print()
        except ValueError as exc:
            print(f"ERROR {p}: {exc}", file=sys.stderr)
            rc = 1
    return rc


def cmd_glsl(paths: list[Path]) -> int:
    rc = 0
    for p in paths:
        if not p.is_file():
            print(f"MISSING {p}", file=sys.stderr)
            rc = 1
            continue
        rep = analyze_glsl(p)
        print_glsl_report(rep)
        print()
    return rc


def run_glslc_noopt(fs: Path, out_spv: Path, root: Path) -> bool:
    glslc = root / "ext" / "shaderc" / "build" / "glslc" / "glslc.exe"
    if not glslc.is_file():
        print(f"SKIP devel SPIR-V map: glslc not found at {glslc}", file=sys.stderr)
        return False
    out_spv.parent.mkdir(parents=True, exist_ok=True)
    import subprocess

    flags = [
        "-fshader-stage=fragment",
        str(fs),
        "-o",
        str(out_spv),
        "--target-env=opengl",
        "-fauto-map-locations",
        "-fauto-bind-uniforms",
        "-std=450",
    ]
    proc = subprocess.run([str(glslc), *flags], capture_output=True, text=True)
    if proc.returncode != 0:
        print(proc.stderr or proc.stdout, file=sys.stderr)
        return False
    return True


def cmd_harness(root: Path) -> int:
    out_dir = root / "tests" / "harness" / "spirv_out"
    print("Harness SPIR-V (OpenGL + Vulkan targets)")
    print("Run after: compile_harness_shaders.bat")
    print()
    names = [
        ("OpenGL", "harness_passthrough.vs.spv"),
        ("OpenGL", "harness_dual_ssbo_gl.fs.spv"),
        ("OpenGL", "harness_ubo_ssbo_gl.fs.spv"),
        ("Vulkan", "harness_passthrough_vk.vs.spv"),
        ("Vulkan", "harness_dual_ssbo_vk.fs.spv"),
        ("Vulkan", "harness_ubo_ssbo_vk.fs.spv"),
    ]
    rc = 0
    for backend, name in names:
        p = out_dir / name
        if not p.is_file():
            print(f"MISSING [{backend}] {p}", file=sys.stderr)
            rc = 1
            continue
        print(f"--- {backend} / {name} ---")
        print_spirv_report(analyze_spirv(p.read_bytes(), p))
        print()
    return rc


def cmd_demon_hunt(root: Path, devel: bool) -> int:
    vs = root / "examples" / "demon_hunt_sky.vs"
    fs = root / "examples" / "demon_hunt_sky.fs"
    vs_spv = root / "examples" / "demon_hunt_sky.vs.spv"
    fs_spv = root / "examples" / "demon_hunt_sky.fs.spv"
    if not fs_spv.is_file():
        fs_spv = root / "examples" / "demon_hunt_sky.fs.spv"
    if not vs_spv.is_file():
        vs_spv = root / "build" / "examples" / "demon_hunt_sky.vs.spv"
    if not fs_spv.is_file():
        fs_spv = root / "build" / "examples" / "demon_hunt_sky.fs.spv"

    print("Demon Hunt skydome shader debug (OpenGL-target SPIR-V)")
    print("Run after: compile_demon_hunt_shaders.bat")
    print()
    rc = 0
    if fs.is_file():
        print_glsl_report(analyze_glsl(fs))
        print()
    else:
        print(f"MISSING {fs}", file=sys.stderr)
        rc = 1
    for spv in (vs_spv, fs_spv):
        if spv.is_file():
            rep = analyze_spirv(spv.read_bytes(), spv)
            print_spirv_report(rep)
            if "fs" in spv.name and rep.opcode_counts.get(OP_FUNCTION, 0) <= 2:
                print(
                    "  NOTE: Production SPIR-V uses glslc -O (single inlined function). "
                    "See devel map below for per-function breakdown.\n"
                )
            print()
        else:
            print(f"MISSING {spv} — run compile_demon_hunt_shaders.bat", file=sys.stderr)
            rc = 1

    if devel and fs.is_file():
        devel_spv = root / "build" / "examples" / "demon_hunt_sky.fs.devel.spv"
        print("=== Devel SPIR-V map (glslc without -O, not shipped) ===")
        if run_glslc_noopt(fs, devel_spv, root):
            print_spirv_report(analyze_spirv(devel_spv.read_bytes(), devel_spv))
            print()
        else:
            rc = 1
    return rc


def cmd_compare(a: Path, b: Path) -> int:
    if not a.is_file() or not b.is_file():
        print("compare requires two existing .spv files", file=sys.stderr)
        return 1
    ra = analyze_spirv(a.read_bytes(), a)
    rb = analyze_spirv(b.read_bytes(), b)
    compare_spirv(ra, rb)
    return 0


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description="SPIR-V / GLSL shader debug for Situation")
    sub = parser.add_subparsers(dest="cmd", required=True)

    p_report = sub.add_parser("report", help="SPIR-V module stats")
    p_report.add_argument("spv", nargs="+", type=Path)

    p_glsl = sub.add_parser("glsl", help="GLSL function / toggle stats")
    p_glsl.add_argument("source", nargs="+", type=Path)

    p_dh = sub.add_parser("demon_hunt", help="Full report for Demon Hunt sky shaders")
    p_dh.add_argument(
        "--devel",
        action="store_true",
        help="Also compile FS without -O for per-function SPIR-V map (debug only)",
    )

    sub.add_parser("harness", help="SPIR-V stats for harness OpenGL + Vulkan blobs")

    p_cmp = sub.add_parser("compare", help="Compare two SPIR-V modules (bisect)")
    p_cmp.add_argument("before", type=Path)
    p_cmp.add_argument("after", type=Path)

    args = parser.parse_args()
    if args.cmd == "report":
        sys.exit(cmd_report(args.spv))
    if args.cmd == "glsl":
        sys.exit(cmd_glsl(args.source))
    if args.cmd == "demon_hunt":
        sys.exit(cmd_demon_hunt(root, getattr(args, "devel", False)))
    if args.cmd == "harness":
        sys.exit(cmd_harness(root))
    if args.cmd == "compare":
        sys.exit(cmd_compare(args.before, args.after))


if __name__ == "__main__":
    main()
