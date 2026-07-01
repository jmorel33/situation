#!/usr/bin/env python3
"""Backward-compatible shim — delegates to tools/generate_api_index.py."""

from __future__ import annotations

import runpy
import sys
from pathlib import Path

TARGET = Path(__file__).resolve().parents[1] / "tools" / "generate_api_index.py"

if __name__ == "__main__":
    sys.argv[0] = str(TARGET)
    runpy.run_path(str(TARGET), run_name="__main__")
