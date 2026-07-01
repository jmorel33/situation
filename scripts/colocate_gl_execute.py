#!/usr/bin/env python3
"""Mechanical colocation: extract GL execute case arms from renderer_lc.h.

Pilot / batch tool for RENDERER_SIAMESE_COLOCATION_PLAN.md.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SIT = ROOT / "sit"
LC = SIT / "situation_impl_renderer_lc.h"
VD = SIT / "situation_impl_vd.h"

CASE_RE = re.compile(r"^\s*case\s+(SIT_OP_\w+)\s*:")

# opcode -> (target_file, anchor_regex, helper_name, forward_decl_file)
REGISTRY: dict[str, dict[str, str]] = {
    "SIT_OP_RENDER_VIRTUAL_DISPLAYS": {
        "target": "vd",
        "anchor": r"^SITAPI SituationError SituationRenderVirtualDisplays\(",
        "helper": "_SitGLExecRenderVirtualDisplays",
        "sig": "static SituationError _SitGLExecRenderVirtualDisplays(void)",
        "forward_note": "defined in situation_impl_vd.h",
    },
}


def read_lines(path: Path) -> list[str]:
    return path.read_text(encoding="utf-8").splitlines(keepends=True)


def find_execute_switch_bounds(lines: list[str]) -> tuple[int, int]:
    start = next(
        i
        for i, line in enumerate(lines)
        if "_SituationGLExecuteCommands" in line and line.strip().startswith("static")
    )
    # loop switch inside function
    sw = next(i for i in range(start, len(lines)) if re.match(r"\s*switch\s*\(\s*p->opcode\s*\)", lines[i]))
    return start, sw


def extract_case_block(lines: list[str], case_line: int) -> tuple[int, int]:
    """Return [start, end) line indices for case through break; (inclusive case line)."""
    depth = 0
    started = False
    i = case_line
    while i < len(lines):
        line = lines[i]
        if CASE_RE.match(line):
            if i != case_line:
                break
        if "{" in line:
            depth += line.count("{")
            started = True
        if "}" in line:
            depth -= line.count("}")
        if started and depth <= 0 and re.search(r"\bbreak\s*;", line):
            return case_line, i + 1
        i += 1
    raise RuntimeError(f"Could not find end of case starting at line {case_line + 1}")


def dedent_body(body_lines: list[str], case_line: str) -> list[str]:
    """Turn case arm into function body (drop case label, break, one brace level)."""
    out: list[str] = []
    for raw in body_lines:
        line = raw.rstrip("\n\r")
        if CASE_RE.match(line):
            continue
        if re.search(r"^\s*break\s*;", line):
            continue
        out.append(raw)
    # strip one level of indent from remaining lines if wrapped in { }
    trimmed = [ln for ln in out if ln.strip()]
    if trimmed and trimmed[0].strip() == "{":
        trimmed = trimmed[1:]
    if trimmed and trimmed[-1].strip() == "}":
        trimmed = trimmed[:-1]
    return trimmed


def build_helper_function(meta: dict[str, str], body_lines: list[str]) -> str:
    sig = meta["sig"]
    parts = [f"{sig} {{\n"]
    parts.extend(body_lines)
    if body_lines and not body_lines[-1].endswith("\n"):
        parts[-1] = parts[-1] + "\n"
    parts.append("    return SITUATION_SUCCESS;\n")
    parts.append("}\n")
    return "".join(parts)


def insert_after_anchor(lines: list[str], anchor_pat: str, insert: str) -> list[str]:
    pat = re.compile(anchor_pat)
    # find closing brace of function starting at anchor — naive: anchor line then scan to matching }
    anchor_idx = next(i for i, line in enumerate(lines) if pat.search(line))
    depth = 0
    started = False
    end_idx = anchor_idx
    for i in range(anchor_idx, len(lines)):
        line = lines[i]
        if "{" in line:
            depth += line.count("{")
            started = True
        if "}" in line:
            depth -= line.count("}")
        if started and depth == 0:
            end_idx = i + 1
            break
    block = (
        "\n#if defined(SITUATION_USE_OPENGL)\n"
        "/* Siamese GL execute twin — colocated via scripts/colocate_gl_execute.py */\n"
        f"{insert}\n"
        "#endif\n"
    )
    return lines[:end_idx] + [block] + lines[end_idx:]


def patch_lc_case(lines: list[str], opcode: str, helper: str, meta: dict[str, str]) -> list[str]:
    case_idx = next(i for i, line in enumerate(lines) if f"case {opcode}:" in line)
    start, end = extract_case_block(lines, case_idx)
    note = meta.get("forward_note", "")
    replacement = [
        f"            case {opcode}:\n",
        "                {\n",
        f"                    SituationError vd_exec_err = {helper}();\n",
        "                    if (vd_exec_err != SITUATION_SUCCESS) return vd_exec_err;\n",
        "                }\n",
        "                break;\n",
    ]
    # ensure forward decl before _SituationGLExecuteCommands
    fn_start, _ = find_execute_switch_bounds(lines)
    forward = meta["sig"] + ";\n"
    forward_block = (
        f"#if defined(SITUATION_USE_OPENGL)\n"
        f"/* Forward — {note} */\n"
        f"{forward}"
        f"#endif\n\n"
    )
    if forward not in "".join(lines[:fn_start]):
        lines = lines[:fn_start] + [forward_block] + lines[fn_start:]
        case_idx = next(i for i, line in enumerate(lines) if f"case {opcode}:" in line)
        start, end = extract_case_block(lines, case_idx)
    return lines[:start] + replacement + lines[end:]


def main() -> int:
    ap = argparse.ArgumentParser(description="Colocate GL execute opcode bodies with VK/GL record twins.")
    ap.add_argument("--opcode", required=True, help="e.g. SIT_OP_RENDER_VIRTUAL_DISPLAYS")
    ap.add_argument("--write", action="store_true", help="Apply edits (default dry-run)")
    args = ap.parse_args()

    if args.opcode not in REGISTRY:
        print(f"Unknown opcode {args.opcode}. Known: {', '.join(REGISTRY)}", file=sys.stderr)
        return 1

    meta = REGISTRY[args.opcode]
    lc_lines = read_lines(LC)
    case_idx = next(i for i, line in enumerate(lc_lines) if f"case {args.opcode}:" in line)
    start, end = extract_case_block(lc_lines, case_idx)
    body = dedent_body(lc_lines[start:end], args.opcode)
    helper_src = build_helper_function(meta, body)

    print(f"Extracted {args.opcode} lines {start + 1}-{end} from {LC.name} ({end - start} lines)")
    print(f"Helper: {meta['helper']} -> {meta['target']}.h")

    if not args.write:
        print("Dry-run only. Pass --write to apply.")
        return 0

    if meta["target"] == "vd":
        vd_lines = read_lines(VD)
        vd_lines = insert_after_anchor(vd_lines, meta["anchor"], helper_src)
        VD.write_text("".join(vd_lines), encoding="utf-8")

    lc_lines = read_lines(LC)
    lc_lines = patch_lc_case(lc_lines, args.opcode, meta["helper"], meta)
    LC.write_text("".join(lc_lines), encoding="utf-8")

    print("Write complete.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
