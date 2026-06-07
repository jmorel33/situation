#!/usr/bin/env python3
"""Phase 0 / 0.1: extract (set, binding) from harness Vulkan FS SPIR-V and check vs GLSL."""
import struct
import sys
from pathlib import Path

# Expected (set, binding) per decorated resource variable, in OpVariable order (first..second).
# Update after harness GLSL / glslc flag changes; run from repo root after compile_harness_shaders.ps1.
EXPECTED_VARS = {
    "harness_dual_ssbo_vk.fs.spv": [(0, 0), (1, 0)],
    "harness_ubo_ssbo_vk.fs.spv": [(0, 0), (1, 0)],  # set0=Frame UBO, set1=TagBlock SSBO (harness binds)
}

OP_DECORATE = 71
OP_VARIABLE = 59
DEC_BINDING = 33
DEC_DESCRIPTOR_SET = 34
DEC_BLOCK = 2
DEC_BUFFER_BLOCK = 3
SC_UNIFORM = 2
SC_STORAGE = 12


def parse_words(data: bytes):
    if len(data) < 20 or struct.unpack_from("<I", data, 0)[0] != 0x07230203:
        raise ValueError("not SPIR-V")
    off = 20
    insns = []
    while off < len(data):
        w0 = struct.unpack_from("<I", data, off)[0]
        cnt = (w0 >> 16) & 0xFFFF
        op = w0 & 0xFFFF
        words = struct.unpack_from("<" + "I" * cnt, data, off)
        insns.append((op, words))
        off += 4 * cnt
    return insns


def analyze(data: bytes, label: str):
    insns = parse_words(data)
    decs = {}
    var_to_type = {}
    type_to_name = {}
    for op, words in insns:
        if op == 5 and len(words) >= 3:  # OpName
            type_to_name[words[1]] = _decode_spirv_string(words, 2)
        if op == OP_DECORATE and len(words) >= 3:
            target, decoration = words[1], words[2]
            if decoration == DEC_BINDING and len(words) >= 4:
                decs.setdefault(target, {})["binding"] = words[3]
            elif decoration == DEC_DESCRIPTOR_SET and len(words) >= 4:
                decs.setdefault(target, {})["set"] = words[3]
            elif decoration in (DEC_BLOCK, DEC_BUFFER_BLOCK):
                decs.setdefault(target, {})["block_kind"] = (
                    "BufferBlock" if decoration == DEC_BUFFER_BLOCK else "Block"
                )
        if op == OP_VARIABLE and len(words) >= 4:
            var_to_type[words[2]] = words[1]
    print(f"=== {label} ===")
    expected_vars = EXPECTED_VARS.get(label)
    got_vars = []
    for op, words in insns:
        if op == OP_VARIABLE and len(words) >= 4:
            _typ, var_id, storage = words[1], words[2], words[3]
            d = decs.get(var_id, {})
            if "set" not in d and "binding" not in d:
                continue
            type_id = var_to_type.get(var_id)
            name = type_to_name.get(type_id, f"var_{var_id:#x}")
            sc = {SC_UNIFORM: "Uniform", SC_STORAGE: "StorageBuffer"}.get(
                storage, f"sc={storage}"
            )
            got = (d.get("set"), d.get("binding"))
            got_vars.append(got)
            print(f"  {name}: {sc} set={got[0]} binding={got[1]}")
    if expected_vars is not None:
        got_sorted = sorted(got_vars, key=lambda x: (x[0] is None, x[0] or 0, x[1] or 0))
        expected_sorted = sorted(expected_vars, key=lambda x: (x[0] is None, x[0] or 0, x[1] or 0))
        if got_sorted == expected_sorted:
            print("  PASS: matches EXPECTED_VARS (harness bind order)")
            return 0
        print(f"  FAIL: got {got_sorted} expected {expected_sorted}")
        return 1
    return 0


def _decode_spirv_string(words, start):
    chars = []
    for i in range(start, len(words)):
        w = words[i]
        for shift in (0, 8, 16, 24):
            c = (w >> shift) & 0xFF
            if c == 0:
                return "".join(chars)
            chars.append(chr(c))
    return "".join(chars)


def main():
    root = Path(__file__).resolve().parents[1] / "tests" / "harness" / "spirv_out"
    rc = 0
    for name in ("harness_dual_ssbo_vk.fs.spv", "harness_ubo_ssbo_vk.fs.spv"):
        p = root / name
        if not p.is_file():
            print(f"MISSING {p}", file=sys.stderr)
            rc = 1
            continue
        if analyze(p.read_bytes(), name):
            rc = 1
    sys.exit(rc)


if __name__ == "__main__":
    main()
