#!/usr/bin/env python3
"""Audit Siamese colocation policy — GL execute helpers live with their record twins."""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SIT = ROOT / "sit"

# opcode -> (record_file, record_pattern, helper_name, helper_file)
PAIRS = [
    (
        "SIT_OP_RENDER_VIRTUAL_DISPLAYS",
        SIT / "situation_impl_vd.h",
        r"SituationRenderVirtualDisplays",
        "_SitGLExecRenderVirtualDisplays",
        SIT / "situation_impl_vd.h",
    ),
]

LC = SIT / "situation_impl_renderer_lc.h"
HELPER_DEF = re.compile(r"^static\s+SituationError\s+(_SitGLExec\w+)\s*\(")


def file_text(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


def main() -> int:
    errors: list[str] = []

    for opcode, record_path, record_pat, helper, helper_path in PAIRS:
        rec = file_text(record_path)
        hel = file_text(helper_path)
        lc = file_text(LC)

        if record_pat not in rec:
            errors.append(f"missing record twin {record_pat!r} in {record_path.name}")
        if helper not in hel:
            errors.append(f"missing helper {helper!r} in {helper_path.name}")
        elif helper_path != record_path:
            errors.append(f"{helper} must live in {record_path.name}, not {helper_path.name}")

        # lc dispatch should call helper, not contain opcode body markers
        case_match = re.search(rf"case {opcode}:\s*\{{[^}}]*{helper}\(\)", lc, re.S)
        if not case_match:
            errors.append(f"{LC.name} case {opcode} must dispatch to {helper}()")

        # body should not remain inline (heuristic: compositor glBindFramebuffer in case)
        inline = re.search(
            rf"case {opcode}:.*?glBindFramebuffer\(GL_FRAMEBUFFER, 0\)",
            lc,
            re.S,
        )
        if inline:
            errors.append(f"{opcode} execute body still inline in {LC.name}")

    if errors:
        print("FAIL siamese colocation audit:")
        for e in errors:
            print(f"  - {e}")
        return 1

    print(f"OK: siamese colocation audit ({len(PAIRS)} pair(s) registered)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
