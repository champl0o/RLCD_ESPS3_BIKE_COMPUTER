#include "model.h"
#include "esp_err.h"
#include "fit.h"
#include "screen.h"

// screen
static screen_data_t data = {0};

// rtos-related synchronization primitives and event handling
static SemaphoreHandle_t data_mutex;

static void lock_data_mutex(void);
static void unlock_data_mutex(void);
static bool fit_time_from_data(const screen_data_t *d, uint32_t *out);

esp_err_t model_init(void) {
  data_mutex = xSemaphoreCreateMutex();
  if (!data_mutex) {
    return ESP_FAIL;
  }
  data.current_screen = SCREEN_MINIMAL;

  return ESP_OK;
}

void model_snapshot(screen_data_t *out) {
  lock_data_mutex();
  *out = data;
  unlock_data_mutex();
}

void model_set_climate(float temp_c, float humidity_pct, bool valid) {
  lock_data_mutex();
  data.temp_c = temp_c;
  data.humidity_pct = humidity_pct;
  data.temp_valid = valid;
  data.humidity_valid = valid;
  unlock_data_mutex();
}

void model_set_battery(int mv, int pct, bool valid) {
  lock_data_mutex();
  data.battery_mv = mv;
  data.battery_pct = pct;
  data.battery_valid = valid;
  unlock_data_mutex();
}

void model_set_rtc(const struct tm *now, bool valid) {
  lock_data_mutex();
  if (now) {
    data.rtc_hour = now->tm_hour;
    data.rtc_min = now->tm_min;
    data.rtc_sec = now->tm_sec;
    data.rtc_day = now->tm_mday;
    data.rtc_month = now->tm_mon + 1;
    data.rtc_year = now->tm_year % 100;
  }
  data.rtc_valid = valid;
  unlock_data_mutex();
}

void model_set_sd_mounted(bool mounted) {
  lock_data_mutex();
  data.sd_mounted = mounted;
  unlock_data_mutex();
}

bool model_sd_mounted(void) {
  bool mounted;

  lock_data_mutex();
  mounted = data.sd_mounted;
  unlock_data_mutex();

  return mounted;
}

void model_apply_gps(const gps_data_t *gps, const trip_t *trip_data) {
  lock_data_mutex();
  data.gps_valid = gps->valid;
  data.satellites = gps->satellites;
  data.hdop = gps->hdop;

  // Apply trip data
  data.dist_m = trip_data->dist_km * 1000.0; // Convert km to meters
  data.moving_s = (uint32_t)trip_data->moving_time;
  data.average_speed = trip_data->average_speed;
  data.max_speed_kmh = trip_data->max_speed_kmh;

  if (!gps->valid) {
    unlock_data_mutex();
    return;
  }
  data.lat_deg = gps->lat;
  data.lon_deg = gps->lon;
  data.speed_kmh = gps->speed_kmh;

  data.utc_day = gps->day;
  data.utc_month = gps->month;
  data.utc_year = gps->year;
  data.utc_hh = gps->hh;
  data.utc_mm = gps->mm;
  data.utc_ss = gps->ss;

  unlock_data_mutex();
}

void model_next_screen(void) {
  lock_data_mutex();
  data.current_screen = (data.current_screen + 1) % SCREEN_COUNT;
  unlock_data_mutex();
}

void model_reset_screen(void) {
  lock_data_mutex();
  data.current_screen = SCREEN_MINIMAL;
  unlock_data_mutex();
}

bool model_fit_time(uint32_t *out) {
  lock_data_mutex();
  bool result = fit_time_from_data(&data, out);
  unlock_data_mutex();

  return result;
}

static void lock_data_mutex(void) { xSemaphoreTake(data_mutex, portMAX_DELAY); }

static void unlock_data_mutex(void) { xSemaphoreGive(data_mutex); }

static bool fit_time_from_data(const screen_data_t *d, uint32_t *out) {
  if (d->gps_valid) {
    *out = fit_timestamp_from_utc(2000 + d->utc_year, d->utc_month, d->utc_day,
                                  d->utc_hh, d->utc_mm, d->utc_ss);
    return true;
  }
  if (d->rtc_valid) {
    *out = fit_timestamp_from_utc(2000 + d->rtc_year, d->rtc_month, d->rtc_day,
                                  d->rtc_hour, d->rtc_min, d->rtc_sec);
    return true;
  }
  return false;
}

void model_set_rtc_synced(bool synced) {
  lock_data_mutex();
  data.rtc_synced = synced;
  unlock_data_mutex();
}

bool model_gps_time_for_rtc(struct tm *out) {
  bool need = false;

  lock_data_mutex();
  if (!data.rtc_synced && data.gps_valid) {
    out->tm_year = data.utc_year + 100; // years since 1900
    out->tm_mon = data.utc_month - 1;   // months since January
    out->tm_mday = data.utc_day;
    out->tm_hour = data.utc_hh;
    out->tm_min = data.utc_mm;
    out->tm_sec = data.utc_ss;

    need = true;
  }
  unlock_data_mutex();

  return need;
}

current_screen_t model_current_screen(void) {
  current_screen_t screen;

  lock_data_mutex();
  screen = data.current_screen;
  unlock_data_mutex();

  return screen;
}
