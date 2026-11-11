#ifndef MAP_BITMAP_H
#define MAP_BITMAP_H

/* Host canvas for the map renderer: 1 byte per pixel (1 = black), written as
 * PBM (P4) and PNG (uncompressed: deflate stored blocks, C standard library
 * only). For the Mac/tests only, not part of the firmware.
 * Everything is static inline: the file is included from a single .c. */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  int w, h;
  uint8_t *px; /* w*h bytes, owned by the caller */
} map_bitmap_t;

static inline void bm_init(map_bitmap_t *bm, int w, int h, uint8_t *storage) {
  bm->w = w;
  bm->h = h;
  bm->px = storage;
  memset(storage, 0, (size_t)w * (size_t)h);
}

static inline void bm_clear(map_bitmap_t *bm) {
  memset(bm->px, 0, (size_t)bm->w * (size_t)bm->h);
}

static inline void bm_set(map_bitmap_t *bm, int x, int y) {
  if (x >= 0 && y >= 0 && x < bm->w && y < bm->h) bm->px[(size_t)y * bm->w + x] = 1;
}

static inline int bm_get(const map_bitmap_t *bm, int x, int y) {
  if (x < 0 || y < 0 || x >= bm->w || y >= bm->h) return 0;
  return bm->px[(size_t)y * bm->w + x];
}

static inline void bm_line(map_bitmap_t *bm, int x0, int y0, int x1, int y1) {
  int dx = abs(x1 - x0), dy = -abs(y1 - y0);
  int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    bm_set(bm, x0, y0);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

/* matches the map_canvas_t.line signature */
static inline void bm_canvas_line(void *ctx, int x0, int y0, int x1, int y1) {
  bm_line((map_bitmap_t *)ctx, x0, y0, x1, y1);
}

/* ---- packing ---- */

static inline size_t bm_row_bytes(const map_bitmap_t *bm) {
  return ((size_t)bm->w + 7) / 8;
}

/* One row: bit 1 = black (PBM). inverted: black = bit 0, padding = 1 (PNG). */
static inline void bm_pack_row(const map_bitmap_t *bm, int y, int inverted,
                               uint8_t *out) {
  size_t n = bm_row_bytes(bm);
  memset(out, inverted ? 0xFF : 0x00, n);
  for (int x = 0; x < bm->w; x++) {
    if (!bm_get(bm, x, y)) continue;
    uint8_t mask = (uint8_t)(0x80u >> (x & 7));
    if (inverted) out[x >> 3] &= (uint8_t)~mask;
    else out[x >> 3] |= mask;
  }
}

/* 0 = success */
static inline int bm_write_pbm(const map_bitmap_t *bm, FILE *f) {
  if (fprintf(f, "P4\n%d %d\n", bm->w, bm->h) < 0) return -1;
  uint8_t *row = malloc(bm_row_bytes(bm));
  if (!row) return -1;
  int rc = 0;
  for (int y = 0; y < bm->h && rc == 0; y++) {
    bm_pack_row(bm, y, 0, row);
    if (fwrite(row, 1, bm_row_bytes(bm), f) != bm_row_bytes(bm)) rc = -1;
  }
  free(row);
  return rc;
}

static inline uint32_t bm_crc32(uint32_t crc, const uint8_t *p, size_t n) {
  crc = ~crc;
  for (size_t i = 0; i < n; i++) {
    crc ^= p[i];
    for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}

static inline uint32_t bm_adler32(uint32_t adler, const uint8_t *p, size_t n) {
  uint32_t a = adler & 0xFFFF, b = adler >> 16;
  for (size_t i = 0; i < n; i++) {
    a = (a + p[i]) % 65521u;
    b = (b + a) % 65521u;
  }
  return (b << 16) | a;
}

static inline void bm_put_be32(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
  p[2] = (uint8_t)(v >> 8);  p[3] = (uint8_t)v;
}

static inline int bm_write_chunk(FILE *f, const char type[4], const uint8_t *data,
                                 size_t len) {
  uint8_t head[8];
  bm_put_be32(head, (uint32_t)len);
  memcpy(head + 4, type, 4);
  uint32_t crc = bm_crc32(0, head + 4, 4);
  if (len) crc = bm_crc32(crc, data, len);
  uint8_t tail[4];
  bm_put_be32(tail, crc);
  if (fwrite(head, 1, 8, f) != 8) return -1;
  if (len && fwrite(data, 1, len, f) != len) return -1;
  return fwrite(tail, 1, 4, f) == 4 ? 0 : -1;
}

/* 0 = success. 1-bit grayscale image, black = 0. */
static inline int bm_write_png(const map_bitmap_t *bm, FILE *f) {
  static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
  size_t rb = bm_row_bytes(bm);
  size_t raw_len = (size_t)bm->h * (rb + 1);
  size_t blocks = (raw_len + 65534) / 65535;
  if (blocks == 0) blocks = 1;
  size_t z_len = 2 + blocks * 5 + raw_len + 4;

  uint8_t *raw = malloc(raw_len ? raw_len : 1);
  uint8_t *z = malloc(z_len);
  if (!raw || !z) { free(raw); free(z); return -1; }
  for (int y = 0; y < bm->h; y++) {
    raw[(size_t)y * (rb + 1)] = 0; /* filter None */
    bm_pack_row(bm, y, 1, raw + (size_t)y * (rb + 1) + 1);
  }

  size_t o = 0;
  z[o++] = 0x78;
  z[o++] = 0x01;
  size_t done = 0;
  for (size_t b = 0; b < blocks; b++) {
    size_t n = raw_len - done;
    if (n > 65535) n = 65535;
    z[o++] = (b + 1 == blocks) ? 1 : 0; /* BFINAL, BTYPE=00 */
    z[o++] = (uint8_t)(n & 0xFF);
    z[o++] = (uint8_t)(n >> 8);
    z[o++] = (uint8_t)(~n & 0xFF);
    z[o++] = (uint8_t)((~n >> 8) & 0xFF);
    memcpy(z + o, raw + done, n);
    o += n;
    done += n;
  }
  bm_put_be32(z + o, bm_adler32(1, raw, raw_len));

  uint8_t ihdr[13];
  bm_put_be32(ihdr, (uint32_t)bm->w);
  bm_put_be32(ihdr + 4, (uint32_t)bm->h);
  ihdr[8] = 1;  /* bit depth */
  ihdr[9] = 0;  /* grayscale */
  ihdr[10] = ihdr[11] = ihdr[12] = 0;

  int rc = 0;
  if (fwrite(sig, 1, 8, f) != 8) rc = -1;
  if (rc == 0) rc = bm_write_chunk(f, "IHDR", ihdr, sizeof ihdr);
  if (rc == 0) rc = bm_write_chunk(f, "IDAT", z, z_len);
  if (rc == 0) rc = bm_write_chunk(f, "IEND", NULL, 0);
  free(raw);
  free(z);
  return rc;
}

#endif
