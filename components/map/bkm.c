#include "bkm.h"

#define FEATURE_HEADER_SIZE 3
#define POINT_SIZE 4

static uint16_t get_u16(const uint8_t *p) {
  return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static int32_t get_i16(const uint8_t *p) {
  uint16_t u = get_u16(p);
  return u >= 0x8000u ? (int32_t)u - 0x10000 : (int32_t)u;
}

bkm_status_t bkm_open(bkm_reader_t *r, const uint8_t *data, size_t len) {
  if (!r || !data) return BKM_ERR_ARG;
  if (len < BKM_HEADER_SIZE) return BKM_ERR_TRUNCATED;
  if (data[0] != 'B' || data[1] != 'K' || data[2] != 'M' || data[3] != '1')
    return BKM_ERR_MAGIC;
  if (data[4] != BKM_VERSION) return BKM_ERR_VERSION;

  uint16_t count = get_u16(data + 5);
  size_t pos = BKM_HEADER_SIZE;
  for (uint16_t i = 0; i < count; i++) {
    if (len - pos < FEATURE_HEADER_SIZE) return BKM_ERR_TRUNCATED;
    uint8_t cls = data[pos];
    uint16_t npts = get_u16(data + pos + 1);
    pos += FEATURE_HEADER_SIZE;
    if (cls < BKM_CLASS_HIGHWAY || cls > BKM_CLASS_RAIL) return BKM_ERR_CLASS;
    if (npts < 2) return BKM_ERR_POINTS;
    if ((len - pos) / POINT_SIZE < npts) return BKM_ERR_TRUNCATED;

    int32_t x = 0, y = 0;
    for (uint16_t k = 0; k < npts; k++, pos += POINT_SIZE) {
      x += get_i16(data + pos);
      y += get_i16(data + pos + 2);
      if (x < INT16_MIN || x > INT16_MAX || y < INT16_MIN || y > INT16_MAX)
        return BKM_ERR_RANGE;
    }
  }
  if (pos != len) return BKM_ERR_TRAILING;

  r->data = data;
  r->len = len;
  r->pos = BKM_HEADER_SIZE;
  r->remaining = count;
  return BKM_OK;
}

bool bkm_next(bkm_reader_t *r, bkm_feature_t *f) {
  if (!r || !f || r->remaining == 0) return false;
  const uint8_t *p = r->data + r->pos;
  f->cls = p[0];
  f->npts = get_u16(p + 1);
  f->pts = p + FEATURE_HEADER_SIZE;
  f->left = f->npts;
  f->x = 0;
  f->y = 0;
  r->pos += FEATURE_HEADER_SIZE + (size_t)f->npts * POINT_SIZE;
  r->remaining--;
  return true;
}

bool bkm_feature_next_point(bkm_feature_t *f, int16_t *x, int16_t *y) {
  if (!f || f->left == 0) return false;
  f->x = (int16_t)(f->x + get_i16(f->pts));
  f->y = (int16_t)(f->y + get_i16(f->pts + 2));
  f->pts += POINT_SIZE;
  f->left--;
  *x = f->x;
  *y = f->y;
  return true;
}
