#include "../../components/ble_parse/ble_parse.c"
#include "unity.h"

void setUp(void) {}

void tearDown(void) {}

void test_uint8_format_reads_one_byte_bpm(void) {
  const uint8_t pkt[] = {0x00, 72};
  hr_data_t hr;
  TEST_ASSERT_TRUE(hr_parse(pkt, sizeof(pkt), &hr));
  TEST_ASSERT_EQUAL_UINT16(72, hr.bpm);
  TEST_ASSERT_FALSE(hr.contact_supported);
  TEST_ASSERT_FALSE(hr.contact_detected);
}

void test_uint16_format_reads_little_endian_bpm(void) {
  const uint8_t pkt[] = {0x01, 0x2C, 0x01};
  hr_data_t hr;
  TEST_ASSERT_TRUE(hr_parse(pkt, sizeof(pkt), &hr));
  TEST_ASSERT_EQUAL_UINT16(300, hr.bpm);
}

void test_contact_detected_matches_blehr_example(void) {
  const uint8_t pkt[] = {0x06, 90};
  hr_data_t hr;
  TEST_ASSERT_TRUE(hr_parse(pkt, sizeof(pkt), &hr));
  TEST_ASSERT_EQUAL_UINT16(90, hr.bpm);
  TEST_ASSERT_TRUE(hr.contact_supported);
  TEST_ASSERT_TRUE(hr.contact_detected);
}

void test_contact_supported_but_not_detected(void) {
  const uint8_t pkt[] = {0x04, 60};
  hr_data_t hr;
  TEST_ASSERT_TRUE(hr_parse(pkt, sizeof(pkt), &hr));
  TEST_ASSERT_TRUE(hr.contact_supported);
  TEST_ASSERT_FALSE(hr.contact_detected);
}

void test_contact_value_one_means_not_supported(void) {
  const uint8_t pkt[] = {0x02, 60};
  hr_data_t hr;
  TEST_ASSERT_TRUE(hr_parse(pkt, sizeof(pkt), &hr));
  TEST_ASSERT_FALSE(hr.contact_supported);
  TEST_ASSERT_FALSE(hr.contact_detected);
}

void test_energy_expended_flag_does_not_shift_bpm(void) {
  const uint8_t pkt[] = {0x08, 80, 0x10, 0x00};
  hr_data_t hr;
  TEST_ASSERT_TRUE(hr_parse(pkt, sizeof(pkt), &hr));
  TEST_ASSERT_EQUAL_UINT16(80, hr.bpm);
}

void test_truncated_uint16_packet_is_rejected(void) {
  const uint8_t pkt[] = {0x01, 0x2C};
  hr_data_t hr;
  TEST_ASSERT_FALSE(hr_parse(pkt, sizeof(pkt), &hr));
}

void test_flags_only_packet_is_rejected(void) {
  const uint8_t pkt[] = {0x00};
  hr_data_t hr;
  TEST_ASSERT_FALSE(hr_parse(pkt, sizeof(pkt), &hr));
}

void test_empty_and_null_inputs_are_rejected(void) {
  const uint8_t pkt[] = {0x00, 72};
  hr_data_t hr;
  TEST_ASSERT_FALSE(hr_parse(pkt, 0, &hr));
  TEST_ASSERT_FALSE(hr_parse(NULL, 2, &hr));
  TEST_ASSERT_FALSE(hr_parse(pkt, sizeof(pkt), NULL));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_uint8_format_reads_one_byte_bpm);
  RUN_TEST(test_uint16_format_reads_little_endian_bpm);
  RUN_TEST(test_contact_detected_matches_blehr_example);
  RUN_TEST(test_contact_supported_but_not_detected);
  RUN_TEST(test_contact_value_one_means_not_supported);
  RUN_TEST(test_energy_expended_flag_does_not_shift_bpm);
  RUN_TEST(test_truncated_uint16_packet_is_rejected);
  RUN_TEST(test_flags_only_packet_is_rejected);
  RUN_TEST(test_empty_and_null_inputs_are_rejected);
  return UNITY_END();
}
