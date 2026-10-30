#include "fit.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define MESG_FILE_ID 0
#define MESG_RECORD 20
#define MESG_EVENT 21
#define MESG_LAP 19
#define MESG_SESSION 18
#define MESG_ACTIVITY 34

#define LOCAL_EVENT 2
#define LOCAL_LAP 3
#define LOCAL_SESSION 4
#define LOCAL_ACTIVITY 5

#define EVENT_TIMER 0
#define SPORT_CYCLING 2
#define SUB_SPORT_GRAVEL_CYCLING 46
#define EVENT_ACTIVITY 26
#define ACTIVITY_MANUAL 0
#define EVENT_TYPE_STOP 1

#define HEADER_SIZE 14
#define PROTOCOL_VERSION 0x20
#define PROFILE_VERSION 0x52A6
#define SIGNATURE ('.' | ('F' << 8) | ('I' << 16) | ('T' << 24))
#define FILE_TYPE_ACTIVITY 4
#define MANUFACTURER_DEVELOPMENT 255
#define PRODUCT_NAME_SIZE 13
#define PRODUCT_NAME "RLCD Bike"

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

size_t fit_write_header(uint8_t *buf, uint32_t data_size) {
  buf[0] = HEADER_SIZE;
  buf[1] = PROTOCOL_VERSION;
  put_u16(buf + 2, PROFILE_VERSION);
  put_u32(buf + 4, data_size);
  buf[8] = SIGNATURE & 0xFF;
  buf[9] = (SIGNATURE >> 8) & 0xFF;
  buf[10] = (SIGNATURE >> 16) & 0xFF;
  buf[11] = (SIGNATURE >> 24) & 0xFF;
  uint16_t crc = fit_crc16(0, buf, HEADER_SIZE - 2);
  put_u16(buf + HEADER_SIZE - 2, crc);
  return HEADER_SIZE;
}

size_t fit_write_crc(uint8_t *buf, uint16_t crc) {
  put_u16(buf, crc);
  return 2;
}

size_t fit_write_file_id_definition(uint8_t *buf) {
  buf[0] = 0x40;                       // Definition message header
  buf[1] = 0x00;                       // Reserved
  buf[2] = 0x00;                       // little-endian
  buf[3] = MESG_FILE_ID & 0xFF;        // Message number (lower byte)
  buf[4] = (MESG_FILE_ID >> 8) & 0xFF; // Message number (upper byte)
  buf[5] = 0x05;                       // Number of fields
  buf[6] = 0x00;                       // Field definition 0: File Type
  buf[7] = 0x01;                       // Field definition 0: Size
  buf[8] = 0x00;                       // Field definition 0: Base
  buf[9] = 0x01;                       // Field definition 1: Manufacturer
  buf[10] = 0x02;                      // Field definition 1: Size
  buf[11] = 0x84;                      // Field definition 1: Base
  buf[12] = 0x04;                      // Field definition 4: Time Created
  buf[13] = 0x04;                      // Field definition 4: Size
  buf[14] = 0x86;                      // Field definition 4: Base
  buf[15] = 0x02;                      // Field definition 2: Product
  buf[16] = 0x02;                      // Field definition 2: Size
  buf[17] = 0x84;                      // Field definition 2: Base
  buf[18] = 0x08;                      // Field definition 8: Product Name
  buf[19] = 0x0d;                      // Field definition 8: Size
  buf[20] = 0x07;                      // Field definition 8: Base

  return 21;
}

size_t fit_write_file_id_data(uint8_t *buf, uint32_t time_created) {
  size_t n = 0;
  buf[n++] = 0x00;
  buf[n++] = FILE_TYPE_ACTIVITY;
  n += put_u16(buf + n, MANUFACTURER_DEVELOPMENT);
  n += put_u32(buf + n, time_created);
  n += put_u16(buf + n, 0);
  memset(buf + n, 0, PRODUCT_NAME_SIZE);
  memcpy(buf + n, PRODUCT_NAME, sizeof(PRODUCT_NAME) - 1);
  n += PRODUCT_NAME_SIZE;

  return n;
}

int32_t fit_degrees_to_semicircles(double degrees) {
  return (int32_t)(degrees * 2147483648.0 / 180.0);
}

size_t fit_write_record_definition(uint8_t *buf) {
  static const uint8_t def[] = {
      0x41, 0x00, 0x00, MESG_RECORD, 0x00, 0x07, // local type 1, 7 fields
      253,  0x04, 0x86,                          // timestamp, uint32
      0,    0x04, 0x85,                          // position_lat, sint32
      1,    0x04, 0x85,                          // position_long, sint32
      5,    0x04, 0x86,                          // distance, uint32
      6,    0x02, 0x84,                          // speed, uint16
      3,    0x01, 0x02,                          // heart_rate, uint8
      4,    0x01, 0x02,                          // cadence, uint8
  };
  memcpy(buf, def, sizeof(def));
  return sizeof(def);
}

size_t fit_write_record_data(uint8_t *buf, const fit_record_t *rec) {
  size_t n = 0;
  buf[n++] = 0x01;
  n += put_u32(buf + n, rec->timestamp);
  n += put_u32(buf + n, (uint32_t)rec->lat);
  n += put_u32(buf + n, (uint32_t)rec->lon);
  n += put_u32(buf + n, rec->distance_cm);
  n += put_u16(buf + n, rec->speed_mm_s);
  buf[n++] = rec->heart_rate;
  buf[n++] = rec->cadence;
  return n;
}

static uint32_t elapsed_ms(const fit_summary_t *summary) {
  return (summary->end_time - summary->start_time) * 1000;
}

size_t fit_write_event_definition(uint8_t *buf) {
  static const uint8_t def[] = {
      0x40 | LOCAL_EVENT, 0x00, 0x00, MESG_EVENT, 0x00, 0x04,
      253,                0x04, 0x86, // timestamp
      0,                  0x01, 0x00, // event
      1,                  0x01, 0x00, // event_type
      4,                  0x01, 0x02, // event_group
  };
  memcpy(buf, def, sizeof(def));
  return sizeof(def);
}

size_t fit_write_event_data(uint8_t *buf, uint32_t timestamp,
                            uint8_t event_type) {
  size_t n = 0;
  buf[n++] = LOCAL_EVENT;
  n += put_u32(buf + n, timestamp);
  buf[n++] = EVENT_TIMER;
  buf[n++] = event_type;
  buf[n++] = 0;
  return n;
}

size_t fit_write_lap_definition(uint8_t *buf) {
  static const uint8_t def[] = {
      0x40 | LOCAL_LAP, 0x00, 0x00, MESG_LAP, 0x00, 0x09,
      254,              0x02, 0x84, // message_index
      253,              0x04, 0x86, // timestamp
      2,                0x04, 0x86, // start_time
      7,                0x04, 0x86, // total_elapsed_time, ms
      8,                0x04, 0x86, // total_timer_time, ms
      9,                0x04, 0x86, // total_distance, cm
      13,               0x02, 0x84, // avg_speed, mm/s
      14,               0x02, 0x84, // max_speed, mm/s
      25,               0x01, 0x00, // sport
  };
  memcpy(buf, def, sizeof(def));
  return sizeof(def);
}

size_t fit_write_lap_data(uint8_t *buf, const fit_summary_t *summary) {
  size_t n = 0;
  buf[n++] = LOCAL_LAP;
  n += put_u16(buf + n, 0);
  n += put_u32(buf + n, summary->end_time);
  n += put_u32(buf + n, summary->start_time);
  n += put_u32(buf + n, elapsed_ms(summary));
  n += put_u32(buf + n, elapsed_ms(summary));
  n += put_u32(buf + n, summary->distance_cm);
  n += put_u16(buf + n, summary->avg_speed_mm_s);
  n += put_u16(buf + n, summary->max_speed_mm_s);
  buf[n++] = SPORT_CYCLING;
  return n;
}

size_t fit_write_session_definition(uint8_t *buf) {
  static const uint8_t def[] = {
      0x40 | LOCAL_SESSION, 0x00, 0x00, MESG_SESSION, 0x00, 0x0b,
      253,                  0x04, 0x86, // timestamp
      2,                    0x04, 0x86, // start_time
      5,                    0x01, 0x00, // sport
      6,                    0x01, 0x00, // sub_sport
      7,                    0x04, 0x86, // total_elapsed_time, ms
      8,                    0x04, 0x86, // total_timer_time, ms
      9,                    0x04, 0x86, // total_distance, cm
      14,                   0x02, 0x84, // avg_speed, mm/s
      15,                   0x02, 0x84, // max_speed, mm/s
      25,                   0x02, 0x84, // first_lap_index
      26,                   0x02, 0x84, // num_laps
  };
  memcpy(buf, def, sizeof(def));
  return sizeof(def);
}

size_t fit_write_session_data(uint8_t *buf, const fit_summary_t *summary) {
  size_t n = 0;
  buf[n++] = LOCAL_SESSION;
  n += put_u32(buf + n, summary->end_time);
  n += put_u32(buf + n, summary->start_time);
  buf[n++] = SPORT_CYCLING;
  buf[n++] = SUB_SPORT_GRAVEL_CYCLING;
  n += put_u32(buf + n, elapsed_ms(summary));
  n += put_u32(buf + n, elapsed_ms(summary));
  n += put_u32(buf + n, summary->distance_cm);
  n += put_u16(buf + n, summary->avg_speed_mm_s);
  n += put_u16(buf + n, summary->max_speed_mm_s);
  n += put_u16(buf + n, 0);
  n += put_u16(buf + n, 1);
  return n;
}

size_t fit_write_activity_definition(uint8_t *buf) {
  static const uint8_t def[] = {
      0x40 | LOCAL_ACTIVITY, 0x00, 0x00, MESG_ACTIVITY, 0x00, 0x07,
      253,                   0x04, 0x86, // timestamp
      0,                     0x04, 0x86, // total_timer_time, ms
      5,                     0x04, 0x86, // local_timestamp
      1,                     0x02, 0x84, // num_sessions
      2,                     0x01, 0x00, // type
      3,                     0x01, 0x00, // event
      4,                     0x01, 0x00, // event_type
  };
  memcpy(buf, def, sizeof(def));
  return sizeof(def);
}

size_t fit_write_activity_data(uint8_t *buf, const fit_summary_t *summary) {
  size_t n = 0;
  buf[n++] = LOCAL_ACTIVITY;
  n += put_u32(buf + n, summary->end_time);
  n += put_u32(buf + n, elapsed_ms(summary));
  n += put_u32(buf + n, summary->end_time); // local_timestamp без зсуву пояса
  n += put_u16(buf + n, 1);
  buf[n++] = ACTIVITY_MANUAL;
  buf[n++] = EVENT_ACTIVITY;
  buf[n++] = EVENT_TYPE_STOP;
  return n;
}
