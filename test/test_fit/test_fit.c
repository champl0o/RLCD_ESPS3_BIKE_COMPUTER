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
  TEST_ASSERT_EQUAL_HEX_ARRAY(expected, out, 14);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_put_u32_is_little_endian);
  RUN_TEST(test_put_u32_returns_correct_value);
  RUN_TEST(test_put_u32_leaves_other_bytes_unchanged);

  RUN_TEST(test_put_u16_is_little_endian);
  RUN_TEST(test_put_u16_returns_correct_value);
  RUN_TEST(test_put_u16_leaves_other_bytes_unchanged);

  RUN_TEST(test_crc16_header_matches_reference);
  RUN_TEST(test_crc16_header_split_calculates_correctly);

  RUN_TEST(test_header_matches_real_life);
  return UNITY_END();
}
