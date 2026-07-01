#!/usr/bin/env python3
"""Read version macros from sit/situation_base_version.h (canonical source).

Used by build scripts to stamp PE resources without hardcoded drift.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "sit" / "situation_base_version.h"


def read_version() -> tuple[int, int, int]:
    text = HEADER.read_text(encoding="utf-8")

    def macro_int(name: str) -> int:
        match = re.search(rf"#define\s+{re.escape(name)}\s+(\d+)", text)
        if not match:
            raise SystemExit(f"read_situation_version: missing {name} in {HEADER}")
        return int(match.group(1))

    return (
        macro_int("SITUATION_VERSION_MAJOR"),
        macro_int("SITUATION_VERSION_MINOR"),
        macro_int("SITUATION_VERSION_PATCH"),
    )


def main() -> None:
    major, minor, patch = read_version()
    mode = sys.argv[1] if len(sys.argv) > 1 else "--string"

    if mode == "--string":
        print(f"{major}.{minor}.{patch}")
    elif mode == "--windres":
        print(
            f"-DSIT_VERSION_MAJOR={major} "
            f"-DSIT_VERSION_MINOR={minor} "
            f"-DSIT_VERSION_PATCH={patch}"
        )
    elif mode == "--make":
        print(f"SIT_VERSION_MAJOR := {major}")
        print(f"SIT_VERSION_MINOR := {minor}")
        print(f"SIT_VERSION_PATCH := {patch}")
    else:
        raise SystemExit(
            "usage: read_situation_version.py [--string|--windres|--make]"
        )


if __name__ == "__main__":
    main()
