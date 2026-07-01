#!/usr/bin/env python3
"""In-process bisect: find virtual_display test that pollutes vd_offset_position (Track C C-I2)."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / "build" / "tests" / "sit_test_opengl.exe"

TESTS = [
    "create_destroy_virtual_display",
    "configure_virtual_display",
    "get_virtual_display",
    "virtual_display_dirty_flag",
    "virtual_display_size",
    "render_virtual_displays",
    "vd_render_into_pipeline",
    "vd_z_ordering",
    "vd_visibility_toggle",
    "vd_opacity_blending",
    "vd_scaling_stretch",
    "vd_scaling_fit",
    "vd_scaling_integer",
    "vd_scaling_mode_switch",
    "vd_blend_alpha",
    "vd_blend_additive",
    "vd_blend_multiply",
    "vd_blend_none_overwrite",
    "vd_composite_time",
    "vd_frame_time_multiplier",
]

TARGET = "vd_offset_position"


def run_prefix(prefix: list[str]) -> bool:
    """Return True if vd_offset passes after running prefix tests in same process."""
    names = prefix + [TARGET]
    filt = "|".join(names)
    env = dict(**{k: v for k, v in __import__("os").environ.items()})
    env["PATH"] = f"{ROOT / 'build' / 'dll'};C:\\msys64\\mingw64\\bin;" + env.get("PATH", "")
    proc = subprocess.run(
        [str(EXE), "--module", "virtual_display", "--filter", filt, "--headless"],
        cwd=ROOT / "build" / "tests",
        env=env,
        capture_output=True,
        text=True,
    )
    out = proc.stdout + proc.stderr
    if TARGET + " FAIL" in out.replace("[FAIL]", " FAIL") or f"[FAIL] {TARGET}" in out:
        return False
    if f"[ OK ] {TARGET}" in out:
        return True
    print(out, file=sys.stderr)
    raise RuntimeError(f"unexpected harness output (exit {proc.returncode})")


def main() -> int:
    if not EXE.is_file():
        print(f"Missing {EXE} — build tests first.", file=sys.stderr)
        return 1

    lo, hi = 0, len(TESTS) - 1
    print(f"Bisect polluter for {TARGET} ({len(TESTS)} candidates before target)")
    while lo < hi:
        mid = (lo + hi) // 2
        prefix = TESTS[: mid + 1]
        ok = run_prefix(prefix)
        tag = "PASS" if ok else "FAIL"
        print(f"  prefix[0..{mid}] ({TESTS[mid]}) + {TARGET}: {tag}")
        if ok:
            lo = mid + 1
        else:
            hi = mid

    print(f"\nPolluter (C-I2): {TESTS[lo]}")
    verify = run_prefix(TESTS[:lo])
    print(f"  prefix[0..{lo - 1}] + {TARGET}: {'PASS' if verify else 'FAIL'}")
    final = run_prefix(TESTS[: lo + 1])
    print(f"  prefix[0..{lo}] + {TARGET}: {'PASS' if final else 'FAIL'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
