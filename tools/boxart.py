#!/usr/bin/env python3
"""The black-box art: a 1985 launch-window style box front rendered from the game's own court screen.
Usage: boxart.py build/court.png art/boxart.png"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from PIL import Image, ImageDraw
src, out = sys.argv[1], sys.argv[2]
W, H = 600, 840
img = Image.new("RGB", (W, H), (12, 12, 12))
d = ImageDraw.Draw(img)
# the silver grid of the black boxes
for x in range(0, W, 24):
    d.line([(x, 0), (x, H)], fill=(70, 70, 70), width=1)
for y in range(0, H, 24):
    d.line([(0, y), (W, y)], fill=(70, 70, 70), width=1)
# the game screen, pixel-doubled
court = Image.open(src).convert("RGB")
court = court.resize((512, 480), Image.NEAREST if court.width <= 256 else Image.LANCZOS)
img.paste(court, ((W - 512) // 2, 150))
d.rectangle([(W - 512) // 2 - 6, 144, (W + 512) // 2 + 6, 636], outline=(200, 200, 200), width=4)
# the title block: thick pixel letters drawn from the game's font would be truest; a blocky fallback here
def block_text(text, x, y, s, col):
    from font import G
    cx = x
    for ch in text:
        if ch == " ":
            cx += 4 * s; continue
        rows = G.get(ch)
        if rows:
            for r, row in enumerate(rows):
                for c, px in enumerate(row):
                    if px == "#":
                        d.rectangle([cx + c * s, y + r * s, cx + (c + 1) * s - 1, y + (r + 1) * s - 1], fill=col)
        cx += 6 * s
block_text("PICKLEBALL", 60, 40, 8, (240, 240, 240))
block_text("SPORTS SERIES", 150, 110, 3, (240, 200, 40))
block_text("1985 EDITION", 160, 660, 4, (240, 240, 240))
block_text("1 OR 2 PLAYERS  FIVE LEVELS", 90, 700, 2, (200, 200, 200))
block_text("SINGLES  DOUBLES  EXHIBITION", 90, 724, 2, (200, 200, 200))
block_text("THE OFFICIAL 1984 RULES", 90, 748, 2, (200, 200, 200))
block_text("MERCURIOUS", 180, 790, 3, (240, 200, 40))
img.save(out)
print("box art:", out, img.size)
