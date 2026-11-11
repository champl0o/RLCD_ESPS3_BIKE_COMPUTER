#include "map_clip.h"
#include <math.h>

enum { OUT_LEFT = 1, OUT_RIGHT = 2, OUT_TOP = 4, OUT_BOTTOM = 8 };

/* each clip moves at least one coordinate onto an edge, so 4 passes suffice;
   the margin guards against an endless loop from float inaccuracy */
#define MAX_ITER 8

static int outcode(float x, float y, float xmin, float ymin, float xmax,
                   float ymax) {
  int c = 0;
  if (x < xmin) c |= OUT_LEFT;
  else if (x > xmax) c |= OUT_RIGHT;
  if (y < ymin) c |= OUT_TOP;
  else if (y > ymax) c |= OUT_BOTTOM;
  return c;
}

bool map_clip_line(float *x0, float *y0, float *x1, float *y1, float xmin,
                   float ymin, float xmax, float ymax) {
  if (!isfinite(*x0) || !isfinite(*y0) || !isfinite(*x1) || !isfinite(*y1))
    return false;

  int c0 = outcode(*x0, *y0, xmin, ymin, xmax, ymax);
  int c1 = outcode(*x1, *y1, xmin, ymin, xmax, ymax);

  for (int i = 0; i < MAX_ITER; i++) {
    if ((c0 | c1) == 0) return true;  /* both inside */
    if ((c0 & c1) != 0) return false; /* both outside on the same side */

    int c = c0 ? c0 : c1;
    float x, y;
    float dx = *x1 - *x0, dy = *y1 - *y0;
    if (c & OUT_BOTTOM) {
      x = *x0 + dx * (ymax - *y0) / dy;
      y = ymax;
    } else if (c & OUT_TOP) {
      x = *x0 + dx * (ymin - *y0) / dy;
      y = ymin;
    } else if (c & OUT_RIGHT) {
      y = *y0 + dy * (xmax - *x0) / dx;
      x = xmax;
    } else {
      y = *y0 + dy * (xmin - *x0) / dx;
      x = xmin;
    }
    if (c == c0) {
      *x0 = x; *y0 = y;
      c0 = outcode(x, y, xmin, ymin, xmax, ymax);
    } else {
      *x1 = x; *y1 = y;
      c1 = outcode(x, y, xmin, ymin, xmax, ymax);
    }
  }
  return false;
}
