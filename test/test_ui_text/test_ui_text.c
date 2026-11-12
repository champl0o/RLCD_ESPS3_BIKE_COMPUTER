#include "../../components/ui_text/ui_text.c"
#include "unity.h"

#define S 1000000LL

void setUp(void) {}
void tearDown(void) {}

/* Texts from the plan: strings in the tests are verbatim so an accidental text edit
 * on screen does not go unnoticed. */

void test_hints_per_state(void) {
  TEST_ASSERT_EQUAL_STRING("KEY hold: START REC   BOOT: Main",
                           ui_hint(UI_RIDE_READY));
  TEST_ASSERT_EQUAL_STRING("KEY x2: PAUSE   BOOT hold: STOP",
                           ui_hint(UI_RIDE_RIDING));
  TEST_ASSERT_EQUAL_STRING("KEY x2: RESUME   BOOT hold: STOP",
                           ui_hint(UI_RIDE_PAUSED));
  TEST_ASSERT_EQUAL_STRING("any key: continue", ui_hint(UI_RIDE_SUMMARY));
}

void test_banner_texts(void) {
  TEST_ASSERT_EQUAL_STRING("NO CARD", ui_banner_title(BANNER_NO_CARD));
  TEST_ASSERT_EQUAL_STRING("insert card, then hold KEY",
                           ui_banner_reason(BANNER_NO_CARD));
  TEST_ASSERT_EQUAL_STRING("NO TIME", ui_banner_title(BANNER_NO_TIME));
  TEST_ASSERT_EQUAL_STRING("waiting for GPS or RTC time",
                           ui_banner_reason(BANNER_NO_TIME));
  TEST_ASSERT_EQUAL_STRING("CANNOT START", ui_banner_title(BANNER_START_FAILED));
  TEST_ASSERT_EQUAL_STRING("recorder busy, try again",
                           ui_banner_reason(BANNER_START_FAILED));
}

/* The recorder gives no result yet, so the summary must not claim a save. */
void test_summary_title_does_not_claim_the_file_was_saved(void) {
  TEST_ASSERT_EQUAL_STRING("RIDE ENDED", ui_summary_title());
}

void test_no_banner_has_no_text(void) {
  TEST_ASSERT_NULL(ui_banner_title(BANNER_NONE));
  TEST_ASSERT_NULL(ui_banner_reason(BANNER_NONE));
}

void test_hint_visible_during_boot_grace(void) {
  TEST_ASSERT_TRUE(ui_hint_visible(UI_RIDE_READY, 5 * S, 10 * S, 0));
}

void test_hint_visible_shortly_after_a_press(void) {
  TEST_ASSERT_TRUE(
      ui_hint_visible(UI_RIDE_RIDING, 100 * S + 2900000, 10 * S, 100 * S));
}

void test_hint_hidden_exactly_three_seconds_after_a_press(void) {
  TEST_ASSERT_FALSE(ui_hint_visible(UI_RIDE_RIDING, 103 * S, 10 * S, 100 * S));
}

void test_hint_hidden_when_nothing_ever_pressed(void) {
  TEST_ASSERT_FALSE(ui_hint_visible(UI_RIDE_RIDING, 100 * S, 10 * S, 0));
}

void test_hint_hidden_for_a_press_from_the_future(void) {
  TEST_ASSERT_FALSE(ui_hint_visible(UI_RIDE_RIDING, 100 * S, 10 * S, 101 * S));
}

void test_summary_always_shows_its_hint(void) {
  TEST_ASSERT_TRUE(ui_hint_visible(UI_RIDE_SUMMARY, 500 * S, 10 * S, 0));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_hints_per_state);
  RUN_TEST(test_banner_texts);
  RUN_TEST(test_summary_title_does_not_claim_the_file_was_saved);
  RUN_TEST(test_no_banner_has_no_text);
  RUN_TEST(test_hint_visible_during_boot_grace);
  RUN_TEST(test_hint_visible_shortly_after_a_press);
  RUN_TEST(test_hint_hidden_exactly_three_seconds_after_a_press);
  RUN_TEST(test_hint_hidden_when_nothing_ever_pressed);
  RUN_TEST(test_hint_hidden_for_a_press_from_the_future);
  RUN_TEST(test_summary_always_shows_its_hint);
  return UNITY_END();
}
