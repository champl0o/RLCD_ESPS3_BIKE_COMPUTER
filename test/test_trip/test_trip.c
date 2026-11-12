#include "../../components/trip/trip.c"
#include "unity.h"

/* 1 м по меридіану ≈ 8.9932e-6° широти (радіус Землі 6371 км). */
#define DEG_PER_10_M 0.0000899322

void setUp(void) { trip_reset(); }

void tearDown(void) {}

void test_trip_moving_adds_distance(void) {
  trip_update(50.0, 50.0, 10.0, 0.0, true);
  trip_update(50.0, 50.1, 10.0, 10.0, true);
  const trip_t *trip_data = trip_get();
  TEST_ASSERT_NOT_NULL(trip_data);
  TEST_ASSERT(trip_data->dist_km > 0.0);
}

void test_one_degree_of_latitude_is_about_111_km(void) {
  trip_update(50.0, 30.0, 20.0, 0.0, true);
  trip_update(51.0, 30.0, 20.0, 1.0, true);
  TEST_ASSERT_DOUBLE_WITHIN(0.5, 111.19, trip_get()->dist_km);
}

void test_distance_matches_known_ten_second_run(void) {
  /* 36 км/год = 10 м/с, десять секунд ходу = 100 м. */
  trip_update(50.0, 30.0, 36.0, 0.0, true);
  for (int s = 1; s <= 10; s++)
    trip_update(50.0 + s * DEG_PER_10_M, 30.0, 36.0, s, true);

  const trip_t *t = trip_get();
  TEST_ASSERT_DOUBLE_WITHIN(0.002, 0.100, t->dist_km);
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 10.0, t->moving_time);
  TEST_ASSERT_DOUBLE_WITHIN(1.0, 36.0, t->average_speed);
}

void test_first_fix_sets_position_without_distance_jump(void) {
  trip_update(50.0, 30.0, 30.0, 100.0, true);
  const trip_t *t = trip_get();
  TEST_ASSERT_TRUE(t->has_last_coordinates);
  TEST_ASSERT_EQUAL_DOUBLE(0.0, t->dist_km);
  TEST_ASSERT_EQUAL_DOUBLE(0.0, t->moving_time);
}

void test_invalid_fix_changes_nothing(void) {
  trip_update(50.0, 30.0, 30.0, 0.0, true);
  trip_update(60.0, 40.0, 30.0, 5.0, false);
  const trip_t *t = trip_get();
  TEST_ASSERT_EQUAL_DOUBLE(0.0, t->dist_km);
  TEST_ASSERT_EQUAL_DOUBLE(0.0, t->total_time);
}

void test_standing_still_counts_time_but_not_moving_time(void) {
  trip_update(50.0, 30.0, 0.0, 0.0, true);
  trip_update(50.0, 30.0, 0.0, 30.0, true);
  const trip_t *t = trip_get();
  TEST_ASSERT_EQUAL_DOUBLE(0.0, t->dist_km);
  TEST_ASSERT_EQUAL_DOUBLE(0.0, t->moving_time);
  TEST_ASSERT_EQUAL_DOUBLE(30.0, t->total_time);
}

void test_gps_drift_below_min_speed_does_not_add_distance(void) {
  trip_update(50.0, 30.0, 0.5, 0.0, true);
  for (int s = 1; s <= 20; s++) {
    double jitter = (s % 2 ? 1 : -1) * 0.00002;
    trip_update(50.0 + jitter, 30.0 - jitter, 0.5, s, true);
  }
  TEST_ASSERT_EQUAL_DOUBLE(0.0, trip_get()->dist_km);
}

void test_max_speed_is_tracked_only_while_moving(void) {
  trip_update(50.0, 30.0, 10.0, 0.0, true);
  trip_update(50.0 + DEG_PER_10_M, 30.0, 25.0, 1.0, true);
  trip_update(50.0 + 2 * DEG_PER_10_M, 30.0, 12.0, 2.0, true);
  trip_update(50.0 + 2 * DEG_PER_10_M, 30.0, 1.0, 3.0, true);
  TEST_ASSERT_EQUAL_FLOAT(25.0f, trip_get()->max_speed_kmh);
}

void test_reset_clears_everything(void) {
  trip_update(50.0, 30.0, 36.0, 0.0, true);
  trip_update(50.001, 30.0, 36.0, 10.0, true);
  trip_reset();
  const trip_t *t = trip_get();
  TEST_ASSERT_FALSE(t->has_last_coordinates);
  TEST_ASSERT_EQUAL_DOUBLE(0.0, t->dist_km);
  TEST_ASSERT_EQUAL_DOUBLE(0.0, t->moving_time);
}

/* Відома межа: після втрати фіксу перша ж точка додає весь пропуск до
 * moving_time, хоча ми не знаємо, чи їхали. Потрібна політика пропуску. */
void test_long_fix_gap_is_not_counted_as_moving_time(void) {
  TEST_IGNORE_MESSAGE("known limitation: no policy for a lost-fix gap yet");
  trip_update(50.0, 30.0, 36.0, 0.0, true);
  trip_update(50.0 + DEG_PER_10_M, 30.0, 36.0, 1.0, true);
  trip_update(50.0 + 2 * DEG_PER_10_M, 30.0, 36.0, 61.0, true);
  TEST_ASSERT_DOUBLE_WITHIN(2.0, 2.0, trip_get()->moving_time);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_trip_moving_adds_distance);
  RUN_TEST(test_one_degree_of_latitude_is_about_111_km);
  RUN_TEST(test_distance_matches_known_ten_second_run);
  RUN_TEST(test_first_fix_sets_position_without_distance_jump);
  RUN_TEST(test_invalid_fix_changes_nothing);
  RUN_TEST(test_standing_still_counts_time_but_not_moving_time);
  RUN_TEST(test_gps_drift_below_min_speed_does_not_add_distance);
  RUN_TEST(test_max_speed_is_tracked_only_while_moving);
  RUN_TEST(test_reset_clears_everything);
  RUN_TEST(test_long_fix_gap_is_not_counted_as_moving_time);
  return UNITY_END();
}
