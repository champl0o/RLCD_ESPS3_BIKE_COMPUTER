#include "fit.h"
#include <stddef.h>
#include <stdint.h>

#define MESG_FILE_ID 0
#define MESG_RECORD 20
#define FIT_INVALID_SINT32 0x7FFFFFFF

#define HEADER_SIZE 14
#define PROTOCOL_VERSION 0x20
#define PROFILE_VERSION 0x52A6
#define SIGNATURE '.fit'
#define DATA_SIZE(total_size) ((total_size) - HEADER_SIZE)

size_t put_u32(uint8_t *buf, uint32_t value) {
  for (size_t i = 0; i < 4; i++) {
    buf[i] = (uint8_t)((value >> (8 * i)) & 0xFF);
  }
  return 4;
}

size_t put_u16(uint8_t *buf, uint16_t value) {
  for (size_t i = 0; i < 2; i++) {
    buf[i] = (uint8_t)((value >> (8 * i)) & 0xFF);
  }
  return 2;
}

uint16_t fit_crc16(uint16_t crc, const uint8_t *buf, size_t len) {
  static const uint16_t crc_table[16] = {
      0x0000, 0xCC01, 0xD801, 0x1400, 0xF001, 0x3C00, 0x2800, 0xE401,
      0xA001, 0x6C00, 0x7800, 0xB401, 0x5000, 0x9C01, 0x8801, 0x4400};
  uint16_t tmp;

  // ітеруюся по кожному байту буфера
  for (size_t i = 0; i < len; i++) {
    uint8_t byte = buf[i];

    // вираховую контрольну суму для нижніх чотирьох бітів байта
    tmp = crc_table[crc & 0xF];
    crc = (crc >> 4) & 0x0FFF;
    crc = crc ^ tmp ^ crc_table[byte & 0xF];

    // вираховую контрольну суму для верхніх чотирьох бітів байта
    tmp = crc_table[crc & 0xF];
    crc = (crc >> 4) & 0x0FFF;
    crc = crc ^ tmp ^ crc_table[(byte >> 4) & 0xF];
  }

  return crc;
}

size_t fit_write_header(uint8_t *buf, uint32_t total_size) {
  buf[0] = HEADER_SIZE;
  buf[1] = PROTOCOL_VERSION;
  put_u16(buf + 2, PROFILE_VERSION);
  put_u32(buf + 4, total_size);
  buf[8] = SIGNATURE & 0xFF;
  buf[9] = (SIGNATURE >> 8) & 0xFF;
  buf[10] = (SIGNATURE >> 16) & 0xFF;
  buf[11] = (SIGNATURE >> 24) & 0xFF;
  uint16_t crc = fit_crc16(0, buf, HEADER_SIZE - 2);
  put_u16(buf + HEADER_SIZE - 2, crc);
  return HEADER_SIZE;
}
