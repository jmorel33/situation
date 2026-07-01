#!/usr/bin/env python3
"""Legacy: split v2.3.x from monolithic UPDATELOG. Superseded by split_updatelog_chunks.py."""
from __future__ import annotations

import sys

if __name__ == "__main__":
    print(
        "split_updatelog_v23.py is superseded by split_updatelog_chunks.py.\n"
        "Run: python scripts/split_updatelog_chunks.py",
        file=sys.stderr,
    )
    raise SystemExit(1)