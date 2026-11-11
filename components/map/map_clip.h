#ifndef MAP_CLIP_H
#define MAP_CLIP_H

#include <stdbool.h>

/* Cohen-Sutherland algorithm. Clips a segment to the rectangle
 * [xmin,xmax]x[ymin,ymax] (edges inclusive), adjusting the endpoints in place.
 * false: the segment is not visible and the coordinates are then undefined.
 * Infinite and NaN inputs give false (with NaN every comparison is false, so
 * the segment would otherwise "pass"). */
bool map_clip_line(float *x0, float *y0, float *x1, float *y1, float xmin,
                   float ymin, float xmax, float ymax);

#endif
