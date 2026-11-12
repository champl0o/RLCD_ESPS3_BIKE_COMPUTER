#pragma once
#include "esp_err.h"
#include "gps.h"
#include "screen.h"
#include "trip.h"
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

esp_err_t model_init(void);
void model_snapshot(screen_data_t *out);
void model_set_climate(float temp_c, float humidity_pct, bool valid);
void model_set_battery(int mv, int pct, bool valid);
void model_set_rtc(const struct tm *now, bool valid);
void model_set_sd_mounted(bool mounted);
bool model_hr_bpm(uint8_t *out);
void model_set_hr(uint16_t bpm, bool contact_supported, bool contact_detected);
void model_clear_hr(void);
bool model_sd_mounted(void);
void model_set_recording(bool recording); /* false also clears the pause */
void model_set_paused(bool paused);
void model_set_summary(bool summary);
void model_set_banner(banner_t banner); /* fades by itself after UI_BANNER_US */
void model_note_press(int64_t now_us);
bool model_recording(void);
bool model_paused(void);
bool model_summary(void);
void model_apply_gps(const gps_data_t *gps, const trip_t *trip_data);
void model_next_screen(void);
void model_reset_screen(void);
void model_set_rtc_synced(bool);
bool model_fit_time(uint32_t *out);
bool model_gps_time_for_rtc(struct tm *out);
current_screen_t model_current_screen(void);
