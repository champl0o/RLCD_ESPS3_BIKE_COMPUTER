#include "../../components/fit/fit.c"
#include "unity.h"
#include <stddef.h>

void setUp(void) {}

void tearDown(void) {}

void test_put_u32_is_little_endian(void) {
  uint8_t buf[4];
  put_u32(buf, 0x01020304);
  TEST_ASSERT_EQUAL_HEX8(0x04, buf[0]);
  TEST_ASSERT_EQUAL_HEX8(0x03, buf[1]);
  TEST_ASSERT_EQUAL_HEX8(0x02, buf[2]);
  TEST_ASSERT_EQUAL_HEX8(0x01, buf[3]);
}

void test_put_u32_returns_correct_value(void) {
  uint8_t buf[4];
  size_t value = put_u32(buf, 0x01020304);
  TEST_ASSERT_EQUAL_HEX32(4, value);
}

void test_put_u32_leaves_other_bytes_unchanged(void) {
  uint8_t buf[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
  put_u32(buf, 0x01020304);
  TEST_ASSERT_EQUAL_HEX8(0x04, buf[0]);
  TEST_ASSERT_EQUAL_HEX8(0x03, buf[1]);
  TEST_ASSERT_EQUAL_HEX8(0x02, buf[2]);
  TEST_ASSERT_EQUAL_HEX8(0x01, buf[3]);
  TEST_ASSERT_EQUAL_HEX8(0xEE, buf[4]);
  TEST_ASSERT_EQUAL_HEX8(0xFF, buf[5]);
}

void test_put_u16_is_little_endian(void) {
  uint8_t buf[2];
  put_u16(buf, 0x0102);
  TEST_ASSERT_EQUAL_HEX8(0x02, buf[0]);
  TEST_ASSERT_EQUAL_HEX8(0x01, buf[1]);
}

void test_put_u16_returns_correct_value(void) {
  uint8_t buf[2];
  size_t value = put_u16(buf, 0x0102);
  TEST_ASSERT_EQUAL_HEX32(2, value);
}

void test_put_u16_leaves_other_bytes_unchanged(void) {
  uint8_t buf[4] = {0xAA, 0xBB, 0xCC, 0xDD};
  put_u16(buf, 0x0102);
  TEST_ASSERT_EQUAL_HEX8(0x02, buf[0]);
  TEST_ASSERT_EQUAL_HEX8(0x01, buf[1]);
  TEST_ASSERT_EQUAL_HEX8(0xCC, buf[2]);
  TEST_ASSERT_EQUAL_HEX8(0xDD, buf[3]);
}

void test_crc16_header_matches_reference(void) {
  const uint8_t header[] = {0x0e, 0x20, 0xa6, 0x52, 0x65, 0xe9,
                            0x02, 0x00, 0x2e, 0x46, 0x49, 0x54};
  TEST_ASSERT_EQUAL_HEX16(0x4020, fit_crc16(0, header, sizeof(header)));
}

void test_crc16_header_split_calculates_correctly(void) {
  const uint8_t header[] = {0x0e, 0x20, 0xa6, 0x52, 0x65, 0xe9,
                            0x02, 0x00, 0x2e, 0x46, 0x49, 0x54};
  uint16_t crc = 0;
  crc = fit_crc16(crc, header, 6);
  crc = fit_crc16(crc, header + 6, 6);
  TEST_ASSERT_EQUAL_HEX16(0x4020, crc);
}

void test_header_matches_real_life(void) {
  const uint8_t expected[14] = {0x0e, 0x20, 0xa6, 0x52, 0x65, 0xe9, 0x02,
                                0x00, 0x2e, 0x46, 0x49, 0x54, 0x20, 0x40};
  uint8_t out[14];

  size_t n = fit_write_header(out, 190821);
  TEST_ASSERT_EQUAL(14, n);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, out, 14);
}

void test_write_crc_is_little_endian(void) {
  uint8_t buf[2];

  size_t n = fit_write_crc(buf, 0x4020);
  TEST_ASSERT_EQUAL(2, n);
  TEST_ASSERT_EQUAL_HEX8(0x20, buf[0]);
  TEST_ASSERT_EQUAL_HEX8(0x40, buf[1]);
}

void test_crc_over_file_with_appended_crc_is_zero(void) {
  uint8_t file[20];
  size_t n = fit_write_header(file, 4);
  const uint8_t data[4] = {0x01, 0x02, 0x03, 0x04};
  memcpy(file + n, data, sizeof(data));
  n += sizeof(data);
  n += fit_write_crc(file + n, fit_crc16(0, file, n));

  TEST_ASSERT_EQUAL_HEX16(0, fit_crc16(0, file, n));
}

void test_file_id_definition_matches_reference(void) {
  const uint8_t expected[] = {0x40, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00,
                              0x01, 0x00, 0x01, 0x02, 0x84, 0x04, 0x04,
                              0x86, 0x02, 0x02, 0x84, 0x08, 0x0d, 0x07};
  uint8_t out[sizeof(expected)];

  size_t n = fit_write_file_id_definition(out);
  TEST_ASSERT_EQUAL(sizeof(expected), n);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, out, sizeof(expected));
}

void test_file_id_data_follows_definition_order(void) {
  const uint8_t expected[] = {0x00, 0x04, 0xff, 0x00, 0x78, 0xbe, 0xf6, 0x44,
                              0x00, 0x00, 'R',  'L',  'C',  'D',  ' ',  'B',
                              'i',  'k',  'e',  0x00, 0x00, 0x00, 0x00};
  uint8_t out[sizeof(expected)];

  size_t n = fit_write_file_id_data(out, 0x44F6BE78);
  TEST_ASSERT_EQUAL(sizeof(expected), n);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, out, sizeof(expected));
}

void test_degrees_to_semicircles_50_degrees(void) {
  TEST_ASSERT_EQUAL_HEX32(0x238E38E3, fit_degrees_to_semicircles(50.0));
}

void test_degrees_to_semicircles_south_is_negative(void) {
  TEST_ASSERT_EQUAL_INT32(-fit_degrees_to_semicircles(50.0),
                          fit_degrees_to_semicircles(-50.0));
}

void test_record_definition_matches_spec(void) {
  const uint8_t expected[] = {0x41, 0x00, 0x00, 0x14, 0x00, 0x07, 0xfd, 0x04,
                              0x86, 0x00, 0x04, 0x85, 0x01, 0x04, 0x85, 0x05,
                              0x04, 0x86, 0x06, 0x02, 0x84, 0x03, 0x01, 0x02,
                              0x04, 0x01, 0x02};
  uint8_t out[sizeof(expected)];

  size_t n = fit_write_record_definition(out);
  TEST_ASSERT_EQUAL(sizeof(expected), n);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, out, sizeof(expected));
}

void test_record_data_follows_definition_order(void) {
  const uint8_t expected[] = {0x01, 0x78, 0xbe, 0xf6, 0x44, 0xe3, 0x38,
                              0x8e, 0x23, 0x05, 0x5b, 0xb0, 0x15, 0x40,
                              0xe2, 0x01, 0x00, 0x4c, 0x1d, 0xff, 0x5a};
  const fit_record_t rec = {
      .timestamp = 0x44F6BE78,
      .lat = fit_degrees_to_semicircles(50.0),
      .lon = fit_degrees_to_semicircles(30.5),
      .distance_cm = 123456,
      .speed_mm_s = 7500,
      .heart_rate = FIT_INVALID_UINT8,
      .cadence = 90,
  };
  uint8_t out[sizeof(expected)];

  size_t n = fit_write_record_data(out, &rec);
  TEST_ASSERT_EQUAL(sizeof(expected), n);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, out, sizeof(expected));
}

void test_record_data_size_matches_definition(void) {
  uint8_t def[32], data[32];
  const fit_record_t rec = {0};

  size_t def_n = fit_write_record_definition(def);
  size_t fields = def[5];
  size_t expected = 1;
  for (size_t i = 0; i < fields; i++) {
    expected += def[6 + 3 * i + 1];
  }

  TEST_ASSERT_EQUAL(6 + 3 * fields, def_n);
  TEST_ASSERT_EQUAL(expected, fit_write_record_data(data, &rec));
}

// Below are values from a real file from COROS PACE 3 (Downloads/479985917885448196.fit)

void test_record_data_matches_real_ride_point(void) {
  // a record from the sample `09 02 f1 8b f6 44 e9 73 6a 23 9d 49 11 11 f0 43 03 00 86
  // 3a 10 ff 0e`, the same values in our field order, the sample has no cadence
  const uint8_t expected[] = {0x01, 0xf1, 0x8b, 0xf6, 0x44, 0xe9, 0x73,
                              0x6a, 0x23, 0x9d, 0x49, 0x11, 0x11, 0xf0, 0x43,
                              0x03, 0x00, 0xff, 0x0e, 0x86, 0xff};
  const fit_record_t rec = {
      .timestamp = 1157008369,
      .lat = 594179049,
      .lon = 286345629,
      .distance_cm = 214000,
      .speed_mm_s = 3839,
      .heart_rate = 134,
      .cadence = FIT_INVALID_UINT8,
  };
  uint8_t out[sizeof(expected)];

  size_t n = fit_write_record_data(out, &rec);
  TEST_ASSERT_EQUAL(sizeof(expected), n);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, out, sizeof(expected));
}

void test_record_data_without_gps_fix_matches_real_sample(void) {
  // a record without a fix from the sample: lat/lon = 0x7FFFFFFF
  const uint8_t expected[] = {0x01, 0x42, 0x8a, 0xf6, 0x44, 0xff, 0xff,
                              0xff, 0x7f, 0xff, 0xff, 0xff, 0x7f, 0x80,
                              0x57, 0x00, 0x00, 0xff, 0x02, 0x66, 0xff};
  const fit_record_t rec = {
      .timestamp = 0x44F68A42,
      .lat = FIT_INVALID_SINT32,
      .lon = FIT_INVALID_SINT32,
      .distance_cm = 22400,
      .speed_mm_s = 0x02ff,
      .heart_rate = 102,
      .cadence = FIT_INVALID_UINT8,
  };
  uint8_t out[sizeof(expected)];

  size_t n = fit_write_record_data(out, &rec);
  TEST_ASSERT_EQUAL(sizeof(expected), n);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, out, sizeof(expected));
}

void test_semicircles_round_trip_real_coordinates(void) {
  const int32_t real[][2] = {{594348711, 286357898},
                             {592050769, 285831435},
                             {593211380, 285211966},
                             {594179049, 286345629}};

  for (size_t i = 0; i < sizeof(real) / sizeof(real[0]); i++) {
    double lat = real[i][0] * 180.0 / 2147483648.0;
    double lon = real[i][1] * 180.0 / 2147483648.0;
    TEST_ASSERT_EQUAL_INT32(real[i][0], fit_degrees_to_semicircles(lat));
    TEST_ASSERT_EQUAL_INT32(real[i][1], fit_degrees_to_semicircles(lon));
  }
}

/* Expected values computed independently (Python datetime from 1989-12-31). */
void test_timestamp_fit_epoch_is_zero(void) {
  TEST_ASSERT_EQUAL_UINT32(0, fit_timestamp_from_utc(1989, 12, 31, 0, 0, 0));
}

void test_timestamp_one_day_after_epoch(void) {
  TEST_ASSERT_EQUAL_UINT32(86400, fit_timestamp_from_utc(1990, 1, 1, 0, 0, 0));
}

void test_timestamp_year_2000(void) {
  TEST_ASSERT_EQUAL_UINT32(315619200,
                           fit_timestamp_from_utc(2000, 1, 1, 0, 0, 0));
}

void test_timestamp_leap_day_with_time(void) {
  TEST_ASSERT_EQUAL_UINT32(1078144496,
                           fit_timestamp_from_utc(2024, 2, 29, 12, 34, 56));
}

void test_timestamp_day_after_leap_day(void) {
  TEST_ASSERT_EQUAL_UINT32(1078185600,
                           fit_timestamp_from_utc(2024, 3, 1, 0, 0, 0));
}

void test_timestamp_today(void) {
  TEST_ASSERT_EQUAL_UINT32(1162512000,
                           fit_timestamp_from_utc(2026, 11, 2, 0, 0, 0));
  TEST_ASSERT_EQUAL_UINT32(1162598399,
                           fit_timestamp_from_utc(2026, 11, 2, 23, 59, 59));
}

void test_timestamp_before_fit_epoch_is_zero(void) {
  TEST_ASSERT_EQUAL_UINT32(0, fit_timestamp_from_utc(1970, 1, 1, 0, 0, 0));
}

void test_record_make_without_position_keeps_distance(void) {
  fit_record_t r = fit_record_make(100, false, 50.0, 30.0, 25.0f, 1.5, 120, 80);
  TEST_ASSERT_EQUAL_INT32(FIT_INVALID_SINT32, r.lat);
  TEST_ASSERT_EQUAL_INT32(FIT_INVALID_SINT32, r.lon);
  TEST_ASSERT_EQUAL_UINT16(FIT_INVALID_UINT16, r.speed_mm_s);
  TEST_ASSERT_EQUAL_UINT32(150000, r.distance_cm);
  TEST_ASSERT_EQUAL_UINT32(100, r.timestamp);
}

void test_record_make_with_position_converts_units(void) {
  fit_record_t r =
      fit_record_make(7, true, 50.0, -30.0, 36.0f, 2.0, FIT_INVALID_UINT8,
                      FIT_INVALID_UINT8);
  TEST_ASSERT_EQUAL_INT32(fit_degrees_to_semicircles(50.0), r.lat);
  TEST_ASSERT_EQUAL_INT32(fit_degrees_to_semicircles(-30.0), r.lon);
  TEST_ASSERT_EQUAL_UINT16(10000, r.speed_mm_s); /* 36 km/h = 10 m/s */
  TEST_ASSERT_EQUAL_UINT32(200000, r.distance_cm);
  TEST_ASSERT_EQUAL_UINT8(FIT_INVALID_UINT8, r.heart_rate);
  TEST_ASSERT_EQUAL_UINT8(FIT_INVALID_UINT8, r.cadence);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_timestamp_fit_epoch_is_zero);
  RUN_TEST(test_timestamp_one_day_after_epoch);
  RUN_TEST(test_timestamp_year_2000);
  RUN_TEST(test_timestamp_leap_day_with_time);
  RUN_TEST(test_timestamp_day_after_leap_day);
  RUN_TEST(test_timestamp_today);
  RUN_TEST(test_timestamp_before_fit_epoch_is_zero);
  RUN_TEST(test_record_make_without_position_keeps_distance);
  RUN_TEST(test_record_make_with_position_converts_units);

  RUN_TEST(test_put_u32_is_little_endian);
  RUN_TEST(test_put_u32_returns_correct_value);
  RUN_TEST(test_put_u32_leaves_other_bytes_unchanged);

  RUN_TEST(test_put_u16_is_little_endian);
  RUN_TEST(test_put_u16_returns_correct_value);
  RUN_TEST(test_put_u16_leaves_other_bytes_unchanged);

  RUN_TEST(test_crc16_header_matches_reference);
  RUN_TEST(test_crc16_header_split_calculates_correctly);

  RUN_TEST(test_header_matches_real_life);

  RUN_TEST(test_write_crc_is_little_endian);
  RUN_TEST(test_crc_over_file_with_appended_crc_is_zero);

  RUN_TEST(test_file_id_definition_matches_reference);
  RUN_TEST(test_file_id_data_follows_definition_order);

  RUN_TEST(test_degrees_to_semicircles_50_degrees);
  RUN_TEST(test_degrees_to_semicircles_south_is_negative);

  RUN_TEST(test_record_definition_matches_spec);
  RUN_TEST(test_record_data_follows_definition_order);
  RUN_TEST(test_record_data_size_matches_definition);

  RUN_TEST(test_record_data_matches_real_ride_point);
  RUN_TEST(test_record_data_without_gps_fix_matches_real_sample);
  RUN_TEST(test_semicircles_round_trip_real_coordinates);
  return UNITY_END();
}
