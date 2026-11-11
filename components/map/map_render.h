#ifndef MAP_RENDER_H
#define MAP_RENDER_H

/* Vector map renderer (docs/map-design.md, section 7). No ESP-IDF dependency:
 * tiles are read through a loader function, drawing goes through a canvas that
 * is implemented on top of U8g2 on the board and writes to a bitmap on the host. */

#include "bkm.h"
#include "map_clip.h"
#include "map_geo.h"
#include <stddef.h>
#include <stdint.h>

/* Maximum tiles per view: 4 m/pixel gives ~2x3, 8 m/pixel up to 4x5. */
#define MAP_MAX_TILES 25
#define MAP_MAX_CANVAS_DIM 4096

typedef enum {
  MAP_OK = 0,
  MAP_ERR_ARG,   /* NULL, zero sizes, non-finite lat/lon/scale */
  MAP_ERR_RANGE, /* view needs more than MAP_MAX_TILES tiles */
} map_status_t;

/* Canvas. line() only receives coordinates inside [0,width-1]x[0,height-1]
 * (the renderer does the clipping) and draws a 1-pixel black segment. */
typedef struct {
  void *ctx;
  int width;
  int height;
  void (*line)(void *ctx, int x0, int y0, int x1, int y1);
} map_canvas_t;

/* Screen centre = (lat, lon); north is up; m_per_px = metres per pixel. */
typedef struct {
  double lat;
  double lon;
  float m_per_px;
} map_view_t;

/* Returns the number of tile bytes written to buf (<= cap), 0 if the tile is absent. */
typedef size_t (*map_tile_loader_t)(void *ctx, int z, int32_t x, int32_t y,
                                    uint8_t *buf, size_t cap);

typedef struct {
  map_tile_loader_t load;
  void *ctx;
  uint8_t *buf; /* working buffer for one tile, owned by the caller */
  size_t cap;
} map_tile_source_t;

typedef struct {
  unsigned tiles_drawn;
  unsigned tiles_missing; /* loader returned 0 */
  unsigned tiles_corrupt; /* failed bkm_open() validation */
  unsigned lines;         /* calls to canvas->line */
} map_stats_t;

/* Corrupt and missing tiles are not an error: they are only counted in stats
 * (stats may be NULL). */
map_status_t map_render(const map_view_t *view, const map_tile_source_t *src,
                        const map_canvas_t *canvas, map_stats_t *stats);

#endif
