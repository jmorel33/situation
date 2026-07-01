#!/usr/bin/env python3
"""
gen_situation_icon.py
Generates sit/situation_icon.ico from icon_source.PNG (project root).

The source image is already RGBA (transparency present). This script:
  1. Loads the source PNG
  2. Removes the dark background using flood-fill alpha masking
     (handles any residual near-black pixels the source still has)
  3. Downsamples to all required icon sizes using Lanczos resampling
  4. Writes the multi-resolution .ico to sit/situation_icon.ico

Requirements:
    pip install Pillow

Usage (from project root):
    python scripts/gen_situation_icon.py

Output:
    sit/situation_icon.ico    -- committed to repo, used by windres
    sit/situation_icon_src.png -- clean RGBA source (copy of processed image)
"""

import sys
import os
from pathlib import Path

try:
    from PIL import Image, ImageFilter
except ImportError:
    print("[ERROR] Pillow is not installed.")
    print("        Install it with:  pip install Pillow")
    sys.exit(1)

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
SCRIPT_DIR   = Path(__file__).parent
PROJECT_ROOT = SCRIPT_DIR.parent
SRC_PATH     = SCRIPT_DIR / "art" / "icon_source.PNG"
OUT_ICO      = PROJECT_ROOT / "sit" / "platform" / "windows" / "situation_icon.ico"

# ---------------------------------------------------------------------------
# Icon sizes to embed in the .ico container
# 256 is stored as PNG-compressed inside the ICO (Windows 10/11 high-DPI)
# All others are stored as BMP bitmaps
# ---------------------------------------------------------------------------
ICON_SIZES = [
    (256, 256),
    (128, 128),
    (64,  64),
    (48,  48),
    (32,  32),
    (16,  16),
]

# ---------------------------------------------------------------------------
# Background removal
# Flood-fills from each corner with alpha=0 for pixels sufficiently close
# to the background color. Threshold controls aggressiveness.
# ---------------------------------------------------------------------------
def remove_dark_background(img: Image.Image, threshold: int = 40) -> Image.Image:
    """
    Remove dark background by flood-filling from the image edges, then
    doing a second pass to catch small enclosed background pockets that
    the edge fill couldn't reach (e.g. inside hourglass corners where
    the rim creates a closed boundary).
    """
    import collections

    img = img.convert("RGBA")
    data = img.load()
    w, h = img.size

    # Sample background color from the four corners (average)
    corners = [data[0, 0], data[w-1, 0], data[0, h-1], data[w-1, h-1]]
    bg_r = sum(c[0] for c in corners) // 4
    bg_g = sum(c[1] for c in corners) // 4
    bg_b = sum(c[2] for c in corners) // 4

    print(f"  Background color sampled: RGB({bg_r}, {bg_g}, {bg_b})")

    def color_dist(r, g, b):
        return ((r - bg_r)**2 + (g - bg_g)**2 + (b - bg_b)**2) ** 0.5

    def flood_fill(seeds, thresh):
        visited_set = set()
        queue = collections.deque()
        for sx, sy in seeds:
            if (sx, sy) not in visited_set:
                r, g, b, a = data[sx, sy]
                if a > 0 and color_dist(r, g, b) < thresh:
                    queue.append((sx, sy))
                    visited_set.add((sx, sy))
        removed = 0
        while queue:
            x, y = queue.popleft()
            r, g, b, a = data[x, y]
            if a > 0:
                data[x, y] = (r, g, b, 0)
                removed += 1
            for nx, ny in [(x-1,y),(x+1,y),(x,y-1),(x,y+1)]:
                if 0 <= nx < w and 0 <= ny < h and (nx, ny) not in visited_set:
                    nr, ng, nb, na = data[nx, ny]
                    if na > 0 and color_dist(nr, ng, nb) < thresh:
                        visited_set.add((nx, ny))
                        queue.append((nx, ny))
        return removed

    # Pass 1: flood fill from all border pixels
    border_seeds = []
    for x in range(w):
        border_seeds.append((x, 0))
        border_seeds.append((x, h-1))
    for y in range(1, h-1):
        border_seeds.append((0, y))
        border_seeds.append((w-1, y))

    removed1 = flood_fill(border_seeds, threshold)
    print("  Pass 1 (edge flood-fill): removed %d pixels" % removed1)

    # Pass 2: remove enclosed background pockets unreachable from the edges.
    # These are the 4 concave corners of the hourglass silhouette where
    # background-colored pixels are trapped inside the shape's outline.
    # Strategy: any remaining opaque pixel that matches the background color
    # (tight threshold) and is NOT adjacent to any non-background opaque pixel
    # is interior background that should be transparent.
    # We use a small-area connected-component check: find all remaining
    # background-matching opaque pixels, group them into connected components,
    # and remove only components that are small (enclosed pockets, not logo body).
    bg_remaining = []
    for y in range(h):
        for x in range(w):
            r, g, b, a = data[x, y]
            if a > 0 and color_dist(r, g, b) < threshold + 5:
                bg_remaining.append((x, y))

    # Connected components of remaining background pixels
    remaining_set = set(bg_remaining)
    visited_cc = set()
    pocket_pixels = set()

    for start in bg_remaining:
        if start in visited_cc:
            continue
        # BFS to find connected component
        component = []
        q = collections.deque([start])
        visited_cc.add(start)
        while q:
            cx, cy = q.popleft()
            component.append((cx, cy))
            for nx, ny in [(cx-1,cy),(cx+1,cy),(cx,cy-1),(cx,cy+1)]:
                nb = (nx, ny)
                if nb in remaining_set and nb not in visited_cc:
                    visited_cc.add(nb)
                    q.append(nb)
        # Small components are enclosed pockets; large ones are exterior remnants
        # The exterior background was already removed so any remaining component
        # is by definition a trapped pocket -- remove all of them
        pocket_pixels.update(component)

    for x, y in pocket_pixels:
        r, g, b, a = data[x, y]
        data[x, y] = (r, g, b, 0)

    print("  Pass 2 (enclosed pockets): removed %d pixels" % len(pocket_pixels))

    # Pass 3: soften anti-aliased rim pixels adjacent to transparent areas.
    fade_end = threshold + 35
    softened = 0
    for y in range(h):
        for x in range(w):
            r, g, b, a = data[x, y]
            if a > 0:
                dist = color_dist(r, g, b)
                if threshold <= dist < fade_end:
                    has_transp = any(
                        0 <= nx < w and 0 <= ny < h and data[nx, ny][3] == 0
                        for nx, ny in [(x-1,y),(x+1,y),(x,y-1),(x,y+1)]
                    )
                    if has_transp:
                        t = (dist - threshold) / (fade_end - threshold)
                        new_a = int(t * 255)
                        data[x, y] = (r, g, b, new_a)
                        softened += 1
    print("  Pass 3 (rim fade): softened %d edge pixels" % softened)

    return img


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main():
    if not SRC_PATH.exists():
        print(f"[ERROR] Source image not found: {SRC_PATH}")
        print("        Save the logo as 'scripts/art/icon_source.PNG'.")
        sys.exit(1)

    print(f"[icon] Loading source: {SRC_PATH}")
    img = Image.open(SRC_PATH).convert("RGBA")
    print(f"  Size: {img.width} x {img.height}")

    print("[icon] Removing dark background (flood-fill from edges)...")
    img = remove_dark_background(img, threshold=30)

    # Save the .ico
    OUT_ICO.parent.mkdir(parents=True, exist_ok=True)
    print("[icon] Generating icon sizes...")
    frames = []
    for size in ICON_SIZES:
        frame = img.resize(size, Image.LANCZOS)
        frames.append(frame)
        print("  %3dx%-3d -- done" % (size[0], size[1]))

    # Write the .ico file
    # Pillow writes the first image as the primary, appends the rest.
    # sizes= tells Pillow to embed each frame at its actual size.
    frames[0].save(
        OUT_ICO,
        format="ICO",
        append_images=frames[1:],
        sizes=ICON_SIZES,
    )
    print(f"[icon] ICO written: {OUT_ICO}")
    print(f"       {OUT_ICO.stat().st_size:,} bytes, {len(ICON_SIZES)} sizes embedded")
    print()
    print("Next steps:")
    print("  1. Visually check sit/platform/windows/situation_icon.ico")
    print("  2. build\\build_situation.bat opengl  -- icon is embedded automatically")
    print()


if __name__ == "__main__":
    main()
