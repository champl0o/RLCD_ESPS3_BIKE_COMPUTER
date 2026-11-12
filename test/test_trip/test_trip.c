#include "../../components/trip/trip.c"
#include "unity.h"

/* 1 m along a meridian ≈ 8.9932e-6° of latitude (Earth radius 6371 km). */
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
  /* 36 km/h = 10 m/s, ten seconds of riding = 100 m. */
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

/* Known limit: after losing the fix the very first point adds the whole gap to
 * moving_time, although we do not know whether we were riding. A gap policy is needed. */
void test_long_fix_gap_is_not_counted_as_moving_time(void) {
  TEST_IGNORE_MESSAGE("known limitation: no policy for a lost-fix gap yet");
  trip_update(50.0, 30.0, 36.0, 0.0, true);
  trip_update(50.0 + DEG_PER_10_M, 30.0, 36.0, 1.0, true);
  trip_update(50.0 + 2 * DEG_PER_10_M, 30.0, 36.0, 61.0, true);
  TEST_ASSERT_DOUBLE_WITHIN(2.0, 2.0, trip_get()->moving_time);
}

/* Pause: after trip_break the first fix only sets the anchor point, so the duration
 * of the pause and the distance to where riding resumed do not end up in the totals. */
void test_trip_break_reanchors_without_adding_the_pause(void) {
  /* 72 km/h = 20 m/s: 20 m every second. */
  trip_update(50.0, 30.0, 72.0, 0.0, true);
  for (int s = 1; s <= 5; s++)
    trip_update(50.0 + s * 2 * DEG_PER_10_M, 30.0, 72.0, s, true);
  const double dist_before = trip_get()->dist_km;
  const double moving_before = trip_get()->moving_time;

  trip_break();
  /* After 60 s, 11 km from the previous point. */
  trip_update(50.1, 30.0, 72.0, 65.0, true);
  TEST_ASSERT_EQUAL_DOUBLE(dist_before, trip_get()->dist_km);
  TEST_ASSERT_EQUAL_DOUBLE(moving_before, trip_get()->moving_time);

  /* Counting then continues as usual from the new point. */
  trip_update(50.1 + 2 * DEG_PER_10_M, 30.0, 72.0, 66.0, true);
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, moving_before + 1.0, trip_get()->moving_time);
  TEST_ASSERT_DOUBLE_WITHIN(0.002, dist_before + 0.020, trip_get()->dist_km);
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
  RUN_TEST(test_trip_break_reanchors_without_adding_the_pause);
  RUN_TEST(test_long_fix_gap_is_not_counted_as_moving_time);
  return UNITY_END();
}
