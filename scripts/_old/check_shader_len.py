import re
from pathlib import Path

t = Path(r"C:\Users\User\Desktop\hobby\_kiro\situation\wrappers\Modula2\examples\hello_situation\Main.mod").read_text(encoding="utf-8")
for name in ["FRAG_SRC_GL", "FRAG_SRC_VK", "VERT_SRC_GL", "VERT_SRC_VK"]:
    m = re.search(rf"{name}\s*=\s*\"([^\"]+)\"", t, re.S)
    if m:
        print(f"{name}: {len(m.group(1))} chars")
    else:
        print(f"{name}: NOT FOUND")