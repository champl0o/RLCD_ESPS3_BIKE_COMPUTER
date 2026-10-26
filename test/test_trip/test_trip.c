#include "../../components/trip/trip.c"
#include "unity.h"

void setUp(void) { trip_reset(); }

void tearDown(void) {}

void test_trip_moving_adds_distance(void) {
  trip_update(50.0, 50.0, 10.0, 0.0, true);
  trip_update(50.0, 50.1, 10.0, 10.0, true);
  const trip_t *trip_data = trip_get();
  TEST_ASSERT_NOT_NULL(trip_data);
  TEST_ASSERT(trip_data->dist_km > 0.0);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_trip_moving_adds_distance);
  return UNITY_END();
}
