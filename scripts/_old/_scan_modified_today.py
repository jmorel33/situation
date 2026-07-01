import os
from datetime import datetime

situation_path = os.path.dirname(os.path.abspath(__file__))
today = datetime.now().date()
out_path = os.path.join(situation_path, "_modified_today.txt")

lines = []
lines.append(f"Today: {today:%Y-%m-%d}")
lines.append(f"Scanned: {situation_path}")
lines.append("")

matches = []
for root, dirs, files in os.walk(situation_path):
    if "_modified_today.txt" in files and root == situation_path:
        pass
    for name in files:
        if name == "_modified_today.txt":
            continue
        full = os.path.join(root, name)
        try:
            mtime = datetime.fromtimestamp(os.path.getmtime(full))
        except OSError:
            continue
        if mtime.date() == today:
            rel = os.path.relpath(full, situation_path)
            matches.append((mtime, rel))

matches.sort(key=lambda x: x[0], reverse=True)
lines.append(f"Count: {len(matches)}")
lines.append("")
for mtime, rel in matches:
    lines.append(f"{mtime:%Y-%m-%d %H:%M:%S} | {rel}")

with open(out_path, "w", encoding="utf-8") as f:
    f.write("\n".join(lines) + "\n")
