#!/usr/bin/env python3
"""PNG -> .ico для SD-карти (формат описано в docs/icons-sd.md / у звіті).

Формат файлу (little-endian не потрібен, усе по 1 байту):
  байт 0   'I'  (magic)
  байт 1   '1'  (версія формату / 1 біт на піксель)
  байт 2   w    (1..255)
  байт 3   h    (1..255)
  байти 4… XBM-дані: h рядків по ceil(w/8) байтів, у байті молодший біт = лівий
           піксель (рівно те, що очікує u8g2_DrawXBM). Біт 1 = «чорнило».

Використання:
  python3 tools/png2icon.py assets/icons_src/*.png -o assets/sd/icons
  python3 tools/png2icon.py x.png --threshold 128 --invert
Прозорий піксель (alpha < 128) = 0. Темніше за поріг = 1 (чорнило).
"""
import argparse, sys
from pathlib import Path
from PIL import Image

def convert(path, threshold, invert):
    im = Image.open(path).convert("RGBA")
    w, h = im.size
    if not (1 <= w <= 255 and 1 <= h <= 255):
        sys.exit(f"{path}: розмір {w}x{h} поза 1..255")
    # композит на білому: прозоре -> біле -> 0
    bg = Image.new("RGBA", im.size, (255, 255, 255, 255))
    gray = Image.alpha_composite(bg, im).convert("L")
    px = gray.load()
    stride = (w + 7) // 8
    out = bytearray([ord("I"), ord("1"), w, h])
    for y in range(h):
        row = bytearray(stride)
        for x in range(w):
            ink = px[x, y] < threshold
            if invert:
                ink = not ink
            if ink:
                row[x // 8] |= 1 << (x % 8)   # LSB = лівий піксель
        out += row
    return bytes(out)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("files", nargs="+", type=Path)
    ap.add_argument("-o", "--out", type=Path, default=Path("assets/sd/icons"))
    ap.add_argument("--threshold", type=int, default=128)
    ap.add_argument("--invert", action="store_true")
    a = ap.parse_args()
    a.out.mkdir(parents=True, exist_ok=True)
    for f in a.files:
        data = convert(f, a.threshold, a.invert)
        dst = a.out / (f.stem.lower() + ".ico")
        dst.write_bytes(data)
        print(f"{f.name} -> {dst} ({data[2]}x{data[3]}, {len(data)} B)")

if __name__ == "__main__":
    main()
