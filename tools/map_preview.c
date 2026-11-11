/* Host map viewer: reads .bkm tiles from a directory and draws a 300x400 view to PNG.
 *
 *   cc -std=c11 -O2 -Wall -Icomponents/map tools/map_preview.c \
 *      components/map/bkm.c components/map/map_geo.c components/map/map_clip.c \
 *      components/map/map_render.c -lm -o tools/out/map_preview
 *   tools/out/map_preview tools/out/map 49.8397 24.0297 4 tools/out/preview.png
 *
 * For the Mac only; not part of the firmware. */
#include "map_bitmap.h"
#include "map_render.h"
#include <errno.h>

#define W 300
#define H 400
#define TILE_BUF 65536

typedef struct {
  const char *dir;
} dir_ctx_t;

static size_t load_from_dir(void *ctx, int z, int32_t x, int32_t y, uint8_t *buf,
                            size_t cap) {
  const dir_ctx_t *c = ctx;
  char path[1024];
  int n = snprintf(path, sizeof path, "%s/%d/%d/%d.bkm", c->dir, z, (int)x, (int)y);
  if (n < 0 || (size_t)n >= sizeof path) return 0;
  FILE *f = fopen(path, "rb");
  if (!f) return 0;
  /* cap+1: a file larger than the buffer yields len > cap, not a silently truncated tile */
  size_t len = fread(buf, 1, cap, f);
  int extra = fgetc(f);
  fclose(f);
  return extra != EOF ? cap + 1 : len;
}

static uint8_t g_px[W * H];
static uint8_t g_tile[TILE_BUF];

int main(int argc, char **argv) {
  if (argc != 6) {
    fprintf(stderr, "usage: %s tile_dir lat lon metres_per_pixel out.png\n", argv[0]);
    return 2;
  }
  char *end;
  map_view_t v;
  v.lat = strtod(argv[2], &end);
  if (*end) { fprintf(stderr, "bad lat\n"); return 2; }
  v.lon = strtod(argv[3], &end);
  if (*end) { fprintf(stderr, "bad lon\n"); return 2; }
  v.m_per_px = strtof(argv[4], &end);
  if (*end) { fprintf(stderr, "bad scale\n"); return 2; }

  map_bitmap_t bm;
  bm_init(&bm, W, H, g_px);
  dir_ctx_t dc = {argv[1]};
  map_tile_source_t src = {load_from_dir, &dc, g_tile, sizeof g_tile};
  map_canvas_t cv = {&bm, W, H, bm_canvas_line};
  map_stats_t st;
  map_status_t rc = map_render(&v, &src, &cv, &st);
  if (rc != MAP_OK) {
    fprintf(stderr, "map_render failed: %d\n", (int)rc);
    return 1;
  }
  fprintf(stderr, "tiles drawn=%u missing=%u corrupt=%u lines=%u\n", st.tiles_drawn,
          st.tiles_missing, st.tiles_corrupt, st.lines);

  FILE *f = fopen(argv[5], "wb");
  if (!f || bm_write_png(&bm, f) != 0 || fclose(f) != 0) {
    fprintf(stderr, "cannot write %s: %s\n", argv[5], strerror(errno));
    return 1;
  }
  return 0;
}
