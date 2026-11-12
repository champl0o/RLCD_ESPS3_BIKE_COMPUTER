#include "../../components/button/gesture.c"
#include "unity.h"

#define T0 1000000LL

static btn_state_t b;

void setUp(void) {
  b = (btn_state_t){.state = IDLE, .gesture_deadline_us = NO_DEADLINE};
}

void tearDown(void) {}

/* Кожен рядок таблиці переходів окремо. */

void test_idle_press_goes_down_and_arms_long_press_deadline(void) {
  TEST_ASSERT_EQUAL(GESTURE_NONE, handle_event(&b, PRESS, T0));
  TEST_ASSERT_EQUAL(DOWN, b.state);
  TEST_ASSERT_EQUAL_INT64(T0 + LONG_PRESS_TIME_US, b.gesture_deadline_us);
}

void test_down_timeout_is_long_press(void) {
  b.state = DOWN;
  TEST_ASSERT_EQUAL(PRESS_LONG, handle_event(&b, TIMEOUT, T0));
  TEST_ASSERT_EQUAL(LONG_DOWN, b.state);
  TEST_ASSERT_EQUAL_INT64(NO_DEADLINE, b.gesture_deadline_us);
}

void test_down_release_waits_for_second_click(void) {
  b.state = DOWN;
  TEST_ASSERT_EQUAL(GESTURE_NONE, handle_event(&b, RELEASE, T0));
  TEST_ASSERT_EQUAL(WAIT_2ND, b.state);
  TEST_ASSERT_EQUAL_INT64(T0 + SECOND_CLICK_TIME_US, b.gesture_deadline_us);
}

void test_wait_2nd_timeout_is_short_press(void) {
  b.state = WAIT_2ND;
  TEST_ASSERT_EQUAL(PRESS_SHORT, handle_event(&b, TIMEOUT, T0));
  TEST_ASSERT_EQUAL(IDLE, b.state);
  TEST_ASSERT_EQUAL_INT64(NO_DEADLINE, b.gesture_deadline_us);
}

void test_wait_2nd_press_is_double(void) {
  b.state = WAIT_2ND;
  TEST_ASSERT_EQUAL(PRESS_DOUBLE, handle_event(&b, PRESS, T0));
  TEST_ASSERT_EQUAL(DOUBLE_DOWN, b.state);
  TEST_ASSERT_EQUAL_INT64(NO_DEADLINE, b.gesture_deadline_us);
}

void test_release_after_long_press_returns_to_idle_silently(void) {
  b.state = LONG_DOWN;
  TEST_ASSERT_EQUAL(GESTURE_NONE, handle_event(&b, RELEASE, T0));
  TEST_ASSERT_EQUAL(IDLE, b.state);
}

void test_release_after_double_returns_to_idle_silently(void) {
  b.state = DOUBLE_DOWN;
  TEST_ASSERT_EQUAL(GESTURE_NONE, handle_event(&b, RELEASE, T0));
  TEST_ASSERT_EQUAL(IDLE, b.state);
}

/* Події, яких у таблиці для цього стану немає, нічого не міняють. */

void test_unlisted_events_are_ignored(void) {
  struct {
    btn_state_enum_t state;
    btn_event_type_t event;
  } ignored[] = {
      {IDLE, RELEASE},      {IDLE, TIMEOUT},      {DOWN, PRESS},
      {LONG_DOWN, PRESS},   {LONG_DOWN, TIMEOUT}, {WAIT_2ND, RELEASE},
      {DOUBLE_DOWN, PRESS}, {DOUBLE_DOWN, TIMEOUT},
  };

  for (size_t i = 0; i < sizeof(ignored) / sizeof(ignored[0]); i++) {
    b = (btn_state_t){.state = ignored[i].state,
                      .gesture_deadline_us = 12345};
    TEST_ASSERT_EQUAL(GESTURE_NONE, handle_event(&b, ignored[i].event, T0));
    TEST_ASSERT_EQUAL(ignored[i].state, b.state);
    TEST_ASSERT_EQUAL_INT64(12345, b.gesture_deadline_us);
  }
}

/* Повні жести: порядок подій такий, як його видасть button.c. */

void test_full_short_click(void) {
  handle_event(&b, PRESS, T0);
  handle_event(&b, RELEASE, T0 + 80000);
  TEST_ASSERT_EQUAL(PRESS_SHORT,
                    handle_event(&b, TIMEOUT, T0 + 80000 + SECOND_CLICK_TIME_US));
  TEST_ASSERT_EQUAL(IDLE, b.state);
}

void test_full_long_press_then_release(void) {
  handle_event(&b, PRESS, T0);
  TEST_ASSERT_EQUAL(PRESS_LONG,
                    handle_event(&b, TIMEOUT, T0 + LONG_PRESS_TIME_US));
  TEST_ASSERT_EQUAL(GESTURE_NONE,
                    handle_event(&b, RELEASE, T0 + LONG_PRESS_TIME_US + 100));
  TEST_ASSERT_EQUAL(IDLE, b.state);
}

void test_full_double_click(void) {
  handle_event(&b, PRESS, T0);
  handle_event(&b, RELEASE, T0 + 60000);
  TEST_ASSERT_EQUAL(PRESS_DOUBLE, handle_event(&b, PRESS, T0 + 150000));
  handle_event(&b, RELEASE, T0 + 210000);
  TEST_ASSERT_EQUAL(IDLE, b.state);
}

void test_state_machine_is_reusable_after_a_gesture(void) {
  handle_event(&b, PRESS, T0);
  handle_event(&b, TIMEOUT, T0 + LONG_PRESS_TIME_US);
  handle_event(&b, RELEASE, T0 + 2 * LONG_PRESS_TIME_US);
  handle_event(&b, PRESS, T0 + 3 * LONG_PRESS_TIME_US);
  handle_event(&b, RELEASE, T0 + 3 * LONG_PRESS_TIME_US + 50000);
  TEST_ASSERT_EQUAL(PRESS_SHORT,
                    handle_event(&b, TIMEOUT,
                                 T0 + 3 * LONG_PRESS_TIME_US + 50000 +
                                     SECOND_CLICK_TIME_US));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_idle_press_goes_down_and_arms_long_press_deadline);
  RUN_TEST(test_down_timeout_is_long_press);
  RUN_TEST(test_down_release_waits_for_second_click);
  RUN_TEST(test_wait_2nd_timeout_is_short_press);
  RUN_TEST(test_wait_2nd_press_is_double);
  RUN_TEST(test_release_after_long_press_returns_to_idle_silently);
  RUN_TEST(test_release_after_double_returns_to_idle_silently);
  RUN_TEST(test_unlisted_events_are_ignored);
  RUN_TEST(test_full_short_click);
  RUN_TEST(test_full_long_press_then_release);
  RUN_TEST(test_full_double_click);
  RUN_TEST(test_state_machine_is_reusable_after_a_gesture);
  return UNITY_END();
}
