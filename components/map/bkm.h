#ifndef BKM_H
#define BKM_H

/* .bkm tile format (details in docs/map-design.md, section 5).
 *
 *   Header (8 bytes): "BKM1" | version u8 | feature count u16 LE | 0 u8
 *   Feature:          class u8 | point count u16 LE | points
 *   Point:            dx i16 LE, dy i16 LE (delta from the previous point; the
 *                     first delta is from (0,0)), i.e. absolute tile coordinates
 *
 * Data from the SD card is treated as hostile: bkm_open() validates the whole
 * buffer in one pass, after which bkm_next()/bkm_feature_next_point() never
 * read outside it. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BKM_VERSION 1
#define BKM_HEADER_SIZE 8
#define BKM_TILE_UNITS 4096

typedef enum {
  BKM_CLASS_HIGHWAY = 1, /* highways, major roads */
  BKM_CLASS_STREET = 2,  /* streets */
  BKM_CLASS_BIKE = 3,    /* bike paths */
  BKM_CLASS_WATER = 4,   /* rivers, canals, water outlines */
  BKM_CLASS_RAIL = 5,    /* railway */
} bkm_class_t;

typedef enum {
  BKM_OK = 0,
  BKM_ERR_ARG,
  BKM_ERR_TRUNCATED, /* data ended earlier than promised */
  BKM_ERR_MAGIC,
  BKM_ERR_VERSION,
  BKM_ERR_CLASS,
  BKM_ERR_POINTS,   /* fewer than two points */
  BKM_ERR_RANGE,    /* absolute coordinate outside int16 */
  BKM_ERR_TRAILING, /* extra bytes after the last feature */
} bkm_status_t;

typedef struct {
  const uint8_t *data;
  size_t len;
  size_t pos;
  uint16_t remaining;
} bkm_reader_t;

typedef struct {
  uint8_t cls;
  uint16_t npts;
  const uint8_t *pts; /* points at deltas inside the tile buffer */
  uint16_t left;
  int16_t x, y;
} bkm_feature_t;

bkm_status_t bkm_open(bkm_reader_t *r, const uint8_t *data, size_t len);
bool bkm_next(bkm_reader_t *r, bkm_feature_t *f);
bool bkm_feature_next_point(bkm_feature_t *f, int16_t *x, int16_t *y);

#endif
