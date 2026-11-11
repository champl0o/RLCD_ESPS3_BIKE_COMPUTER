"""Map converter tests. Run: python3 -I -m unittest discover -s tools -p 'test_map_convert.py'"""
import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import map_convert as mc  # noqa: E402


def way(tags, pts, wid=1):
    return {"type": "way", "id": wid, "tags": tags,
            "geometry": [{"lat": la, "lon": lo} for la, lo in pts]}


class Projection(unittest.TestCase):
    def test_lviv_matches_c_reference(self):
        # same numbers as in test_geo_world_matches_tile_index_and_reference
        wx, wy = mc.world_units(49.8397, 24.0297)
        self.assertAlmostEqual(wx, 76067785.49589, places=2)
        self.assertAlmostEqual(wy, 45612063.11982, places=2)
        self.assertEqual((int(wx // 4096), int(wy // 4096)), (18571, 11135))


class Simplify(unittest.TestCase):
    def test_collinear_points_collapse_to_endpoints(self):
        pts = [(0, 0), (10, 0), (20, 0), (30, 0)]
        self.assertEqual(mc.simplify(pts, 1.0), [(0, 0), (30, 0)])

    def test_corner_beyond_tolerance_is_kept(self):
        pts = [(0, 0), (50, 50), (100, 0)]
        self.assertEqual(mc.simplify(pts, 10.0), pts)

    def test_deviation_within_tolerance_is_dropped(self):
        pts = [(0, 0), (50, 3), (100, 0)]
        self.assertEqual(mc.simplify(pts, 10.0), [(0, 0), (100, 0)])

    def test_closed_ring_does_not_collapse(self):
        ring = [(0, 0), (100, 0), (100, 100), (0, 100), (0, 0)]
        out = mc.simplify(ring, 10.0)
        self.assertGreaterEqual(len(out), 4)
        self.assertEqual(out[0], out[-1])

    def test_short_inputs_unchanged(self):
        self.assertEqual(mc.simplify([(1, 1), (2, 2)], 5.0), [(1, 1), (2, 2)])
        self.assertEqual(mc.simplify([], 5.0), [])

    def test_long_line_does_not_hit_recursion_limit(self):
        pts = [(i, (i % 2) * 100) for i in range(2000)]  # > recursion limit (1000)
        out = mc.simplify(pts, 1.0)
        self.assertEqual(out[0], pts[0])
        self.assertEqual(out[-1], pts[-1])


class Classify(unittest.TestCase):
    def test_classes(self):
        c = mc.classify
        self.assertEqual(c({"highway": "primary"}), mc.CLASS_HIGHWAY)
        self.assertEqual(c({"highway": "motorway_link"}), mc.CLASS_HIGHWAY)
        self.assertEqual(c({"highway": "residential"}), mc.CLASS_STREET)
        self.assertEqual(c({"highway": "tertiary"}), mc.CLASS_STREET)
        self.assertEqual(c({"highway": "cycleway"}), mc.CLASS_BIKE)
        self.assertEqual(c({"highway": "path", "bicycle": "designated"}), mc.CLASS_BIKE)
        self.assertEqual(c({"waterway": "river"}), mc.CLASS_WATER)
        self.assertEqual(c({"natural": "water"}), mc.CLASS_WATER)
        self.assertEqual(c({"railway": "rail"}), mc.CLASS_RAIL)

    def test_everything_else_is_dropped(self):
        c = mc.classify
        self.assertIsNone(c({"highway": "footway"}))
        self.assertIsNone(c({"highway": "path"}))
        self.assertIsNone(c({"highway": "service"}))
        self.assertIsNone(c({"railway": "tram"}))
        self.assertIsNone(c({"waterway": "stream"}))
        self.assertIsNone(c({"building": "yes"}))
        self.assertIsNone(c({}))
        self.assertIsNone(c(None))


class Encoding(unittest.TestCase):
    def test_golden_bytes_match_c_test(self):
        # test_bkm_is_little_endian_and_signed: absolute points (258,-2),(257,-1)
        raw = mc.encode_tile([(mc.CLASS_BIKE, [(258, -2), (257, -1)])])
        self.assertEqual(raw, bytes([ord("B"), ord("K"), ord("M"), ord("1"), 1, 1, 0, 0,
                                     mc.CLASS_BIKE, 2, 0,
                                     0x02, 0x01, 0xFE, 0xFF,
                                     0xFF, 0xFF, 0x01, 0x00]))

    def test_roundtrip(self):
        feats = [(mc.CLASS_HIGHWAY, [(100, 100), (150, 80), (150, 4000)]),
                 (mc.CLASS_WATER, [(-256, 5), (4351, 4351)])]
        self.assertEqual(mc.decode_tile(mc.encode_tile(feats)), feats)

    def test_empty_tile(self):
        self.assertEqual(mc.decode_tile(mc.encode_tile([])), [])

    def test_encoder_rejects_invalid_features(self):
        with self.assertRaises(ValueError):
            mc.encode_tile([(0, [(1, 1), (2, 2)])])
        with self.assertRaises(ValueError):
            mc.encode_tile([(6, [(1, 1), (2, 2)])])
        with self.assertRaises(ValueError):
            mc.encode_tile([(mc.CLASS_STREET, [(1, 1)])])
        with self.assertRaises(ValueError):
            mc.encode_tile([(mc.CLASS_STREET, [(1, 1), (40000, 1)])])
        with self.assertRaises(ValueError):
            mc.encode_tile([(mc.CLASS_STREET, [(1, 1), (2, 2)])] * 65536)

    def test_decoder_rejects_corrupt_input(self):
        good = mc.encode_tile([(mc.CLASS_STREET, [(1, 1), (5, 5)])])
        for bad in (b"", good[:-1], good + b"\0", b"XKM1" + good[4:],
                    good[:4] + b"\x02" + good[5:]):
            with self.assertRaises(ValueError):
                mc.decode_tile(bad)


class Tiling(unittest.TestCase):
    T = mc.TILE_UNITS
    M = mc.TILE_MARGIN

    def test_segment_crossing_border_lands_in_both_tiles_clipped_at_margin(self):
        out = mc.split_to_tiles([(100, 100), (5000, 100)])
        self.assertEqual(out[(0, 0)], [[(100, 100), (self.T + self.M, 100)]])
        self.assertEqual(out[(1, 0)], [[(-self.M, 100), (5000 - self.T, 100)]])
        self.assertEqual(set(out), {(0, 0), (1, 0)})

    def test_segment_inside_tile_also_enters_neighbour_within_margin(self):
        out = mc.split_to_tiles([(4000, 500), (4050, 500)])
        self.assertEqual(out[(0, 0)], [[(4000, 500), (4050, 500)]])
        self.assertEqual(out[(1, 0)], [[(4000 - self.T, 500), (4050 - self.T, 500)]])

    def test_segment_far_from_edges_stays_in_one_tile(self):
        out = mc.split_to_tiles([(2000, 2000), (2100, 2100)])
        self.assertEqual(list(out), [(0, 0)])

    def test_continuous_polyline_stays_one_piece(self):
        out = mc.split_to_tiles([(1000, 1000), (1500, 1200), (2000, 1000)])
        self.assertEqual(out[(0, 0)], [[(1000, 1000), (1500, 1200), (2000, 1000)]])

    def test_polyline_leaving_and_reentering_tile_splits(self):
        pts = [(1000, 1000), (1000, 6000), (1500, 6000), (1500, 1000)]
        out = mc.split_to_tiles(pts)
        self.assertEqual(len(out[(0, 0)]), 2)

    def test_negative_world_offset_and_degenerate_points(self):
        out = mc.split_to_tiles([(2000, 2000), (2000, 2000)])
        self.assertEqual(out, {})  # zero length is dropped

    def test_pieces_are_never_longer_than_limit_and_share_endpoints(self):
        pts = [(1000 + i % 2, 100 + i) for i in range(2500)]
        pts = [(x, y) for x, y in pts]
        pieces = mc.chunk(pts, 1000)
        self.assertTrue(all(2 <= len(p) <= 1000 for p in pieces))
        for a, b in zip(pieces, pieces[1:]):
            self.assertEqual(a[-1], b[0])
        self.assertEqual(sum(len(p) - 1 for p in pieces), len(pts) - 1)


class EndToEnd(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)

    def run_convert(self, elements, **kw):
        data = {"elements": elements}
        return mc.convert(data, self.tmp.name, **kw)

    def read_tile(self, z, x, y):
        with open(os.path.join(self.tmp.name, str(z), str(x), f"{y}.bkm"), "rb") as f:
            return mc.decode_tile(f.read())

    def test_street_becomes_tile_with_expected_class(self):
        lat, lon = 49.8397, 24.0297
        el = way({"highway": "residential"}, [(lat, lon), (lat, lon + 0.0005)])
        stats = self.run_convert([el], tolerance=1.0)
        self.assertEqual(stats["ways"], 1)
        feats = self.read_tile(15, 18571, 11135)
        self.assertEqual(len(feats), 1)
        self.assertEqual(feats[0][0], mc.CLASS_STREET)
        (x0, y0), (x1, y1) = feats[0][1][0], feats[0][1][-1]
        self.assertEqual(y0, y1)
        # 0.0005 deg of longitude at 49.84 deg is about 36 m, about 187 units
        self.assertAlmostEqual(x1 - x0, 187, delta=2)

    def test_unknown_and_malformed_elements_are_ignored(self):
        lat, lon = 49.8397, 24.0297
        els = [
            way({"highway": "footway"}, [(lat, lon), (lat, lon + 0.001)], 1),
            {"type": "node", "id": 2, "lat": lat, "lon": lon},
            {"type": "way", "id": 3, "tags": {"highway": "primary"}},          # no geometry
            way({"highway": "primary"}, [(lat, lon)], 4),                      # single point
            way({"highway": "primary"}, [(95.0, lon), (96.0, lon)], 5),        # outside Mercator
            {"type": "way", "id": 6, "tags": {"highway": "primary"},
             "geometry": [{"lat": "x", "lon": lon}, {"lat": lat}]},            # garbage
            {"type": "way", "id": 7, "tags": "oops", "geometry": []},
            "not a dict",
        ]
        stats = self.run_convert(els)
        self.assertEqual(stats["ways"], 0)
        self.assertEqual(stats["tiles"], 0)

    def test_tiny_features_below_tolerance_are_dropped(self):
        lat, lon = 49.8397, 24.0297
        tiny = way({"highway": "residential"}, [(lat, lon), (lat, lon + 0.00001)])
        self.assertEqual(self.run_convert([tiny], tolerance=10.0)["ways"], 0)

    def test_output_is_deterministic(self):
        lat, lon = 49.8397, 24.0297
        els = [way({"highway": "primary"}, [(lat, lon), (lat + 0.001, lon + 0.002),
                                            (lat + 0.002, lon + 0.002)], 1),
               way({"waterway": "river"}, [(lat, lon + 0.01), (lat + 0.01, lon + 0.012)], 2)]
        self.run_convert(els)
        first = {}
        for root, _, files in os.walk(self.tmp.name):
            for f in files:
                p = os.path.join(root, f)
                with open(p, "rb") as fh:
                    first[os.path.relpath(p, self.tmp.name)] = fh.read()
        self.run_convert(els)
        for rel, data in first.items():
            with open(os.path.join(self.tmp.name, rel), "rb") as fh:
                self.assertEqual(fh.read(), data)
        self.assertGreater(len(first), 0)

    def test_closed_water_ring_keeps_its_shape(self):
        lat, lon = 49.8397, 24.0297
        d = 0.001
        ring = [(lat, lon), (lat, lon + d), (lat + d, lon + d), (lat + d, lon), (lat, lon)]
        self.run_convert([way({"natural": "water"}, ring)], tolerance=1.0)
        feats = self.read_tile(15, 18571, 11135)
        self.assertEqual(len(feats), 1)
        pts = feats[0][1]
        self.assertEqual(pts[0], pts[-1])
        self.assertGreaterEqual(len(pts), 5)


class Housekeeping(unittest.TestCase):
    def test_bbox_is_centred_and_scaled_by_latitude(self):
        s, w, n, e = mc.bbox_around(49.8397, 24.0297, 10.0)
        self.assertAlmostEqual((s + n) / 2, 49.8397, places=6)
        self.assertAlmostEqual((w + e) / 2, 24.0297, places=6)
        self.assertAlmostEqual(n - s, 20 / 111.32, places=3)
        self.assertGreater(e - w, n - s)  # at 50 deg a degree of longitude is "shorter"

    def test_parse_bbox_accepts_valid_and_rejects_bad_input(self):
        self.assertEqual(mc.parse_bbox("49.65,23.79,49.93,24.17"), (49.65, 23.79, 49.93, 24.17))
        for bad in ("1,2,3", "a,b,c,d", "50,24,49,25", "49,25,50,24", "95,0,96,1",
                    "nan,0,1,1", "49,23,50,181"):
            with self.assertRaises(ValueError):
                mc.parse_bbox(bad)

    def test_query_contains_every_class_and_bbox(self):
        q = mc.build_query((49.7, 23.9, 49.9, 24.1))
        for needle in ("highway", "cycleway", "waterway", "natural", "railway",
                       "49.7,23.9,49.9,24.1", "out geom"):
            self.assertIn(needle, q)

    def test_clean_removes_only_bkm_tiles(self):
        with tempfile.TemporaryDirectory() as d:
            tile = os.path.join(d, "15", "1", "2.bkm")
            foreign = os.path.join(d, "15", "1", "notes.txt")
            os.makedirs(os.path.dirname(tile))
            for path in (tile, foreign):
                with open(path, "wb") as fh:
                    fh.write(b"x")
            mc.clean_output(d, 15)
            self.assertFalse(os.path.exists(tile))
            self.assertTrue(os.path.exists(foreign))


if __name__ == "__main__":
    unittest.main()
