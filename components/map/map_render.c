#include "map_render.h"
#include <math.h>
#include <string.h>

/* Style depends only on the class (section 5 of the doc). brush: 1 pixel or a
   "plus" of 5 lines (width 3); dash_on/dash_off = 0: solid. */
typedef struct {
  uint8_t brush;
  float dash_on;
  float dash_off;
} style_t;

static const style_t STYLES[] = {
    [BKM_CLASS_HIGHWAY] = {5, 0.f, 0.f},
    [BKM_CLASS_STREET] = {1, 0.f, 0.f},
    [BKM_CLASS_BIKE] = {1, 4.f, 3.f},
    [BKM_CLASS_WATER] = {1, 0.f, 0.f},
    [BKM_CLASS_RAIL] = {1, 8.f, 4.f},
};

static const int8_t BRUSH[5][2] = {{0, 0}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}};

/* Margin around the screen for brush pixels, so dashes are not clipped twice */
#define DASH_CLIP_MARGIN 2.f
#define MIN_DASH_LEN 1e-3f

typedef struct {
  const map_canvas_t *cv;
  map_stats_t *stats;
  float xmax, ymax; /* width-1, height-1 */
} draw_t;

static int round_px(float v) { return (int)floorf(v + 0.5f); }

/* Draws a segment with the brush; floating-point coordinates in screen pixels. */
static void emit(draw_t *d, const style_t *st, float x0, float y0, float x1,
                 float y1) {
  for (int i = 0; i < st->brush; i++) {
    float ax = x0 + BRUSH[i][0], ay = y0 + BRUSH[i][1];
    float bx = x1 + BRUSH[i][0], by = y1 + BRUSH[i][1];
    if (!map_clip_line(&ax, &ay, &bx, &by, 0.f, 0.f, d->xmax, d->ymax))
      continue;
    d->cv->line(d->cv->ctx, round_px(ax), round_px(ay), round_px(bx),
                round_px(by));
    d->stats->lines++;
  }
}

/* Dashes: phase (0..on+off) carries over between segments of one polyline. */
static void dashed_segment(draw_t *d, const style_t *st, float *phase,
                           float x0, float y0, float x1, float y1) {
  float period = st->dash_on + st->dash_off;
  float total = hypotf(x1 - x0, y1 - y0);
  float ax = x0, ay = y0, bx = x1, by = y1;

  if (map_clip_line(&ax, &ay, &bx, &by, -DASH_CLIP_MARGIN, -DASH_CLIP_MARGIN,
                    d->xmax + DASH_CLIP_MARGIN, d->ymax + DASH_CLIP_MARGIN)) {
    float vis = hypotf(bx - ax, by - ay);
    float p = fmodf(*phase + hypotf(ax - x0, ay - y0), period);
    float pos = 0.f;
    while (pos < vis - MIN_DASH_LEN) {
      if (p < st->dash_on) {
        float run = st->dash_on - p;
        bool partial = run > vis - pos;
        if (partial) run = vis - pos;
        float t0 = pos / vis, t1 = (pos + run) / vis;
        emit(d, st, ax + (bx - ax) * t0, ay + (by - ay) * t0,
             ax + (bx - ax) * t1, ay + (by - ay) * t1);
        pos += run;
        p = partial ? p + run : st->dash_on;
      } else {
        pos += period - p;
        p = 0.f;
      }
    }
  }
  *phase = fmodf(*phase + total, period);
}

typedef struct {
  float ox, oy; /* tile corner relative to the view centre, units */
  float scale;  /* pixels per unit */
  float cx, cy; /* screen centre */
} xform_t;

static void draw_feature(draw_t *d, const xform_t *xf, bkm_feature_t *f) {
  const style_t *st = &STYLES[f->cls];
  bool dashed = st->dash_on > 0.f;
  float phase = 0.f, px = 0.f, py = 0.f;
  bool have_prev = false;
  int16_t x, y;
  while (bkm_feature_next_point(f, &x, &y)) {
    float sx = xf->cx + (xf->ox + (float)x) * xf->scale;
    float sy = xf->cy + (xf->oy + (float)y) * xf->scale;
    if (have_prev) {
      if (dashed) dashed_segment(d, st, &phase, px, py, sx, sy);
      else emit(d, st, px, py, sx, sy);
    }
    px = sx;
    py = sy;
    have_prev = true;
  }
}

static double clamp_d(double v, double lo, double hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

map_status_t map_render(const map_view_t *view, const map_tile_source_t *src,
                        const map_canvas_t *cv, map_stats_t *stats) {
  if (!view || !src || !cv || !src->load || !src->buf || src->cap == 0 ||
      !cv->line || cv->width < 1 || cv->height < 1 ||
      cv->width > MAP_MAX_CANVAS_DIM || cv->height > MAP_MAX_CANVAS_DIM)
    return MAP_ERR_ARG;
  if (!isfinite(view->m_per_px) || view->m_per_px <= 0.f) return MAP_ERR_ARG;

  double wcx, wcy;
  if (!map_world(view->lat, view->lon, MAP_ZOOM, &wcx, &wcy)) return MAP_ERR_ARG;

  double m_per_unit = map_meters_per_unit(view->lat, MAP_ZOOM);
  double units_per_px = (double)view->m_per_px / m_per_unit;
  double hx = cv->width / 2.0 * units_per_px;
  double hy = cv->height / 2.0 * units_per_px;

  /* tile range in double and only then int: the scale can be arbitrary */
  double last = ldexp(1.0, MAP_ZOOM) - 1.0;
  double fx0 = clamp_d(floor((wcx - hx) / BKM_TILE_UNITS), 0.0, last);
  double fx1 = clamp_d(floor((wcx + hx) / BKM_TILE_UNITS), 0.0, last);
  double fy0 = clamp_d(floor((wcy - hy) / BKM_TILE_UNITS), 0.0, last);
  double fy1 = clamp_d(floor((wcy + hy) / BKM_TILE_UNITS), 0.0, last);
  if ((fx1 - fx0 + 1.0) * (fy1 - fy0 + 1.0) > MAP_MAX_TILES) return MAP_ERR_RANGE;

  map_stats_t local;
  if (!stats) stats = &local;
  memset(stats, 0, sizeof *stats);

  draw_t d = {cv, stats, (float)(cv->width - 1), (float)(cv->height - 1)};
  xform_t xf;
  xf.scale = (float)(1.0 / units_per_px);
  xf.cx = (float)cv->width / 2.0f;
  xf.cy = (float)cv->height / 2.0f;

  for (int32_t ty = (int32_t)fy0; ty <= (int32_t)fy1; ty++) {
    for (int32_t tx = (int32_t)fx0; tx <= (int32_t)fx1; tx++) {
      size_t len = src->load(src->ctx, MAP_ZOOM, tx, ty, src->buf, src->cap);
      if (len == 0) {
        stats->tiles_missing++;
        continue;
      }
      bkm_reader_t r;
      if (len > src->cap || bkm_open(&r, src->buf, len) != BKM_OK) {
        stats->tiles_corrupt++;
        continue;
      }
      xf.ox = (float)((double)tx * BKM_TILE_UNITS - wcx);
      xf.oy = (float)((double)ty * BKM_TILE_UNITS - wcy);
      bkm_feature_t f;
      while (bkm_next(&r, &f)) draw_feature(&d, &xf, &f);
      stats->tiles_drawn++;
    }
  }
  return MAP_OK;
}
