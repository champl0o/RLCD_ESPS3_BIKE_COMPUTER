#include "../../components/map/bkm.c"
#include "../../components/map/map_clip.c"
#include "../../components/map/map_geo.c"
#include "../../components/map/map_render.c"
#include "../../tools/host/map_bitmap.h"
#include "unity.h"
#include <stddef.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/* Unity is built without double support: own check for double precision */
#define ASSERT_NEAR(tol, exp, act) \
  TEST_ASSERT_TRUE_MESSAGE(fabs((double)(exp) - (double)(act)) <= (tol), \
                           "double outside tolerance: " #act)

void setUp(void) {}
void tearDown(void) {}

/* ---------- helpers: byte-by-byte tile builder for tests ---------- */

typedef struct {
  uint8_t b[4096];
  size_t n;
} tbuf_t;

static void w8(tbuf_t *t, unsigned v) { t->b[t->n++] = (uint8_t)v; }
static void w16(tbuf_t *t, unsigned v) {
  w8(t, v & 0xFF);
  w8(t, (v >> 8) & 0xFF);
}
static void w16s(tbuf_t *t, int v) { w16(t, (unsigned)(v & 0xFFFF)); }

static void t_header(tbuf_t *t, unsigned count) {
  t->n = 0;
  w8(t, 'B'); w8(t, 'K'); w8(t, 'M'); w8(t, '1');
  w8(t, 1);
  w16(t, count);
  w8(t, 0);
}
static void t_feature(tbuf_t *t, unsigned cls, unsigned npts) {
  w8(t, cls);
  w16(t, npts);
}
static void t_delta(tbuf_t *t, int dx, int dy) {
  w16s(t, dx);
  w16s(t, dy);
}

/* ---------- bkm: format and reader ---------- */

void test_bkm_open_empty_tile(void) {
  tbuf_t t;
  t_header(&t, 0);
  bkm_reader_t r;
  bkm_feature_t f;
  TEST_ASSERT_EQUAL(BKM_OK, bkm_open(&r, t.b, t.n));
  TEST_ASSERT_FALSE(bkm_next(&r, &f));
}

void test_bkm_decodes_deltas_to_absolute_points(void) {
  tbuf_t t;
  t_header(&t, 1);
  t_feature(&t, BKM_CLASS_STREET, 3);
  t_delta(&t, 10, 20);
  t_delta(&t, 5, -3);
  t_delta(&t, -15, 0);
  bkm_reader_t r;
  bkm_feature_t f;
  int16_t x, y;
  TEST_ASSERT_EQUAL(BKM_OK, bkm_open(&r, t.b, t.n));
  TEST_ASSERT_TRUE(bkm_next(&r, &f));
  TEST_ASSERT_EQUAL_UINT8(BKM_CLASS_STREET, f.cls);
  TEST_ASSERT_EQUAL_UINT16(3, f.npts);
  TEST_ASSERT_TRUE(bkm_feature_next_point(&f, &x, &y));
  TEST_ASSERT_EQUAL_INT16(10, x);
  TEST_ASSERT_EQUAL_INT16(20, y);
  TEST_ASSERT_TRUE(bkm_feature_next_point(&f, &x, &y));
  TEST_ASSERT_EQUAL_INT16(15, x);
  TEST_ASSERT_EQUAL_INT16(17, y);
  TEST_ASSERT_TRUE(bkm_feature_next_point(&f, &x, &y));
  TEST_ASSERT_EQUAL_INT16(0, x);
  TEST_ASSERT_EQUAL_INT16(17, y);
  TEST_ASSERT_FALSE(bkm_feature_next_point(&f, &x, &y));
  TEST_ASSERT_FALSE(bkm_next(&r, &f));
}

void test_bkm_is_little_endian_and_signed(void) {
  /* first point dx=0x0102, dy=-2 (FE FF); second dx=-1, dy=+1 */
  static const uint8_t raw[] = {'B', 'K', 'M', '1', 1, 1, 0, 0,
                                BKM_CLASS_BIKE, 2, 0,
                                0x02, 0x01, 0xFE, 0xFF,
                                0xFF, 0xFF, 0x01, 0x00};
  bkm_reader_t r;
  bkm_feature_t f;
  int16_t x, y;
  TEST_ASSERT_EQUAL(BKM_OK, bkm_open(&r, raw, sizeof raw));
  TEST_ASSERT_TRUE(bkm_next(&r, &f));
  TEST_ASSERT_TRUE(bkm_feature_next_point(&f, &x, &y));
  TEST_ASSERT_EQUAL_INT16(0x0102, x);
  TEST_ASSERT_EQUAL_INT16(-2, y);
  TEST_ASSERT_TRUE(bkm_feature_next_point(&f, &x, &y));
  TEST_ASSERT_EQUAL_INT16(0x0101, x);
  TEST_ASSERT_EQUAL_INT16(-1, y);
}

void test_bkm_iterates_all_features_in_order(void) {
  tbuf_t t;
  t_header(&t, 2);
  t_feature(&t, BKM_CLASS_WATER, 2);
  t_delta(&t, 1, 1);
  t_delta(&t, 1, 1);
  t_feature(&t, BKM_CLASS_RAIL, 2);
  t_delta(&t, 7, 7);
  t_delta(&t, 1, 1);
  bkm_reader_t r;
  bkm_feature_t f;
  TEST_ASSERT_EQUAL(BKM_OK, bkm_open(&r, t.b, t.n));
  TEST_ASSERT_TRUE(bkm_next(&r, &f));
  TEST_ASSERT_EQUAL_UINT8(BKM_CLASS_WATER, f.cls);
  TEST_ASSERT_TRUE(bkm_next(&r, &f));
  TEST_ASSERT_EQUAL_UINT8(BKM_CLASS_RAIL, f.cls);
  int16_t x, y;
  TEST_ASSERT_TRUE(bkm_feature_next_point(&f, &x, &y));
  TEST_ASSERT_EQUAL_INT16(7, x);
  TEST_ASSERT_FALSE(bkm_next(&r, &f));
}

void test_bkm_open_rejects_null_arguments(void) {
  tbuf_t t;
  t_header(&t, 0);
  bkm_reader_t r;
  TEST_ASSERT_EQUAL(BKM_ERR_ARG, bkm_open(NULL, t.b, t.n));
  TEST_ASSERT_EQUAL(BKM_ERR_ARG, bkm_open(&r, NULL, t.n));
}

void test_bkm_rejects_short_header(void) {
  tbuf_t t;
  t_header(&t, 0);
  bkm_reader_t r;
  TEST_ASSERT_EQUAL(BKM_ERR_TRUNCATED, bkm_open(&r, t.b, 7));
  TEST_ASSERT_EQUAL(BKM_ERR_TRUNCATED, bkm_open(&r, t.b, 0));
}

void test_bkm_rejects_bad_magic_and_version(void) {
  tbuf_t t;
  bkm_reader_t r;
  t_header(&t, 0);
  t.b[3] = '2';
  TEST_ASSERT_EQUAL(BKM_ERR_MAGIC, bkm_open(&r, t.b, t.n));
  t_header(&t, 0);
  t.b[4] = 2;
  TEST_ASSERT_EQUAL(BKM_ERR_VERSION, bkm_open(&r, t.b, t.n));
}

void test_bkm_rejects_unknown_class(void) {
  tbuf_t t;
  bkm_reader_t r;
  for (unsigned cls = 0; cls < 256; cls++) {
    t_header(&t, 1);
    t_feature(&t, cls, 2);
    t_delta(&t, 1, 1);
    t_delta(&t, 1, 1);
    bool valid = cls >= BKM_CLASS_HIGHWAY && cls <= BKM_CLASS_RAIL;
    TEST_ASSERT_EQUAL_MESSAGE(valid ? BKM_OK : BKM_ERR_CLASS,
                              bkm_open(&r, t.b, t.n), "class");
  }
}

void test_bkm_rejects_feature_with_fewer_than_two_points(void) {
  tbuf_t t;
  bkm_reader_t r;
  t_header(&t, 1);
  t_feature(&t, BKM_CLASS_STREET, 1);
  t_delta(&t, 1, 1);
  TEST_ASSERT_EQUAL(BKM_ERR_POINTS, bkm_open(&r, t.b, t.n));
  t_header(&t, 1);
  t_feature(&t, BKM_CLASS_STREET, 0);
  TEST_ASSERT_EQUAL(BKM_ERR_POINTS, bkm_open(&r, t.b, t.n));
}

void test_bkm_rejects_point_data_past_end_of_buffer(void) {
  tbuf_t t;
  bkm_reader_t r;
  t_header(&t, 1);
  t_feature(&t, BKM_CLASS_STREET, 65535); /* promises 256 KB, no data follows */
  t_delta(&t, 1, 1);
  TEST_ASSERT_EQUAL(BKM_ERR_TRUNCATED, bkm_open(&r, t.b, t.n));
}

void test_bkm_rejects_count_larger_than_content(void) {
  tbuf_t t;
  bkm_reader_t r;
  t_header(&t, 65535);
  t_feature(&t, BKM_CLASS_STREET, 2);
  t_delta(&t, 1, 1);
  t_delta(&t, 1, 1);
  TEST_ASSERT_EQUAL(BKM_ERR_TRUNCATED, bkm_open(&r, t.b, t.n));
}

void test_bkm_rejects_trailing_bytes(void) {
  tbuf_t t;
  bkm_reader_t r;
  t_header(&t, 1);
  t_feature(&t, BKM_CLASS_STREET, 2);
  t_delta(&t, 1, 1);
  t_delta(&t, 1, 1);
  w8(&t, 0); /* extra byte */
  TEST_ASSERT_EQUAL(BKM_ERR_TRAILING, bkm_open(&r, t.b, t.n));
}

void test_bkm_rejects_count_smaller_than_content(void) {
  tbuf_t t;
  bkm_reader_t r;
  t_header(&t, 1);
  for (int i = 0; i < 2; i++) {
    t_feature(&t, BKM_CLASS_STREET, 2);
    t_delta(&t, 1, 1);
    t_delta(&t, 1, 1);
  }
  TEST_ASSERT_EQUAL(BKM_ERR_TRAILING, bkm_open(&r, t.b, t.n));
}

void test_bkm_rejects_absolute_coordinate_outside_int16(void) {
  tbuf_t t;
  bkm_reader_t r;
  t_header(&t, 1);
  t_feature(&t, BKM_CLASS_STREET, 2);
  t_delta(&t, 30000, 0);
  t_delta(&t, 30000, 0); /* x = 60000 */
  TEST_ASSERT_EQUAL(BKM_ERR_RANGE, bkm_open(&r, t.b, t.n));
  t_header(&t, 1);
  t_feature(&t, BKM_CLASS_STREET, 2);
  t_delta(&t, 0, -30000);
  t_delta(&t, 0, -30000); /* y = -60000 */
  TEST_ASSERT_EQUAL(BKM_ERR_RANGE, bkm_open(&r, t.b, t.n));
}

void test_bkm_accepts_exact_int16_limits(void) {
  tbuf_t t;
  bkm_reader_t r;
  t_header(&t, 1);
  t_feature(&t, BKM_CLASS_STREET, 2);
  t_delta(&t, 32767, -32768);
  t_delta(&t, 0, 0);
  TEST_ASSERT_EQUAL(BKM_OK, bkm_open(&r, t.b, t.n));
}

void test_bkm_every_truncation_of_valid_tile_is_rejected(void) {
  tbuf_t t;
  bkm_reader_t r;
  t_header(&t, 2);
  t_feature(&t, BKM_CLASS_HIGHWAY, 3);
  t_delta(&t, 100, 100);
  t_delta(&t, 50, -20);
  t_delta(&t, 50, 20);
  t_feature(&t, BKM_CLASS_WATER, 2);
  t_delta(&t, 0, 0);
  t_delta(&t, 9, 9);
  TEST_ASSERT_EQUAL(BKM_OK, bkm_open(&r, t.b, t.n));
  for (size_t len = 0; len < t.n; len++) {
    TEST_ASSERT_NOT_EQUAL_MESSAGE(BKM_OK, bkm_open(&r, t.b, len), "prefix");
  }
}

void test_bkm_corrupted_bytes_never_overrun(void) {
  /* each byte in turn becomes 0xFF/0x00/0x80: open either refuses, or
     returns a tile that reads exactly to the end without leaving the buffer */
  tbuf_t t;
  t_header(&t, 2);
  t_feature(&t, BKM_CLASS_HIGHWAY, 3);
  t_delta(&t, 100, 100);
  t_delta(&t, 50, -20);
  t_delta(&t, 50, 20);
  t_feature(&t, BKM_CLASS_WATER, 2);
  t_delta(&t, 0, 0);
  t_delta(&t, 9, 9);
  static const uint8_t vals[] = {0xFF, 0x00, 0x80};
  for (size_t i = 0; i < t.n; i++) {
    for (size_t v = 0; v < sizeof vals; v++) {
      uint8_t copy[sizeof t.b];
      memcpy(copy, t.b, t.n);
      copy[i] = vals[v];
      bkm_reader_t r;
      if (bkm_open(&r, copy, t.n) != BKM_OK) continue;
      bkm_feature_t f;
      size_t feats = 0, pts = 0;
      int16_t x, y;
      while (bkm_next(&r, &f)) {
        feats++;
        while (bkm_feature_next_point(&f, &x, &y)) pts++;
      }
      TEST_ASSERT_LESS_OR_EQUAL_size_t(t.n / 3, feats);
      TEST_ASSERT_LESS_OR_EQUAL_size_t(t.n / 4, pts);
    }
  }
}

/* ---------- map_geo: tile index and projection ---------- */

void test_geo_tile_index_known_points(void) {
  int32_t x, y;
  TEST_ASSERT_TRUE(map_tile_index(49.8397, 24.0297, 15, &x, &y)); /* Lviv */
  TEST_ASSERT_EQUAL_INT32(18571, x);
  TEST_ASSERT_EQUAL_INT32(11135, y);
  TEST_ASSERT_TRUE(map_tile_index(0.0, 0.0, 1, &x, &y));
  TEST_ASSERT_EQUAL_INT32(1, x);
  TEST_ASSERT_EQUAL_INT32(1, y);
  TEST_ASSERT_TRUE(map_tile_index(51.4779, 0.0, 10, &x, &y)); /* Greenwich */
  TEST_ASSERT_EQUAL_INT32(512, x);
  TEST_ASSERT_EQUAL_INT32(340, y);
  TEST_ASSERT_TRUE(map_tile_index(-33.8688, 151.2093, 12, &x, &y)); /* Sydney */
  TEST_ASSERT_EQUAL_INT32(3768, x);
  TEST_ASSERT_EQUAL_INT32(2457, y);
}

void test_geo_tile_index_edges_stay_inside_grid(void) {
  int32_t x, y;
  TEST_ASSERT_TRUE(map_tile_index(0.0, 180.0, 3, &x, &y));
  TEST_ASSERT_EQUAL_INT32(7, x); /* not 8 */
  TEST_ASSERT_TRUE(map_tile_index(0.0, -180.0, 3, &x, &y));
  TEST_ASSERT_EQUAL_INT32(0, x);
  TEST_ASSERT_TRUE(map_tile_index(85.0, 0.0, 3, &x, &y));
  TEST_ASSERT_EQUAL_INT32(0, y);
  TEST_ASSERT_TRUE(map_tile_index(-85.0, -179.9999, 3, &x, &y));
  TEST_ASSERT_EQUAL_INT32(7, y);
}

void test_geo_rejects_invalid_coordinates(void) {
  int32_t x, y;
  double wx, wy;
  TEST_ASSERT_FALSE(map_tile_index(89.0, 0.0, 15, &x, &y)); /* outside Mercator */
  TEST_ASSERT_FALSE(map_tile_index(-89.0, 0.0, 15, &x, &y));
  TEST_ASSERT_FALSE(map_tile_index(0.0, 181.0, 15, &x, &y));
  TEST_ASSERT_FALSE(map_tile_index(NAN, 0.0, 15, &x, &y));
  TEST_ASSERT_FALSE(map_tile_index(0.0, INFINITY, 15, &x, &y));
  TEST_ASSERT_FALSE(map_tile_index(0.0, 0.0, 0, &x, &y));
  TEST_ASSERT_FALSE(map_tile_index(0.0, 0.0, 23, &x, &y));
  TEST_ASSERT_FALSE(map_world(NAN, 0.0, 15, &wx, &wy));
  TEST_ASSERT_FALSE(map_world(90.0, 0.0, 15, &wx, &wy));
}

void test_geo_world_matches_tile_index_and_reference(void) {
  double wx, wy;
  int32_t tx, ty;
  TEST_ASSERT_TRUE(map_world(49.8397, 24.0297, 15, &wx, &wy));
  ASSERT_NEAR(0.01, 76067785.49589, wx);
  ASSERT_NEAR(0.01, 45612063.11982, wy);
  TEST_ASSERT_TRUE(map_tile_index(49.8397, 24.0297, 15, &tx, &ty));
  TEST_ASSERT_EQUAL_INT32(tx, (int32_t)(wx / BKM_TILE_UNITS));
  TEST_ASSERT_EQUAL_INT32(ty, (int32_t)(wy / BKM_TILE_UNITS));
}

void test_geo_latlon_roundtrip(void) {
  double wx, wy, lat, lon;
  TEST_ASSERT_TRUE(map_world(49.8397, 24.0297, 15, &wx, &wy));
  TEST_ASSERT_TRUE(map_latlon(wx, wy, 15, &lat, &lon));
  ASSERT_NEAR(1e-7, 49.8397, lat);
  ASSERT_NEAR(1e-7, 24.0297, lon);
  TEST_ASSERT_FALSE(map_latlon(-1.0, 0.0, 15, &lat, &lon));
  TEST_ASSERT_FALSE(map_latlon(NAN, 0.0, 15, &lat, &lon));
}

void test_geo_meters_per_unit(void) {
  /* 40075016,686 * cos(lat) / (2^15 * 4096) */
  ASSERT_NEAR(1e-5, 0.192564, map_meters_per_unit(49.8397, 15));
  ASSERT_NEAR(1e-5, 0.298582, map_meters_per_unit(0.0, 15));
}

/* ---------- map_clip: Cohen-Sutherland ---------- */

static bool clip(float x0, float y0, float x1, float y1, float out[4]) {
  bool vis = map_clip_line(&x0, &y0, &x1, &y1, 0.f, 0.f, 9.f, 9.f);
  out[0] = x0; out[1] = y0; out[2] = x1; out[3] = y1;
  return vis;
}

void test_clip_inside_segment_is_unchanged(void) {
  float o[4];
  TEST_ASSERT_TRUE(clip(1, 2, 8, 7, o));
  TEST_ASSERT_EQUAL_FLOAT(1, o[0]);
  TEST_ASSERT_EQUAL_FLOAT(2, o[1]);
  TEST_ASSERT_EQUAL_FLOAT(8, o[2]);
  TEST_ASSERT_EQUAL_FLOAT(7, o[3]);
}

void test_clip_segment_fully_outside_is_rejected(void) {
  float o[4];
  TEST_ASSERT_FALSE(clip(-5, 1, -1, 8, o));  /* left */
  TEST_ASSERT_FALSE(clip(10, 1, 20, 8, o));  /* right */
  TEST_ASSERT_FALSE(clip(1, -5, 8, -1, o));  /* top */
  TEST_ASSERT_FALSE(clip(1, 10, 8, 20, o));  /* bottom */
}

void test_clip_crossing_segment_is_cut_at_edges(void) {
  float o[4];
  TEST_ASSERT_TRUE(clip(-10, 5, 10, 5, o));
  TEST_ASSERT_EQUAL_FLOAT(0, o[0]);
  TEST_ASSERT_EQUAL_FLOAT(5, o[1]);
  TEST_ASSERT_EQUAL_FLOAT(9, o[2]);
  TEST_ASSERT_EQUAL_FLOAT(5, o[3]);
  TEST_ASSERT_TRUE(clip(-5, -5, 15, 15, o)); /* diagonal through the corners */
  TEST_ASSERT_EQUAL_FLOAT(0, o[0]);
  TEST_ASSERT_EQUAL_FLOAT(0, o[1]);
  TEST_ASSERT_EQUAL_FLOAT(9, o[2]);
  TEST_ASSERT_EQUAL_FLOAT(9, o[3]);
  TEST_ASSERT_TRUE(clip(4, -3, 4, 30, o)); /* vertical */
  TEST_ASSERT_EQUAL_FLOAT(4, o[0]);
  TEST_ASSERT_EQUAL_FLOAT(0, o[1]);
  TEST_ASSERT_EQUAL_FLOAT(4, o[2]);
  TEST_ASSERT_EQUAL_FLOAT(9, o[3]);
}

void test_clip_rejects_segment_passing_outside_a_corner(void) {
  /* endpoints in different zones but AND of codes = 0: rejected only via intersections */
  float o[4];
  TEST_ASSERT_FALSE(clip(-5, 5, 5, 15, o));
}

void test_clip_keeps_one_sided_cut(void) {
  float o[4];
  TEST_ASSERT_TRUE(clip(5, 5, 20, 5, o));
  TEST_ASSERT_EQUAL_FLOAT(5, o[0]);
  TEST_ASSERT_EQUAL_FLOAT(9, o[2]);
}

void test_clip_is_symmetric_for_swapped_endpoints(void) {
  float a[4], b[4];
  TEST_ASSERT_TRUE(clip(-4, 2, 13, 11, a));
  TEST_ASSERT_TRUE(clip(13, 11, -4, 2, b));
  TEST_ASSERT_FLOAT_WITHIN(1e-4, a[0], b[2]);
  TEST_ASSERT_FLOAT_WITHIN(1e-4, a[1], b[3]);
  TEST_ASSERT_FLOAT_WITHIN(1e-4, a[2], b[0]);
  TEST_ASSERT_FLOAT_WITHIN(1e-4, a[3], b[1]);
}

void test_clip_boundary_and_degenerate_cases(void) {
  float o[4];
  TEST_ASSERT_TRUE(clip(0, 0, 9, 0, o)); /* lies on the edge */
  TEST_ASSERT_TRUE(clip(3, 3, 3, 3, o)); /* point inside */
  TEST_ASSERT_FALSE(clip(-3, 3, -3, 3, o)); /* point outside */
}

void test_clip_rejects_non_finite_and_survives_huge_values(void) {
  float o[4];
  TEST_ASSERT_FALSE(clip(NAN, 1, 5, 5, o));
  TEST_ASSERT_FALSE(clip(1, 1, INFINITY, 5, o));
  TEST_ASSERT_TRUE(clip(-1e30f, 5, 1e30f, 5, o));
  TEST_ASSERT_EQUAL_FLOAT(0, o[0]);
  TEST_ASSERT_EQUAL_FLOAT(9, o[2]);
}

/* ---------- map_render: tiles -> lines on the canvas ---------- */

#define SCREEN_W 300
#define SCREEN_H 400
#define TX0 18571 /* Lviv tile at z=15 */
#define TY0 11135

typedef struct { int x0, y0, x1, y1; } rec_line_t;
typedef struct {
  rec_line_t lines[512];
  int n;
} recorder_t;

static void rec_line(void *ctx, int x0, int y0, int x1, int y1) {
  recorder_t *r = ctx;
  if (r->n < 512) r->lines[r->n] = (rec_line_t){x0, y0, x1, y1};
  r->n++;
}

typedef struct {
  int32_t x, y;
  tbuf_t tile;
} stored_tile_t;

typedef struct {
  stored_tile_t tiles[4];
  int ntiles;
  int calls;
  int32_t min_x, min_y;
  int32_t asked_x[16], asked_y[16];
  size_t forced_len; /* !=0: the loader returns this length instead of the real one */
} src_ctx_t;

static size_t table_loader(void *ctx, int z, int32_t x, int32_t y, uint8_t *buf,
                           size_t cap) {
  src_ctx_t *c = ctx;
  TEST_ASSERT_EQUAL_INT(MAP_ZOOM, z);
  if (c->calls < 16) { c->asked_x[c->calls] = x; c->asked_y[c->calls] = y; }
  if (c->calls == 0 || x < c->min_x) c->min_x = x;
  if (c->calls == 0 || y < c->min_y) c->min_y = y;
  c->calls++;
  for (int i = 0; i < c->ntiles; i++) {
    if (c->tiles[i].x == x && c->tiles[i].y == y) {
      size_t n = c->tiles[i].tile.n;
      if (c->forced_len) return c->forced_len;
      if (n > cap) return 0;
      memcpy(buf, c->tiles[i].tile.b, n);
      return n;
    }
  }
  return 0;
}

static uint8_t g_tile_buf[8192];

static map_status_t render_with(const map_view_t *v, src_ctx_t *sc,
                                recorder_t *rec, map_stats_t *st) {
  map_tile_source_t src = {table_loader, sc, g_tile_buf, sizeof g_tile_buf};
  map_canvas_t cv = {rec, SCREEN_W, SCREEN_H, rec_line};
  rec->n = 0;
  return map_render(v, &src, &cv, st);
}

/* view centred on a world point (in units) with s pixels per unit */
static map_view_t view_at_world(double wx, double wy, double s) {
  double lat, lon;
  TEST_ASSERT_TRUE(map_latlon(wx, wy, MAP_ZOOM, &lat, &lon));
  map_view_t v = {lat, lon, (float)(map_meters_per_unit(lat, MAP_ZOOM) / s)};
  return v;
}

static double tile_center(int32_t t) { return (double)t * BKM_TILE_UNITS + 2048; }

static void tile_one_feature(src_ctx_t *c, int idx, int32_t x, int32_t y,
                             unsigned cls, int x0, int y0, int x1, int y1) {
  memset(&c->tiles[idx], 0, sizeof c->tiles[idx]);
  c->tiles[idx].x = x;
  c->tiles[idx].y = y;
  tbuf_t *t = &c->tiles[idx].tile;
  t_header(t, 1);
  t_feature(t, cls, 2);
  t_delta(t, x0, y0);
  t_delta(t, x1 - x0, y1 - y0);
  if (c->ntiles <= idx) c->ntiles = idx + 1;
}

void test_render_street_through_centre_is_one_clipped_line(void) {
  src_ctx_t sc = {0};
  recorder_t rec;
  map_stats_t st;
  tile_one_feature(&sc, 0, TX0, TY0, BKM_CLASS_STREET, 0, 2048, 4095, 2048);
  map_view_t v = view_at_world(tile_center(TX0), tile_center(TY0), 1.0);
  TEST_ASSERT_EQUAL(MAP_OK, render_with(&v, &sc, &rec, &st));
  TEST_ASSERT_EQUAL_INT(1, sc.calls);
  TEST_ASSERT_EQUAL_INT(1, rec.n);
  TEST_ASSERT_EQUAL_INT(0, rec.lines[0].x0);
  TEST_ASSERT_EQUAL_INT(200, rec.lines[0].y0);
  TEST_ASSERT_EQUAL_INT(SCREEN_W - 1, rec.lines[0].x1);
  TEST_ASSERT_EQUAL_INT(200, rec.lines[0].y1);
  TEST_ASSERT_EQUAL_UINT(1, st.tiles_drawn);
  TEST_ASSERT_EQUAL_UINT(1, st.lines);
}

void test_render_scale_halves_distances_from_centre(void) {
  src_ctx_t sc = {0};
  recorder_t rec;
  /* vertical line x=2148, y=1948..2148: 100 units right of the centre */
  tile_one_feature(&sc, 0, TX0, TY0, BKM_CLASS_STREET, 2148, 1948, 2148, 2148);
  map_view_t v = view_at_world(tile_center(TX0), tile_center(TY0), 0.5);
  TEST_ASSERT_EQUAL(MAP_OK, render_with(&v, &sc, &rec, NULL));
  TEST_ASSERT_EQUAL_INT(1, rec.n);
  TEST_ASSERT_EQUAL_INT(200, rec.lines[0].x0);
  TEST_ASSERT_EQUAL_INT(150, rec.lines[0].y0);
  TEST_ASSERT_EQUAL_INT(200, rec.lines[0].x1);
  TEST_ASSERT_EQUAL_INT(250, rec.lines[0].y1);
}

void test_render_loads_four_tiles_at_a_corner_and_draws_neighbour(void) {
  src_ctx_t sc = {0};
  recorder_t rec;
  /* vertical line in tile (TX0+1, TY0): x=10, y=4000..4095 */
  tile_one_feature(&sc, 0, TX0 + 1, TY0, BKM_CLASS_STREET, 10, 4000, 10, 4095);
  map_view_t v = view_at_world((double)(TX0 + 1) * BKM_TILE_UNITS,
                               (double)(TY0 + 1) * BKM_TILE_UNITS, 1.0);
  TEST_ASSERT_EQUAL(MAP_OK, render_with(&v, &sc, &rec, NULL));
  TEST_ASSERT_EQUAL_INT(4, sc.calls);
  TEST_ASSERT_EQUAL_INT32(TX0, sc.min_x);
  TEST_ASSERT_EQUAL_INT32(TY0, sc.min_y);
  TEST_ASSERT_EQUAL_INT(1, rec.n);
  TEST_ASSERT_EQUAL_INT(160, rec.lines[0].x0);
  TEST_ASSERT_EQUAL_INT(104, rec.lines[0].y0);
  TEST_ASSERT_EQUAL_INT(160, rec.lines[0].x1);
  TEST_ASSERT_EQUAL_INT(199, rec.lines[0].y1);
}

void test_render_missing_tiles_are_counted_not_fatal(void) {
  src_ctx_t sc = {0};
  recorder_t rec;
  map_stats_t st;
  map_view_t v = view_at_world((double)(TX0 + 1) * BKM_TILE_UNITS,
                               (double)(TY0 + 1) * BKM_TILE_UNITS, 1.0);
  TEST_ASSERT_EQUAL(MAP_OK, render_with(&v, &sc, &rec, &st));
  TEST_ASSERT_EQUAL_UINT(4, st.tiles_missing);
  TEST_ASSERT_EQUAL_UINT(0, st.tiles_drawn);
  TEST_ASSERT_EQUAL_INT(0, rec.n);
}

void test_render_corrupt_tile_is_skipped_others_still_drawn(void) {
  src_ctx_t sc = {0};
  recorder_t rec;
  map_stats_t st;
  tile_one_feature(&sc, 0, TX0, TY0, BKM_CLASS_STREET, 0, 4000, 0, 4095);
  tile_one_feature(&sc, 1, TX0 + 1, TY0, BKM_CLASS_STREET, 10, 4000, 10, 4095);
  sc.tiles[0].tile.n -= 1; /* truncated tile (TX0,TY0) */
  map_view_t v = view_at_world((double)(TX0 + 1) * BKM_TILE_UNITS,
                               (double)(TY0 + 1) * BKM_TILE_UNITS, 1.0);
  TEST_ASSERT_EQUAL(MAP_OK, render_with(&v, &sc, &rec, &st));
  TEST_ASSERT_EQUAL_UINT(1, st.tiles_corrupt);
  TEST_ASSERT_EQUAL_UINT(1, st.tiles_drawn);
  TEST_ASSERT_EQUAL_UINT(2, st.tiles_missing);
  TEST_ASSERT_EQUAL_INT(1, rec.n);
}

void test_render_loader_claiming_more_than_buffer_is_corrupt(void) {
  src_ctx_t sc = {0};
  recorder_t rec;
  map_stats_t st;
  tile_one_feature(&sc, 0, TX0, TY0, BKM_CLASS_STREET, 0, 2048, 4095, 2048);
  sc.forced_len = sizeof g_tile_buf + 1;
  map_view_t v = view_at_world(tile_center(TX0), tile_center(TY0), 1.0);
  TEST_ASSERT_EQUAL(MAP_OK, render_with(&v, &sc, &rec, &st));
  TEST_ASSERT_EQUAL_UINT(1, st.tiles_corrupt);
  TEST_ASSERT_EQUAL_INT(0, rec.n);
}

/* Loader that breaks the contract: writes a tile longer than cap into a larger array */
static size_t overrun_loader(void *ctx, int z, int32_t x, int32_t y, uint8_t *buf,
                             size_t cap) {
  (void)z; (void)x; (void)y; (void)cap;
  const tbuf_t *t = ctx;
  memcpy(buf, t->b, t->n);
  return t->n;
}

void test_render_valid_tile_longer_than_cap_is_still_rejected(void) {
  tbuf_t t;
  t_header(&t, 1);
  t_feature(&t, BKM_CLASS_STREET, 2);
  t_delta(&t, 0, 2048);
  t_delta(&t, 4095, 0);
  uint8_t big[64];
  recorder_t rec;
  map_stats_t st;
  map_view_t v = view_at_world(tile_center(TX0), tile_center(TY0), 1.0);
  map_tile_source_t src = {overrun_loader, &t, big, 10}; /* tile.n = 19 > 10 */
  map_canvas_t cv = {&rec, SCREEN_W, SCREEN_H, rec_line};
  rec.n = 0;
  TEST_ASSERT_TRUE(t.n > 10 && t.n <= sizeof big);
  TEST_ASSERT_EQUAL(MAP_OK, map_render(&v, &src, &cv, &st));
  TEST_ASSERT_EQUAL_UINT(1, st.tiles_corrupt);
  TEST_ASSERT_EQUAL_UINT(0, st.tiles_drawn);
  TEST_ASSERT_EQUAL_INT(0, rec.n);
}

void test_render_rejects_invalid_arguments(void) {
  src_ctx_t sc = {0};
  recorder_t rec;
  map_view_t good = view_at_world(tile_center(TX0), tile_center(TY0), 1.0);
  map_tile_source_t src = {table_loader, &sc, g_tile_buf, sizeof g_tile_buf};
  map_canvas_t cv = {&rec, SCREEN_W, SCREEN_H, rec_line};
  map_view_t bad;

  TEST_ASSERT_EQUAL(MAP_ERR_ARG, map_render(NULL, &src, &cv, NULL));
  TEST_ASSERT_EQUAL(MAP_ERR_ARG, map_render(&good, NULL, &cv, NULL));
  TEST_ASSERT_EQUAL(MAP_ERR_ARG, map_render(&good, &src, NULL, NULL));
  bad = good; bad.m_per_px = 0.f;
  TEST_ASSERT_EQUAL(MAP_ERR_ARG, map_render(&bad, &src, &cv, NULL));
  bad = good; bad.m_per_px = -1.f;
  TEST_ASSERT_EQUAL(MAP_ERR_ARG, map_render(&bad, &src, &cv, NULL));
  bad = good; bad.m_per_px = NAN;
  TEST_ASSERT_EQUAL(MAP_ERR_ARG, map_render(&bad, &src, &cv, NULL));
  bad = good; bad.lat = NAN;
  TEST_ASSERT_EQUAL(MAP_ERR_ARG, map_render(&bad, &src, &cv, NULL));
  bad = good; bad.lat = 89.0;
  TEST_ASSERT_EQUAL(MAP_ERR_ARG, map_render(&bad, &src, &cv, NULL));
  cv.width = 0;
  TEST_ASSERT_EQUAL(MAP_ERR_ARG, map_render(&good, &src, &cv, NULL));
  cv.width = SCREEN_W; cv.line = NULL;
  TEST_ASSERT_EQUAL(MAP_ERR_ARG, map_render(&good, &src, &cv, NULL));
  src.cap = 0;
  cv.line = rec_line;
  TEST_ASSERT_EQUAL(MAP_ERR_ARG, map_render(&good, &src, &cv, NULL));
  TEST_ASSERT_EQUAL_INT(0, sc.calls);
}

void test_render_refuses_view_needing_too_many_tiles(void) {
  src_ctx_t sc = {0};
  recorder_t rec;
  map_view_t v = view_at_world(tile_center(TX0), tile_center(TY0), 1.0);
  v.m_per_px = 1000.f; /* 300 km per screen */
  TEST_ASSERT_EQUAL(MAP_ERR_RANGE, render_with(&v, &sc, &rec, NULL));
  TEST_ASSERT_EQUAL_INT(0, sc.calls);
  v.m_per_px = 1e30f;
  TEST_ASSERT_EQUAL(MAP_ERR_RANGE, render_with(&v, &sc, &rec, NULL));
  TEST_ASSERT_EQUAL_INT(0, sc.calls);
}

void test_render_never_requests_tiles_outside_the_world(void) {
  src_ctx_t sc = {0};
  recorder_t rec;
  map_view_t v = {0.0, -179.99999, 4.0f};
  TEST_ASSERT_EQUAL(MAP_OK, render_with(&v, &sc, &rec, NULL));
  TEST_ASSERT_GREATER_THAN_INT(0, sc.calls);
  TEST_ASSERT_GREATER_OR_EQUAL_INT32(0, sc.min_x);
}

void test_render_highway_uses_wide_brush_street_does_not(void) {
  src_ctx_t sc = {0};
  recorder_t rec;
  map_view_t v = view_at_world(tile_center(TX0), tile_center(TY0), 1.0);
  tile_one_feature(&sc, 0, TX0, TY0, BKM_CLASS_STREET, 1948, 2048, 2148, 2048);
  render_with(&v, &sc, &rec, NULL);
  TEST_ASSERT_EQUAL_INT(1, rec.n);
  tile_one_feature(&sc, 0, TX0, TY0, BKM_CLASS_HIGHWAY, 1948, 2048, 2148, 2048);
  render_with(&v, &sc, &rec, NULL);
  TEST_ASSERT_EQUAL_INT(5, rec.n);
  int rows[3] = {0, 0, 0}; /* y = 199, 200, 201 */
  for (int i = 0; i < rec.n; i++) {
    int dy = rec.lines[i].y0 - 200;
    TEST_ASSERT_TRUE(dy >= -1 && dy <= 1);
    rows[dy + 1]++;
  }
  TEST_ASSERT_TRUE(rows[0] > 0 && rows[1] > 0 && rows[2] > 0);
}

void test_render_rail_is_dashed_8_on_4_off(void) {
  src_ctx_t sc = {0};
  recorder_t rec;
  map_view_t v = view_at_world(tile_center(TX0), tile_center(TY0), 1.0);
  /* sx = 100..146 at s=1 */
  tile_one_feature(&sc, 0, TX0, TY0, BKM_CLASS_RAIL, 1998, 2048, 2044, 2048);
  TEST_ASSERT_EQUAL(MAP_OK, render_with(&v, &sc, &rec, NULL));
  TEST_ASSERT_EQUAL_INT(4, rec.n);
  const int exp[4][2] = {{100, 108}, {112, 120}, {124, 132}, {136, 144}};
  for (int i = 0; i < 4; i++) {
    TEST_ASSERT_EQUAL_INT(exp[i][0], rec.lines[i].x0);
    TEST_ASSERT_EQUAL_INT(exp[i][1], rec.lines[i].x1);
    TEST_ASSERT_EQUAL_INT(200, rec.lines[i].y0);
  }
}

void test_render_dash_phase_continues_across_segments(void) {
  src_ctx_t sc = {0};
  recorder_t rec;
  map_view_t v = view_at_world(tile_center(TX0), tile_center(TY0), 1.0);
  memset(&sc.tiles[0], 0, sizeof sc.tiles[0]);
  sc.tiles[0].x = TX0;
  sc.tiles[0].y = TY0;
  sc.ntiles = 1;
  tbuf_t *t = &sc.tiles[0].tile;
  t_header(t, 1);
  t_feature(t, BKM_CLASS_RAIL, 3);
  t_delta(t, 1998, 2048); /* sx=100 */
  t_delta(t, 4, 0);       /* sx=104: the first segment ends inside a dash */
  t_delta(t, 16, 0);      /* sx=120 */
  TEST_ASSERT_EQUAL(MAP_OK, render_with(&v, &sc, &rec, NULL));
  TEST_ASSERT_EQUAL_INT(3, rec.n);
  TEST_ASSERT_EQUAL_INT(100, rec.lines[0].x0);
  TEST_ASSERT_EQUAL_INT(104, rec.lines[0].x1);
  TEST_ASSERT_EQUAL_INT(104, rec.lines[1].x0);
  TEST_ASSERT_EQUAL_INT(108, rec.lines[1].x1);
  TEST_ASSERT_EQUAL_INT(112, rec.lines[2].x0);
  TEST_ASSERT_EQUAL_INT(120, rec.lines[2].x1);
}

void test_render_dash_phase_survives_clipping_at_screen_edge(void) {
  /* the railway starts off screen: the pattern must not "crawl" from the edge */
  src_ctx_t sc = {0};
  recorder_t rec;
  map_view_t v = view_at_world(tile_center(TX0), tile_center(TY0), 1.0);
  /* sx = -52..20, dashes start at -52 with period 12: dash [-52,-44]... */
  tile_one_feature(&sc, 0, TX0, TY0, BKM_CLASS_RAIL, 1846, 2048, 1918, 2048);
  TEST_ASSERT_EQUAL(MAP_OK, render_with(&v, &sc, &rec, NULL));
  /* start at sx=-52: phase 0 -> dashes [-52,-44] [-40,-32] [-28,-20] [-16,-8]
     [-4,4] [8,16]; from the edge x=0 we see [0,4] and [8,16] */
  TEST_ASSERT_EQUAL_INT(2, rec.n);
  TEST_ASSERT_EQUAL_INT(0, rec.lines[0].x0);
  TEST_ASSERT_EQUAL_INT(4, rec.lines[0].x1);
  TEST_ASSERT_EQUAL_INT(8, rec.lines[1].x0);
  TEST_ASSERT_EQUAL_INT(16, rec.lines[1].x1);
}

void test_render_canvas_never_gets_out_of_range_coordinates(void) {
  src_ctx_t sc = {0};
  recorder_t rec;
  map_view_t v = view_at_world(tile_center(TX0), tile_center(TY0), 1.0);
  /* highway along the left edge, wide brush on the boundary */
  tile_one_feature(&sc, 0, TX0, TY0, BKM_CLASS_HIGHWAY, 1898, 0, 1898, 4095);
  render_with(&v, &sc, &rec, NULL);
  TEST_ASSERT_GREATER_THAN_INT(0, rec.n);
  for (int i = 0; i < rec.n && i < 512; i++) {
    TEST_ASSERT_TRUE(rec.lines[i].x0 >= 0 && rec.lines[i].x0 < SCREEN_W);
    TEST_ASSERT_TRUE(rec.lines[i].x1 >= 0 && rec.lines[i].x1 < SCREEN_W);
    TEST_ASSERT_TRUE(rec.lines[i].y0 >= 0 && rec.lines[i].y0 < SCREEN_H);
    TEST_ASSERT_TRUE(rec.lines[i].y1 >= 0 && rec.lines[i].y1 < SCREEN_H);
  }
}

/* ---------- map_bitmap: host canvas, PBM/PNG ---------- */

static uint8_t g_px[SCREEN_W * SCREEN_H];
#define ADLER_FF5000 0x54B27596u
#define CRC_FF5000 0x338AE894u

void test_bitmap_line_sets_endpoints_and_clear_resets(void) {
  map_bitmap_t bm;
  bm_init(&bm, 8, 4, g_px);
  bm_line(&bm, 0, 0, 7, 3);
  TEST_ASSERT_TRUE(bm_get(&bm, 0, 0));
  TEST_ASSERT_TRUE(bm_get(&bm, 7, 3));
  TEST_ASSERT_FALSE(bm_get(&bm, 7, 0));
  bm_clear(&bm);
  TEST_ASSERT_FALSE(bm_get(&bm, 0, 0));
  bm_set(&bm, 99, 99); /* outside the canvas: ignored */
  TEST_ASSERT_FALSE(bm_get(&bm, 99, 99));
}

void test_bitmap_pbm_is_packed_msb_first(void) {
  map_bitmap_t bm;
  bm_init(&bm, 10, 2, g_px);
  bm_set(&bm, 0, 0);
  bm_set(&bm, 9, 1);
  FILE *f = tmpfile();
  TEST_ASSERT_NOT_NULL(f);
  TEST_ASSERT_EQUAL_INT(0, bm_write_pbm(&bm, f));
  rewind(f);
  uint8_t out[64];
  size_t n = fread(out, 1, sizeof out, f);
  fclose(f);
  static const uint8_t exp[] = {'P', '4', '\n', '1', '0', ' ', '2', '\n',
                                0x80, 0x00,  /* row 0: pixel 0 */
                                0x00, 0x40}; /* row 1: pixel 9 = bit 6 of the second byte */
  TEST_ASSERT_EQUAL_size_t(sizeof exp, n);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(exp, out, sizeof exp);
}

void test_bitmap_crc32_and_adler32_known_vectors(void) {
  TEST_ASSERT_EQUAL_HEX32(0xCBF43926u, bm_crc32(0, (const uint8_t *)"123456789", 9));
  TEST_ASSERT_EQUAL_HEX32(0x11E60398u, bm_adler32(1, (const uint8_t *)"Wikipedia", 9));
  /* long vector, sums exceed 65521 (reference: zlib.adler32/crc32) */
  static uint8_t ff[5000];
  memset(ff, 0xFF, sizeof ff);
  TEST_ASSERT_EQUAL_HEX32(ADLER_FF5000, bm_adler32(1, ff, sizeof ff));
  TEST_ASSERT_EQUAL_HEX32(CRC_FF5000, bm_crc32(0, ff, sizeof ff));
}

void test_bitmap_png_has_valid_structure_and_pixels(void) {
  map_bitmap_t bm;
  bm_init(&bm, 10, 3, g_px);
  bm_set(&bm, 0, 0);
  bm_set(&bm, 9, 2);
  FILE *f = tmpfile();
  TEST_ASSERT_NOT_NULL(f);
  TEST_ASSERT_EQUAL_INT(0, bm_write_png(&bm, f));
  rewind(f);
  static uint8_t png[1024];
  size_t n = fread(png, 1, sizeof png, f);
  fclose(f);
  static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
  TEST_ASSERT_TRUE(n > 8 + 25);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(sig, png, 8);
  /* IHDR: length 13, 10x3, bit depth 1, type 0 (grayscale) */
  TEST_ASSERT_EQUAL_MEMORY("\0\0\0\x0d" "IHDR", png + 8, 8);
  TEST_ASSERT_EQUAL_HEX8(0, png[16]); TEST_ASSERT_EQUAL_HEX8(10, png[19]);
  TEST_ASSERT_EQUAL_HEX8(3, png[23]);
  TEST_ASSERT_EQUAL_HEX8(1, png[24]);
  TEST_ASSERT_EQUAL_HEX8(0, png[25]);
  /* CRC IHDR */
  uint32_t crc = bm_crc32(0, png + 12, 17);
  uint32_t got = ((uint32_t)png[29] << 24) | ((uint32_t)png[30] << 16) |
                 ((uint32_t)png[31] << 8) | png[32];
  TEST_ASSERT_EQUAL_HEX32(crc, got);
  /* IDAT: zlib + one stored block, then rows with filter 0; black = bit 0 */
  size_t p = 33;
  uint32_t len = ((uint32_t)png[p] << 24) | ((uint32_t)png[p + 1] << 16) |
                 ((uint32_t)png[p + 2] << 8) | png[p + 3];
  TEST_ASSERT_EQUAL_MEMORY("IDAT", png + p + 4, 4);
  const uint8_t *z = png + p + 8;
  TEST_ASSERT_EQUAL_HEX8(0x78, z[0]);
  TEST_ASSERT_EQUAL_HEX8(0x01, z[1]);
  TEST_ASSERT_EQUAL_HEX8(0x01, z[2]); /* BFINAL=1, BTYPE=00 */
  unsigned blen = z[3] | (z[4] << 8), nlen = z[5] | (z[6] << 8);
  TEST_ASSERT_EQUAL_UINT(0xFFFFu, blen ^ nlen);
  TEST_ASSERT_EQUAL_UINT(3 * (1 + 2), blen);
  const uint8_t *rows = z + 7;
  static const uint8_t exp_rows[] = {0, 0x7F, 0xFF,  /* (0,0) is black */
                                     0, 0xFF, 0xFF,
                                     0, 0xFF, 0xBF}; /* (9,2): bit 6 of the second byte */
  TEST_ASSERT_EQUAL_HEX8_ARRAY(exp_rows, rows, sizeof exp_rows);
  TEST_ASSERT_EQUAL_UINT32(bm_adler32(1, rows, blen),
                           ((uint32_t)z[7 + blen] << 24) | ((uint32_t)z[8 + blen] << 16) |
                               ((uint32_t)z[9 + blen] << 8) | z[10 + blen]);
  TEST_ASSERT_EQUAL_UINT32(2 + 5 + blen + 4, len);
  TEST_ASSERT_EQUAL_MEMORY("IEND", png + n - 8, 4);
}

void test_render_into_bitmap_draws_expected_pixels(void) {
  src_ctx_t sc = {0};
  map_bitmap_t bm;
  bm_init(&bm, SCREEN_W, SCREEN_H, g_px);
  tile_one_feature(&sc, 0, TX0, TY0, BKM_CLASS_STREET, 0, 2048, 4095, 2048);
  map_view_t v = view_at_world(tile_center(TX0), tile_center(TY0), 1.0);
  map_tile_source_t src = {table_loader, &sc, g_tile_buf, sizeof g_tile_buf};
  map_canvas_t cv = {&bm, SCREEN_W, SCREEN_H, bm_canvas_line};
  TEST_ASSERT_EQUAL(MAP_OK, map_render(&v, &src, &cv, NULL));
  int set = 0;
  for (int y = 0; y < SCREEN_H; y++)
    for (int x = 0; x < SCREEN_W; x++) set += bm_get(&bm, x, y);
  TEST_ASSERT_EQUAL_INT(SCREEN_W, set);
  TEST_ASSERT_TRUE(bm_get(&bm, 0, 200));
  TEST_ASSERT_TRUE(bm_get(&bm, SCREEN_W - 1, 200));
  TEST_ASSERT_FALSE(bm_get(&bm, 150, 199));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_bkm_open_empty_tile);
  RUN_TEST(test_bkm_decodes_deltas_to_absolute_points);
  RUN_TEST(test_bkm_is_little_endian_and_signed);
  RUN_TEST(test_bkm_iterates_all_features_in_order);
  RUN_TEST(test_bkm_open_rejects_null_arguments);
  RUN_TEST(test_bkm_rejects_short_header);
  RUN_TEST(test_bkm_rejects_bad_magic_and_version);
  RUN_TEST(test_bkm_rejects_unknown_class);
  RUN_TEST(test_bkm_rejects_feature_with_fewer_than_two_points);
  RUN_TEST(test_bkm_rejects_point_data_past_end_of_buffer);
  RUN_TEST(test_bkm_rejects_count_larger_than_content);
  RUN_TEST(test_bkm_rejects_trailing_bytes);
  RUN_TEST(test_bkm_rejects_count_smaller_than_content);
  RUN_TEST(test_bkm_rejects_absolute_coordinate_outside_int16);
  RUN_TEST(test_bkm_accepts_exact_int16_limits);
  RUN_TEST(test_bkm_every_truncation_of_valid_tile_is_rejected);
  RUN_TEST(test_bkm_corrupted_bytes_never_overrun);
  RUN_TEST(test_geo_tile_index_known_points);
  RUN_TEST(test_geo_tile_index_edges_stay_inside_grid);
  RUN_TEST(test_geo_rejects_invalid_coordinates);
  RUN_TEST(test_geo_world_matches_tile_index_and_reference);
  RUN_TEST(test_geo_latlon_roundtrip);
  RUN_TEST(test_geo_meters_per_unit);
  RUN_TEST(test_clip_inside_segment_is_unchanged);
  RUN_TEST(test_clip_segment_fully_outside_is_rejected);
  RUN_TEST(test_clip_crossing_segment_is_cut_at_edges);
  RUN_TEST(test_clip_rejects_segment_passing_outside_a_corner);
  RUN_TEST(test_clip_keeps_one_sided_cut);
  RUN_TEST(test_clip_is_symmetric_for_swapped_endpoints);
  RUN_TEST(test_clip_boundary_and_degenerate_cases);
  RUN_TEST(test_clip_rejects_non_finite_and_survives_huge_values);
  RUN_TEST(test_render_street_through_centre_is_one_clipped_line);
  RUN_TEST(test_render_scale_halves_distances_from_centre);
  RUN_TEST(test_render_loads_four_tiles_at_a_corner_and_draws_neighbour);
  RUN_TEST(test_render_missing_tiles_are_counted_not_fatal);
  RUN_TEST(test_render_corrupt_tile_is_skipped_others_still_drawn);
  RUN_TEST(test_render_loader_claiming_more_than_buffer_is_corrupt);
  RUN_TEST(test_render_valid_tile_longer_than_cap_is_still_rejected);
  RUN_TEST(test_render_rejects_invalid_arguments);
  RUN_TEST(test_render_refuses_view_needing_too_many_tiles);
  RUN_TEST(test_render_never_requests_tiles_outside_the_world);
  RUN_TEST(test_render_highway_uses_wide_brush_street_does_not);
  RUN_TEST(test_render_rail_is_dashed_8_on_4_off);
  RUN_TEST(test_render_dash_phase_continues_across_segments);
  RUN_TEST(test_render_dash_phase_survives_clipping_at_screen_edge);
  RUN_TEST(test_render_canvas_never_gets_out_of_range_coordinates);
  RUN_TEST(test_bitmap_line_sets_endpoints_and_clear_resets);
  RUN_TEST(test_bitmap_pbm_is_packed_msb_first);
  RUN_TEST(test_bitmap_crc32_and_adler32_known_vectors);
  RUN_TEST(test_bitmap_png_has_valid_structure_and_pixels);
  RUN_TEST(test_render_into_bitmap_draws_expected_pixels);
  return UNITY_END();
}
