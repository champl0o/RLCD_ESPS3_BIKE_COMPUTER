#include "map_geo.h"
#include <math.h>

#define EARTH_CIRCUMFERENCE_M 40075016.686
#define DEG2RAD (M_PI / 180.0)
#define RAD2DEG (180.0 / M_PI)

static bool zoom_ok(int z) { return z >= MAP_MIN_ZOOM && z <= MAP_MAX_ZOOM; }

/* number of world units along one axis */
static double world_size(int z) {
  return ldexp((double)BKM_TILE_UNITS, z);
}

bool map_world(double lat, double lon, int z, double *wx, double *wy) {
  if (!wx || !wy || !zoom_ok(z)) return false;
  if (!isfinite(lat) || !isfinite(lon)) return false;
  if (fabs(lat) > MAP_MAX_LAT || lon < -180.0 || lon > 180.0) return false;
  double size = world_size(z);
  *wx = (lon + 180.0) / 360.0 * size;
  *wy = (1.0 - asinh(tan(lat * DEG2RAD)) / M_PI) / 2.0 * size;
  return true;
}

bool map_tile_index(double lat, double lon, int z, int32_t *tx, int32_t *ty) {
  double wx, wy;
  if (!tx || !ty || !map_world(lat, lon, z, &wx, &wy)) return false;
  int32_t last = (int32_t)ldexp(1.0, z) - 1; /* lon=180 or lat on the edge */
  int32_t x = (int32_t)floor(wx / BKM_TILE_UNITS);
  int32_t y = (int32_t)floor(wy / BKM_TILE_UNITS);
  *tx = x > last ? last : (x < 0 ? 0 : x);
  *ty = y > last ? last : (y < 0 ? 0 : y);
  return true;
}

bool map_latlon(double wx, double wy, int z, double *lat, double *lon) {
  if (!lat || !lon || !zoom_ok(z)) return false;
  if (!isfinite(wx) || !isfinite(wy)) return false;
  double size = world_size(z);
  if (wx < 0.0 || wx > size || wy < 0.0 || wy > size) return false;
  *lon = wx / size * 360.0 - 180.0;
  *lat = atan(sinh(M_PI * (1.0 - 2.0 * wy / size))) * RAD2DEG;
  return true;
}

double map_meters_per_unit(double lat, int z) {
  return EARTH_CIRCUMFERENCE_M * cos(lat * DEG2RAD) / world_size(z);
}
