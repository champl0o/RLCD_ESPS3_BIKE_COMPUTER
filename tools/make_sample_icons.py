#!/usr/bin/env python3

from pathlib import Path
from PIL import Image, ImageDraw
out = Path(__file__).resolve().parent.parent / "assets/icons_src"
out.mkdir(parents=True, exist_ok=True)
S = 24
def new():
    im = Image.new("1", (S, S), 1); return im, ImageDraw.Draw(im)

# thermometer
im, d = new()
d.rounded_rectangle((9, 1, 14, 15), radius=2, outline=0, width=2)
d.ellipse((5, 13, 18, 22), fill=0); d.rectangle((11, 6, 12, 14), fill=0)
im.save(out / "temp.png")
# droplet (humidity)
im, d = new()
d.polygon([(12, 1), (4, 13), (20, 13)], fill=0); d.ellipse((4, 8, 20, 22), fill=0)
d.arc((7, 12, 15, 19), 100, 190, fill=1, width=2)
im.save(out / "humidity.png")
# mountain (elevation)
im, d = new()
d.polygon([(0, 21), (8, 6), (13, 14), (17, 9), (23, 21)], fill=0)
d.polygon([(8, 6), (6, 10), (8, 9), (10, 10)], fill=1)
im.save(out / "climb.png")
