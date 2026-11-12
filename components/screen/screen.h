#pragma once

#include "ui_text.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
  SCREEN_MINIMAL = 0,
  SCREEN_DETAILED,
  SCREEN_MAP,
  SCREEN_COUNT
} current_screen_t;

typedef struct {
  /* Climate */
  bool temp_valid;
  bool humidity_valid;
  float temp_c;
  float humidity_pct;

  /* Battery */
  bool battery_valid;
  int battery_mv;
  int battery_pct;

  /* System */
  uint32_t uptime_s;

  /* BLE data*/
  uint16_t hr_bpm;
  bool hr_contact_supported;
  bool hr_contact_detected;
  bool hr_valid;
  int64_t hr_updated_us;

  /* SD */
  bool sd_mounted;

  /* RTC */
  bool rtc_valid;
  uint8_t rtc_hour, rtc_min, rtc_sec, rtc_day, rtc_month, rtc_year;

  /* The ride from the UI's point of view: what to show, not how to count it. */
  bool recording;
  bool paused;
  bool summary;               /* show the summary after stopping */
  banner_t banner;            /* start-error banner, BANNER_NONE means none */
  ride_ui_state_t ui_state;   /* derived in model_snapshot */
  bool hint_visible;          /* derived in model_snapshot */

  /* GPS + ride */
  bool gps_valid;
  bool rtc_synced;
  uint8_t utc_hh, utc_mm, utc_ss;
  uint8_t utc_day, utc_month, utc_year;
  uint8_t satellites;
  float hdop;
  double lat_deg, lon_deg;
  float speed_kmh;
  float max_speed_kmh;
  double average_speed;
  double dist_m; /* distance and moving time are always valid — they are accumulated */
  uint32_t moving_s;

  current_screen_t current_screen;
} screen_data_t;

void screen_init(void);

int64_t screen_draw(const screen_data_t *d, current_screen_t screen);
