#include "fit_writer.h"
#include <string.h>

#define MAX_MESSAGE_SIZE 64
#define CRC_CHUNK_SIZE 256

static int write_message(fit_writer_t *writer, const uint8_t *buf, size_t n) {
  if (fwrite(buf, 1, n, writer->file) != n) {
    return -1;
  }
  writer->data_size += n;
  return 0;
}

static int write_header(fit_writer_t *writer) {
  uint8_t header[14];
  size_t n = fit_write_header(header, writer->data_size);
  return fwrite(header, 1, n, writer->file) == n ? 0 : -1;
}

static int append_file_crc(fit_writer_t *writer) {
  uint8_t chunk[CRC_CHUNK_SIZE];
  uint16_t crc = 0;
  size_t n;

  rewind(writer->file);
  while ((n = fread(chunk, 1, sizeof(chunk), writer->file)) > 0) {
    crc = fit_crc16(crc, chunk, n);
  }
  if (ferror(writer->file)) {
    return -1;
  }

  fseek(writer->file, 0, SEEK_END);
  uint8_t out[2];
  fit_write_crc(out, crc);
  return fwrite(out, 1, sizeof(out), writer->file) == sizeof(out) ? 0 : -1;
}

int fit_writer_open(fit_writer_t *writer, const char *path,
                    uint32_t time_created) {
  uint8_t buf[MAX_MESSAGE_SIZE];

  writer->data_size = 0;
  writer->start_time = time_created;
  writer->last_time = time_created;
  writer->last_distance_cm = 0;
  writer->max_speed_mm_s = 0;
  writer->file = fopen(path, "w+b");
  if (!writer->file) {
    return -1;
  }

  if (write_header(writer) != 0 ||
      write_message(writer, buf, fit_write_file_id_definition(buf)) != 0 ||
      write_message(writer, buf, fit_write_file_id_data(buf, time_created)) !=
          0 ||
      write_message(writer, buf, fit_write_event_definition(buf)) != 0 ||
      write_message(writer, buf,
                    fit_write_event_data(buf, time_created, FIT_EVENT_START)) !=
          0 ||
      write_message(writer, buf, fit_write_record_definition(buf)) != 0) {
    fclose(writer->file);
    writer->file = NULL;
    return -1;
  }
  return 0;
}

int fit_writer_add_record(fit_writer_t *writer, const fit_record_t *record) {
  uint8_t buf[MAX_MESSAGE_SIZE];
  if (write_message(writer, buf, fit_write_record_data(buf, record)) != 0) {
    return -1;
  }
  writer->last_time = record->timestamp;
  writer->last_distance_cm = record->distance_cm;
  if (record->speed_mm_s != FIT_INVALID_UINT16 &&
      record->speed_mm_s > writer->max_speed_mm_s) {
    writer->max_speed_mm_s = record->speed_mm_s;
  }
  return 0;
}

static int write_summary(fit_writer_t *writer) {
  uint8_t buf[MAX_MESSAGE_SIZE];
  uint32_t elapsed_s = writer->last_time - writer->start_time;
  fit_summary_t summary = {
      .start_time = writer->start_time,
      .end_time = writer->last_time,
      .distance_cm = writer->last_distance_cm,
      .avg_speed_mm_s =
          elapsed_s ? (uint16_t)(writer->last_distance_cm * 10 / elapsed_s) : 0,
      .max_speed_mm_s = writer->max_speed_mm_s,
  };

  return write_message(writer, buf,
                       fit_write_event_data(buf, summary.end_time,
                                            FIT_EVENT_STOP_ALL)) ||
         write_message(writer, buf, fit_write_lap_definition(buf)) ||
         write_message(writer, buf, fit_write_lap_data(buf, &summary)) ||
         write_message(writer, buf, fit_write_session_definition(buf)) ||
         write_message(writer, buf, fit_write_session_data(buf, &summary)) ||
         write_message(writer, buf, fit_write_activity_definition(buf)) ||
         write_message(writer, buf, fit_write_activity_data(buf, &summary));
}

int fit_writer_close(fit_writer_t *writer) {
  int result = 0;

  if (write_summary(writer) != 0 || fflush(writer->file) != 0 || fseek(writer->file, 0, SEEK_SET) != 0 ||
      write_header(writer) != 0 || append_file_crc(writer) != 0) {
    result = -1;
  }
  if (fclose(writer->file) != 0) {
    result = -1;
  }
  writer->file = NULL;
  return result;
}
