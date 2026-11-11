#ifndef MAP_GEO_H
#define MAP_GEO_H

/* Web Mercator: tile index and lat/lon projection into "world units".
 * One world unit = 1/4096 of a tile (BKM_TILE_UNITS per tile). World
 * coordinates are computed in double (at z=15 they reach ~1.3e8, float would
 * give a step of 8 units); we switch to float only for the difference from the
 * view centre. */

#include "bkm.h"
#include <stdbool.h>
#include <stdint.h>

#define MAP_ZOOM 15
#define MAP_MIN_ZOOM 1
#define MAP_MAX_ZOOM 22
#define MAP_MAX_LAT 85.0511287798

/* false if the input is not finite, outside Mercator, or zoom is outside 1..22 */
bool map_tile_index(double lat, double lon, int z, int32_t *tx, int32_t *ty);
bool map_world(double lat, double lon, int z, double *wx, double *wy);
bool map_latlon(double wx, double wy, int z, double *lat, double *lon);

/* Metres per world unit at this latitude. */
double map_meters_per_unit(double lat, int z);

#endif
