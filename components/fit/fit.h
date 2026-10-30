#pragma once

#include <stddef.h>
#include <stdint.h>

size_t fit_write_header(uint8_t *buf, uint32_t data_size);
uint16_t fit_crc16(uint16_t crc, const uint8_t *buf, size_t len);
size_t fit_write_crc(uint8_t *buf, uint16_t crc);
size_t fit_write_file_id_definition(uint8_t *buf);
size_t fit_write_file_id_data(uint8_t *buf, uint32_t time_created);

#define FIT_INVALID_UINT8 0xFF
#define FIT_INVALID_UINT16 0xFFFF
#define FIT_INVALID_SINT32 0x7FFFFFFF

typedef struct {
  uint32_t timestamp;   // секунди від 1989-12-31 (FIT epoch)
  int32_t lat;          // semicircles, FIT_INVALID_SINT32 якщо немає фіксу
  int32_t lon;          // semicircles, FIT_INVALID_SINT32 якщо немає фіксу
  uint32_t distance_cm; // сантиметри
  uint16_t speed_mm_s;  // мм/с, FIT_INVALID_UINT16 якщо невідомо
  uint8_t heart_rate;   // уд/хв, FIT_INVALID_UINT8 якщо немає датчика
  uint8_t cadence;      // об/хв, FIT_INVALID_UINT8 якщо немає датчика
} fit_record_t;

int32_t fit_degrees_to_semicircles(double degrees);
size_t fit_write_record_definition(uint8_t *buf);
size_t fit_write_record_data(uint8_t *buf, const fit_record_t *rec);

#define FIT_EVENT_START 0
#define FIT_EVENT_STOP_ALL 4

typedef struct {
  uint32_t start_time;       // FIT epoch
  uint32_t end_time;         // FIT epoch
  uint32_t distance_cm;
  uint16_t avg_speed_mm_s;
  uint16_t max_speed_mm_s;
} fit_summary_t;

size_t fit_write_event_definition(uint8_t *buf);
size_t fit_write_event_data(uint8_t *buf, uint32_t timestamp,
                            uint8_t event_type);
size_t fit_write_lap_definition(uint8_t *buf);
size_t fit_write_lap_data(uint8_t *buf, const fit_summary_t *summary);
size_t fit_write_session_definition(uint8_t *buf);
size_t fit_write_session_data(uint8_t *buf, const fit_summary_t *summary);
size_t fit_write_activity_definition(uint8_t *buf);
size_t fit_write_activity_data(uint8_t *buf, const fit_summary_t *summary);
