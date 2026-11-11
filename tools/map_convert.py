#!/usr/bin/env python3
"""OSM (Overpass JSON) to .bkm tile converter for the bike computer.

Format and decisions: docs/map-design.md. Standard library only.

  python3 tools/map_convert.py fetch --center 49.8397,24.0297 --radius-km 10 -o tools/out/lviv.json
  python3 tools/map_convert.py convert tools/out/lviv.json -o tools/out/map
"""
import argparse
import json
import math
import os
import struct
import sys
import urllib.request

ZOOM = 15
TILE_UNITS = 4096
TILE_MARGIN = 256           # units beyond the tile edge (~49 m): seamless joins
MAX_POINTS_PER_FEATURE = 1000
DEFAULT_TOLERANCE = 10.0    # units (~2 m): 1 pixel at 2 m/pixel
MAX_LAT = 85.0511287798

CLASS_HIGHWAY, CLASS_STREET, CLASS_BIKE, CLASS_WATER, CLASS_RAIL = 1, 2, 3, 4, 5

_HIGHWAY = {"motorway", "trunk", "primary", "secondary"}
_STREET = {"tertiary", "unclassified", "residential", "living_street"}

OVERPASS_URL = "https://overpass-api.de/api/interpreter"


def classify(tags):
    if not isinstance(tags, dict):
        return None
    hw = tags.get("highway")
    if isinstance(hw, str):
        base = hw[:-5] if hw.endswith("_link") else hw
        if base in _HIGHWAY:
            return CLASS_HIGHWAY
        if base in _STREET:
            return CLASS_STREET
        if hw == "cycleway":
            return CLASS_BIKE
        if hw in ("path", "footway") and tags.get("bicycle") == "designated":
            return CLASS_BIKE
    if tags.get("waterway") in ("river", "canal") or tags.get("natural") == "water":
        return CLASS_WATER
    if tags.get("railway") == "rail":
        return CLASS_RAIL
    return None


# ---------- projection ----------

def world_units(lat, lon, z=ZOOM):
    if not (math.isfinite(lat) and math.isfinite(lon)):
        raise ValueError("non-finite coordinate")
    if abs(lat) > MAX_LAT or not -180.0 <= lon <= 180.0:
        raise ValueError("outside Web Mercator")
    size = TILE_UNITS * 2 ** z
    wx = (lon + 180.0) / 360.0 * size
    wy = (1.0 - math.asinh(math.tan(math.radians(lat))) / math.pi) / 2.0 * size
    return wx, wy


# ---------- Douglas-Peucker ----------

def _dist(p, a, b):
    dx, dy = b[0] - a[0], b[1] - a[1]
    if dx == 0 and dy == 0:
        return math.hypot(p[0] - a[0], p[1] - a[1])
    return abs(dx * (a[1] - p[1]) - (a[0] - p[0]) * dy) / math.hypot(dx, dy)


def _dp(pts, tol):
    n = len(pts)
    keep = [False] * n
    keep[0] = keep[-1] = True
    stack = [(0, n - 1)]  # iterative: long lines do not hit the recursion limit
    while stack:
        lo, hi = stack.pop()
        far, far_d = -1, tol
        for i in range(lo + 1, hi):
            d = _dist(pts[i], pts[lo], pts[hi])
            if d > far_d:
                far, far_d = i, d
        if far >= 0:
            keep[far] = True
            stack.append((lo, far))
            stack.append((far, hi))
    return [p for p, k in zip(pts, keep) if k]


def simplify(pts, tol):
    pts = list(pts)
    if len(pts) < 3:
        return pts
    return _dp(pts, tol)  # rings too: _dist measures to the point for degenerate chords


# ---------- splitting into tiles ----------

def _clip_segment(a, b, lo, hi):
    """Liang-Barsky against the square [lo,hi]^2 (float); returns an integer segment or None."""
    x0, y0 = a
    dx, dy = b[0] - a[0], b[1] - a[1]
    t0, t1 = 0.0, 1.0
    for p, q in ((-dx, x0 - lo), (dx, hi - x0), (-dy, y0 - lo), (dy, hi - y0)):
        if p == 0:
            if q < 0:
                return None
            continue
        r = q / p
        if p < 0:
            if r > t1:
                return None
            t0 = max(t0, r)
        else:
            if r < t0:
                return None
            t1 = min(t1, r)
    pa = (round(x0 + t0 * dx), round(y0 + t0 * dy))
    pb = (round(x0 + t1 * dx), round(y0 + t1 * dy))
    return None if pa == pb else (pa, pb)


def split_to_tiles(pts, z=ZOOM):
    """World integer points -> {(tx,ty): [polylines in tile coordinates]}."""
    last = 2 ** z - 1
    out = {}
    for a, b in zip(pts, pts[1:]):
        t_x0 = max(0, (min(a[0], b[0]) - TILE_MARGIN) // TILE_UNITS)
        t_x1 = min(last, (max(a[0], b[0]) + TILE_MARGIN) // TILE_UNITS)
        t_y0 = max(0, (min(a[1], b[1]) - TILE_MARGIN) // TILE_UNITS)
        t_y1 = min(last, (max(a[1], b[1]) + TILE_MARGIN) // TILE_UNITS)
        for ty in range(t_y0, t_y1 + 1):
            for tx in range(t_x0, t_x1 + 1):
                ox, oy = tx * TILE_UNITS, ty * TILE_UNITS
                seg = _clip_segment((a[0] - ox, a[1] - oy), (b[0] - ox, b[1] - oy),
                                    -TILE_MARGIN, TILE_UNITS + TILE_MARGIN)
                if seg is None:
                    continue
                lines = out.setdefault((tx, ty), [])
                if lines and lines[-1][-1] == seg[0]:
                    lines[-1].append(seg[1])
                else:
                    lines.append([seg[0], seg[1]])
    return out


def chunk(pts, limit):
    """Cuts a long polyline into pieces of <= limit points sharing a join point."""
    if len(pts) <= limit:
        return [list(pts)]
    out, i = [], 0
    while i < len(pts) - 1:
        out.append(list(pts[i:i + limit]))
        i += limit - 1
    return out


# ---------- .bkm format ----------

def encode_tile(features):
    if len(features) > 0xFFFF:
        raise ValueError("too many features")
    out = bytearray(b"BKM1")
    out += struct.pack("<BHB", 1, len(features), 0)
    for cls, pts in features:
        if not CLASS_HIGHWAY <= cls <= CLASS_RAIL:
            raise ValueError("bad class")
        if not 2 <= len(pts) <= 0xFFFF:
            raise ValueError("bad point count")
        out += struct.pack("<BH", cls, len(pts))
        px = py = 0
        for x, y in pts:
            if not (-32768 <= x <= 32767 and -32768 <= y <= 32767):
                raise ValueError("coordinate outside int16")
            dx, dy = x - px, y - py
            if not (-32768 <= dx <= 32767 and -32768 <= dy <= 32767):
                raise ValueError("delta outside int16")
            out += struct.pack("<hh", dx, dy)
            px, py = x, y
    return bytes(out)


def decode_tile(raw):
    """Strict parser (mirror of the C reader), for tests and checks."""
    if len(raw) < 8 or raw[:4] != b"BKM1" or raw[4] != 1:
        raise ValueError("bad header")
    count = struct.unpack_from("<H", raw, 5)[0]
    pos, feats = 8, []
    for _ in range(count):
        if len(raw) - pos < 3:
            raise ValueError("truncated")
        cls, n = struct.unpack_from("<BH", raw, pos)
        pos += 3
        if not CLASS_HIGHWAY <= cls <= CLASS_RAIL or n < 2:
            raise ValueError("bad feature")
        if len(raw) - pos < 4 * n:
            raise ValueError("truncated")
        pts, x, y = [], 0, 0
        for _ in range(n):
            dx, dy = struct.unpack_from("<hh", raw, pos)
            pos += 4
            x, y = x + dx, y + dy
            pts.append((x, y))
        feats.append((cls, pts))
    if pos != len(raw):
        raise ValueError("trailing bytes")
    return feats


# ---------- conversion ----------

def _num(v):
    return isinstance(v, (int, float)) and not isinstance(v, bool) and math.isfinite(v)


def _way_points(el):
    geom = el.get("geometry")
    if not isinstance(geom, list):
        return None
    pts = []
    for g in geom:
        if not isinstance(g, dict) or not _num(g.get("lat")) or not _num(g.get("lon")):
            return None
        try:
            pts.append(world_units(g["lat"], g["lon"]))
        except ValueError:
            return None
    return pts if len(pts) >= 2 else None


def convert(data, out_dir, tolerance=DEFAULT_TOLERANCE, zoom=ZOOM):
    if zoom != ZOOM:
        raise ValueError("only z=15 is supported")
    elements = data.get("elements", []) if isinstance(data, dict) else []
    tiles = {}
    ways = 0
    for el in elements:
        if not isinstance(el, dict) or el.get("type") != "way":
            continue
        cls = classify(el.get("tags"))
        pts = _way_points(el) if cls else None
        if pts is None:
            continue
        xs, ys = [p[0] for p in pts], [p[1] for p in pts]
        if max(max(xs) - min(xs), max(ys) - min(ys)) < tolerance:
            continue  # smaller than the pixel tolerance: not visible on screen
        simple = simplify(pts, tolerance)
        ints = []
        for x, y in simple:
            q = (round(x), round(y))
            if not ints or ints[-1] != q:
                ints.append(q)
        if len(ints) < 2:
            continue
        ways += 1
        for key, lines in split_to_tiles(ints).items():
            for line in lines:
                for piece in chunk(line, MAX_POINTS_PER_FEATURE):
                    tiles.setdefault(key, []).append((cls, piece))

    total = 0
    for (tx, ty), feats in sorted(tiles.items()):
        raw = encode_tile(feats)
        path = os.path.join(out_dir, str(zoom), str(tx), f"{ty}.bkm")
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(raw)
        total += len(raw)
    return {"ways": ways, "tiles": len(tiles),
            "features": sum(len(v) for v in tiles.values()), "bytes": total}


def clean_output(out_dir, zoom=ZOOM):
    """Removes only *.bkm under <out>/<zoom> and empty directories; leaves foreign files."""
    root = os.path.join(out_dir, str(zoom))
    for d, _, files in os.walk(root, topdown=False):
        for name in files:
            if name.endswith(".bkm"):
                os.remove(os.path.join(d, name))
        try:
            os.rmdir(d)
        except OSError:
            pass


# ---------- Overpass ----------

def bbox_around(lat, lon, radius_km):
    dlat = radius_km / 111.32
    dlon = radius_km / (111.32 * math.cos(math.radians(lat)))
    return lat - dlat, lon - dlon, lat + dlat, lon + dlon


def parse_bbox(text):
    """'S,W,N,E' -> (s, w, n, e); ValueError unless four finite numbers within range."""
    parts = text.split(",")
    if len(parts) != 4:
        raise ValueError("bbox needs S,W,N,E")
    s, w, n, e = (float(v) for v in parts)
    if not all(math.isfinite(v) for v in (s, w, n, e)):
        raise ValueError("non-finite bbox")
    if not (-MAX_LAT <= s < n <= MAX_LAT and -180.0 <= w < e <= 180.0):
        raise ValueError("bbox outside Web Mercator or empty")
    return s, w, n, e


def build_query(bbox):
    b = "{},{},{},{}".format(*bbox)
    return (
        "[out:json][timeout:300];\n(\n"
        f'  way["highway"~"^(motorway|trunk|primary|secondary|tertiary|unclassified'
        f'|residential|living_street|cycleway)(_link)?$"]({b});\n'
        f'  way["highway"~"^(path|footway)$"]["bicycle"="designated"]({b});\n'
        f'  way["waterway"~"^(river|canal)$"]({b});\n'
        f'  way["natural"="water"]({b});\n'
        f'  way["railway"="rail"]({b});\n'
        ");\nout geom;\n"
    )


def fetch_overpass(query, url=OVERPASS_URL):
    req = urllib.request.Request(
        url, data=query.encode("utf-8"),
        headers={"User-Agent": "bike-computer-map-convert/1.0"})
    with urllib.request.urlopen(req, timeout=330) as resp:
        return resp.read()


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    f = sub.add_parser("fetch", help="download an area from Overpass as JSON")
    area = f.add_mutually_exclusive_group(required=True)
    area.add_argument("--center", help="lat,lon (used with --radius-km)")
    area.add_argument("--bbox", help="S,W,N,E in degrees")
    f.add_argument("--radius-km", type=float, default=10.0)
    f.add_argument("-o", "--output", required=True)
    c = sub.add_parser("convert", help="Overpass JSON -> .bkm tiles")
    c.add_argument("input")
    c.add_argument("-o", "--output", default=os.path.join("tools", "out", "map"))
    c.add_argument("--tol", type=float, default=DEFAULT_TOLERANCE,
                   help="Douglas-Peucker tolerance in tile units (1 unit ~ 0.19 m)")
    args = ap.parse_args(argv)

    if args.cmd == "fetch":
        if args.bbox:
            bbox = parse_bbox(args.bbox)
        else:
            lat, lon = (float(v) for v in args.center.split(","))
            bbox = bbox_around(lat, lon, args.radius_km)
        print("bbox S,W,N,E =", bbox, file=sys.stderr)
        body = fetch_overpass(build_query(bbox))
        os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)
        with open(args.output, "wb") as out:
            out.write(body)
        print(f"{len(body)} bytes -> {args.output}", file=sys.stderr)
        return 0

    with open(args.input, "r", encoding="utf-8") as src:
        data = json.load(src)
    clean_output(args.output)
    stats = convert(data, args.output, tolerance=args.tol)
    print(json.dumps(stats))
    return 0


if __name__ == "__main__":
    sys.exit(main())
