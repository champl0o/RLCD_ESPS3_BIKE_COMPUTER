#include "../../components/nmea/nmea.c"
#include "unity.h"
#include <string.h>

/* Канонічні приклади NMEA з перерахованими контрольними сумами. */
#define RMC_FIX                                                                \
  "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A"
#define RMC_VOID "$GPRMC,123519,V,,,,,,,230394,,*33"
#define RMC_SW "$GPRMC,010203,A,3351.500,S,15112.000,W,000.0,000.0,010125,,*12"
#define RMC_NO_TIME "$GPRMC,,A,4807.038,N,01131.000,E,010.0,,,,*30"
#define RMC_NO_LAT "$GPRMC,123519,A,,N,01131.000,E,010.0,084.4,230394,,*0A"
#define GGA_GOOD "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47"
#define GGA_BAD_HDOP                                                           \
  "$GPGGA,123519,4807.038,N,01131.000,E,1,03,5.2,545.4,M,46.9,M,,*42"
#define GGA_NO_FIX "$GPGGA,123519,,,,,0,00,99.9,,M,,M,,*7C"
#define GN_RMC                                                                 \
  "$GNRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*74"

void setUp(void) {}

void tearDown(void) {}

void test_checksum_accepts_canonical_sentence(void) {
  TEST_ASSERT_TRUE(nmea_checksum_ok(RMC_FIX));
}

void test_checksum_accepts_lowercase_hex(void) {
  TEST_ASSERT_TRUE(nmea_checksum_ok(
      "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6a"));
}

void test_checksum_rejects_wrong_digit(void) {
  TEST_ASSERT_FALSE(nmea_checksum_ok(
      "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6B"));
}

void test_checksum_rejects_corrupted_payload(void) {
  TEST_ASSERT_FALSE(nmea_checksum_ok(
      "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230395,003.1,W*6A"));
}

void test_checksum_rejects_missing_star_and_bad_input(void) {
  TEST_ASSERT_FALSE(nmea_checksum_ok("$GPRMC,123519,A"));
  TEST_ASSERT_FALSE(nmea_checksum_ok("GPRMC,123519,A*00"));
  TEST_ASSERT_FALSE(nmea_checksum_ok("$GPRMC,123519*"));
  TEST_ASSERT_FALSE(nmea_checksum_ok(""));
  TEST_ASSERT_FALSE(nmea_checksum_ok(NULL));
}

void test_rmc_with_fix_parses_position_time_and_speed(void) {
  nmea_rmc_t r;
  TEST_ASSERT_TRUE(nmea_parse_rmc(RMC_FIX, &r));
  TEST_ASSERT_TRUE(r.fix);
  TEST_ASSERT_TRUE(r.time_valid);
  TEST_ASSERT_DOUBLE_WITHIN(1e-4, 48.1173, r.lat_deg);
  TEST_ASSERT_DOUBLE_WITHIN(1e-4, 11.516667, r.lon_deg);
  TEST_ASSERT_FLOAT_WITHIN(0.05, 41.48, r.speed_kmh); /* 22.4 уз × 1.852 */
  TEST_ASSERT_EQUAL_UINT8(12, r.hh);
  TEST_ASSERT_EQUAL_UINT8(35, r.mm);
  TEST_ASSERT_EQUAL_UINT8(19, r.ss);
  TEST_ASSERT_EQUAL_UINT8(23, r.day);
  TEST_ASSERT_EQUAL_UINT8(3, r.month);
  TEST_ASSERT_EQUAL_UINT16(2094, r.year);
}

void test_rmc_void_has_no_fix_but_keeps_date_and_time(void) {
  nmea_rmc_t r;
  TEST_ASSERT_TRUE(nmea_parse_rmc(RMC_VOID, &r));
  TEST_ASSERT_FALSE(r.fix);
  TEST_ASSERT_TRUE(r.time_valid);
  TEST_ASSERT_EQUAL_UINT8(12, r.hh);
}

void test_rmc_south_and_west_are_negative(void) {
  nmea_rmc_t r;
  TEST_ASSERT_TRUE(nmea_parse_rmc(RMC_SW, &r));
  TEST_ASSERT_DOUBLE_WITHIN(1e-4, -33.858333, r.lat_deg);
  TEST_ASSERT_DOUBLE_WITHIN(1e-4, -151.2, r.lon_deg);
  TEST_ASSERT_FLOAT_WITHIN(0.01, 0.0, r.speed_kmh);
  TEST_ASSERT_EQUAL_UINT16(2025, r.year);
}

void test_rmc_without_time_is_still_a_fix_but_time_is_invalid(void) {
  nmea_rmc_t r;
  TEST_ASSERT_TRUE(nmea_parse_rmc(RMC_NO_TIME, &r));
  TEST_ASSERT_TRUE(r.fix);
  TEST_ASSERT_FALSE(r.time_valid);
}

void test_rmc_fix_without_latitude_is_rejected(void) {
  nmea_rmc_t r;
  TEST_ASSERT_FALSE(nmea_parse_rmc(RMC_NO_LAT, &r));
}

void test_rmc_rejects_bad_input_and_leaves_output_untouched(void) {
  nmea_rmc_t r = {.lat_deg = 1.5};
  TEST_ASSERT_FALSE(nmea_parse_rmc(
      "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*00",
      &r));
  TEST_ASSERT_FALSE(nmea_parse_rmc(GGA_GOOD, &r)); /* чужий тип речення */
  TEST_ASSERT_FALSE(nmea_parse_rmc("$GPRMC,123519,A,4807.038", &r));
  TEST_ASSERT_FALSE(nmea_parse_rmc(NULL, &r));
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.5, r.lat_deg);
}

void test_rmc_rejects_overlong_line(void) {
  char line[200];
  memset(line, 'A', sizeof(line) - 1);
  line[sizeof(line) - 1] = '\0';
  memcpy(line, "$GPRMC,", 7);
  nmea_rmc_t r;
  TEST_ASSERT_FALSE(nmea_parse_rmc(line, &r));
}

void test_gga_good_hdop_is_a_fix(void) {
  nmea_gga_t g;
  TEST_ASSERT_TRUE(nmea_parse_gga(GGA_GOOD, &g));
  TEST_ASSERT_TRUE(g.fix);
  TEST_ASSERT_EQUAL_UINT8(1, g.fix_quality);
  TEST_ASSERT_EQUAL_UINT8(8, g.satellites);
  TEST_ASSERT_FLOAT_WITHIN(0.01, 0.9, g.hdop);
}

void test_gga_high_hdop_is_not_a_fix(void) {
  nmea_gga_t g;
  TEST_ASSERT_TRUE(nmea_parse_gga(GGA_BAD_HDOP, &g));
  TEST_ASSERT_FALSE(g.fix);
  TEST_ASSERT_EQUAL_UINT8(3, g.satellites);
}

void test_gga_without_fix_reports_quality_zero(void) {
  nmea_gga_t g;
  TEST_ASSERT_TRUE(nmea_parse_gga(GGA_NO_FIX, &g));
  TEST_ASSERT_EQUAL_UINT8(0, g.fix_quality);
  TEST_ASSERT_EQUAL_UINT8(0, g.satellites);
}

void test_gga_rejects_bad_input(void) {
  nmea_gga_t g;
  TEST_ASSERT_FALSE(nmea_parse_gga(RMC_FIX, &g));
  TEST_ASSERT_FALSE(nmea_parse_gga(NULL, &g));
  TEST_ASSERT_FALSE(nmea_parse_gga(
      "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*00",
      &g));
}

/* Відома межа: приймаємо лише talker "GP". Приймачі з кількома системами
 * (GLONASS, Galileo) шлють "$GN...", їх зараз ігноруємо. */
void test_gn_talker_is_accepted(void) {
  TEST_IGNORE_MESSAGE("known limitation: only $GPRMC/$GPGGA are parsed");
  nmea_rmc_t r;
  TEST_ASSERT_TRUE(nmea_parse_rmc(GN_RMC, &r));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_checksum_accepts_canonical_sentence);
  RUN_TEST(test_checksum_accepts_lowercase_hex);
  RUN_TEST(test_checksum_rejects_wrong_digit);
  RUN_TEST(test_checksum_rejects_corrupted_payload);
  RUN_TEST(test_checksum_rejects_missing_star_and_bad_input);
  RUN_TEST(test_rmc_with_fix_parses_position_time_and_speed);
  RUN_TEST(test_rmc_void_has_no_fix_but_keeps_date_and_time);
  RUN_TEST(test_rmc_south_and_west_are_negative);
  RUN_TEST(test_rmc_without_time_is_still_a_fix_but_time_is_invalid);
  RUN_TEST(test_rmc_fix_without_latitude_is_rejected);
  RUN_TEST(test_rmc_rejects_bad_input_and_leaves_output_untouched);
  RUN_TEST(test_rmc_rejects_overlong_line);
  RUN_TEST(test_gga_good_hdop_is_a_fix);
  RUN_TEST(test_gga_high_hdop_is_not_a_fix);
  RUN_TEST(test_gga_without_fix_reports_quality_zero);
  RUN_TEST(test_gga_rejects_bad_input);
  RUN_TEST(test_gn_talker_is_accepted);
  return UNITY_END();
}
