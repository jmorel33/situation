#!/usr/bin/env python3
"""One-off: migrate _SitGLSoftCmdPush/_SitGLSoftDataPush call sites to Phase 3 API."""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def patch(text: str) -> str:
    text = re.sub(
        r"if\s*\(\s*!_SitGLSoftCmdPush\(\s*([^,]+)\s*,\s*([^)]+)\s*\)\s*\)\s*return\s+([^;]+);",
        r"SitCommandPacket* _sit_pkt_ = NULL; SIT_GL_SOFT_CMD_PUSH(\1, \2, _sit_pkt_);",
        text,
    )

    def repl_assign(m):
        var, buf, op = m.group(1), m.group(2), m.group(3)
        return f"SitCommandPacket* {var} = NULL;\n    SIT_GL_SOFT_CMD_PUSH({buf}, {op}, {var});"

    text = re.sub(
        r"SitCommandPacket\*\s*(\w+)\s*=\s*_SitGLSoftCmdPush\(\s*([^,]+)\s*,\s*([^)]+)\s*\)\s*;",
        repl_assign,
        text,
    )
    text = re.sub(
        r"\n\s*if\s*\(\s*!\s*(\w+)\s*\)\s*return\s+SITUATION_ERROR_MEMORY_ALLOCATION\s*;",
        "",
        text,
    )

    def repl_data(m):
        var, buf, data, size = m.group(1), m.group(2), m.group(3), m.group(4)
        return f"void* {var} = NULL;\n    SIT_GL_SOFT_DATA_PUSH({buf}, {data}, {size}, {var});"

    text = re.sub(
        r"void\*\s*(\w+)\s*=\s*_SitGLSoftDataPush\(\s*([^,]+)\s*,\s*([^,]+)\s*,\s*([^)]+)\s*\)\s*;",
        repl_data,
        text,
    )
    return text

def main():
    for rel in ("sit/situation_impl_renderer.h", "sit/situation_impl_vd.h"):
        path = ROOT / rel
        old = path.read_text(encoding="utf-8")
        new = patch(old)
        if new != old:
            path.write_text(new, encoding="utf-8", newline="\n")
            print("patched", rel)
        else:
            print("unchanged", rel)

if __name__ == "__main__":
    main()
