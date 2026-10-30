#include "../../components/fit/fit.c"
#include "../../components/fit/fit_writer.c"
#include "unity.h"

#define TEST_PATH "fit_writer_test.fit"
#define HEADER_LEN 14
#define FILE_ID_DEF_LEN 21
#define FILE_ID_DATA_LEN 23
#define EVENT_DEF_LEN 18
#define EVENT_DATA_LEN 8
#define RECORD_DEF_LEN 27
#define RECORD_DATA_LEN 21
// event(stop) + lap + session + activity, кожне з definition
#define SUMMARY_LEN (EVENT_DATA_LEN + (33 + 28) + (39 + 31) + (27 + 18))
#define BEFORE_FIRST_RECORD                                                    \
  (HEADER_LEN + FILE_ID_DEF_LEN + FILE_ID_DATA_LEN + EVENT_DEF_LEN +           \
   EVENT_DATA_LEN + RECORD_DEF_LEN)

static uint8_t file_bytes[1024];
static size_t file_len;

void setUp(void) {}

void tearDown(void) { remove(TEST_PATH); }

static void read_back(void) {
  FILE *f = fopen(TEST_PATH, "rb");
  TEST_ASSERT_NOT_NULL(f);
  file_len = fread(file_bytes, 1, sizeof(file_bytes), f);
  fclose(f);
}

static void write_file(size_t records) {
  fit_writer_t w;
  TEST_ASSERT_EQUAL(0, fit_writer_open(&w, TEST_PATH, 0x44F6BE78));
  for (size_t i = 0; i < records; i++) {
    fit_record_t r = {.timestamp = 0x44F6BE78 + i,
                      .lat = fit_degrees_to_semicircles(50.0),
                      .lon = fit_degrees_to_semicircles(30.5),
                      .distance_cm = (uint32_t)(i * 500),
                      .speed_mm_s = 7500,
                      .heart_rate = FIT_INVALID_UINT8,
                      .cadence = FIT_INVALID_UINT8};
    TEST_ASSERT_EQUAL(0, fit_writer_add_record(&w, &r));
  }
  TEST_ASSERT_EQUAL(0, fit_writer_close(&w));
  read_back();
}

void test_file_without_records_has_expected_size(void) {
  write_file(0);
  TEST_ASSERT_EQUAL(BEFORE_FIRST_RECORD + SUMMARY_LEN + 2, file_len);
}

void test_header_data_size_matches_actual_messages(void) {
  write_file(3);
  uint32_t data_size;
  memcpy(&data_size, file_bytes + 4, sizeof(data_size));
  TEST_ASSERT_EQUAL(file_len - HEADER_LEN - 2, data_size);
}

void test_file_crc_residue_is_zero(void) {
  write_file(3);
  TEST_ASSERT_EQUAL_HEX16(0, fit_crc16(0, file_bytes, file_len));
}

void test_file_crc_residue_is_zero_across_read_chunks(void) {
  write_file(30);
  TEST_ASSERT_TRUE(file_len > 2 * CRC_CHUNK_SIZE);
  TEST_ASSERT_EQUAL_HEX16(0, fit_crc16(0, file_bytes, file_len));
}

void test_header_crc_covers_final_data_size(void) {
  write_file(3);
  uint16_t header_crc = fit_crc16(0, file_bytes, HEADER_LEN - 2);
  TEST_ASSERT_EQUAL_HEX8(header_crc & 0xFF, file_bytes[HEADER_LEN - 2]);
  TEST_ASSERT_EQUAL_HEX8(header_crc >> 8, file_bytes[HEADER_LEN - 1]);
}

void test_records_are_appended_in_order(void) {
  write_file(3);
  size_t first = BEFORE_FIRST_RECORD;
  uint32_t ts;
  for (size_t i = 0; i < 3; i++) {
    memcpy(&ts, file_bytes + first + i * RECORD_DATA_LEN + 1, sizeof(ts));
    TEST_ASSERT_EQUAL_HEX32(0x44F6BE78 + i, ts);
  }
}

void test_file_with_records_has_expected_size(void) {
  write_file(3);
  TEST_ASSERT_EQUAL(BEFORE_FIRST_RECORD + 3 * RECORD_DATA_LEN + SUMMARY_LEN + 2,
                    file_len);
}

void test_session_summary_comes_from_records(void) {
  write_file(11); // 11 записів, відстань 5000 см, 10 с між першим і останнім
  size_t activity = file_len - 2 - 18;
  uint32_t ts, timer_ms;
  memcpy(&ts, file_bytes + activity + 1, sizeof(ts));
  memcpy(&timer_ms, file_bytes + activity + 5, sizeof(timer_ms));
  TEST_ASSERT_EQUAL_HEX32(0x44F6BE78 + 10, ts);
  TEST_ASSERT_EQUAL(10000, timer_ms);

  size_t session = activity - 27 - 31;
  uint32_t distance_cm;
  uint16_t avg, max;
  memcpy(&distance_cm, file_bytes + session + 1 + 4 + 4 + 2 + 4 + 4,
         sizeof(distance_cm));
  memcpy(&avg, file_bytes + session + 1 + 4 + 4 + 2 + 4 + 4 + 4, sizeof(avg));
  memcpy(&max, file_bytes + session + 1 + 4 + 4 + 2 + 4 + 4 + 4 + 2,
         sizeof(max));
  TEST_ASSERT_EQUAL(5000, distance_cm);
  TEST_ASSERT_EQUAL(5000, avg); // 50 м за 10 с = 5 м/с
  TEST_ASSERT_EQUAL(7500, max);
}

void test_open_fails_for_unwritable_path(void) {
  fit_writer_t w;
  TEST_ASSERT_EQUAL(-1, fit_writer_open(&w, "/nonexistent-dir/x.fit", 0));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_file_without_records_has_expected_size);
  RUN_TEST(test_header_data_size_matches_actual_messages);
  RUN_TEST(test_file_crc_residue_is_zero);
  RUN_TEST(test_file_crc_residue_is_zero_across_read_chunks);
  RUN_TEST(test_header_crc_covers_final_data_size);
  RUN_TEST(test_records_are_appended_in_order);
  RUN_TEST(test_file_with_records_has_expected_size);
  RUN_TEST(test_session_summary_comes_from_records);
  RUN_TEST(test_open_fails_for_unwritable_path);
  return UNITY_END();
}
