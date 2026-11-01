#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#include "button_types.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "battery.h"
#include "ble_sensors.h"
#include "button.h"
#include "event_orchestrator.h"
#include "freertos/semphr.h"
#include "gps.h"
#include "monitor_rtc.h"
#include "portmacro.h"
#include "screen.h"
#include "sdcard.h"
#include "shtc3.h"
#include "trip.h"

#define I2C_SDA_GPIO 13
#define I2C_SCL_GPIO 14

#define NEXT_CLIMATE_DELAY_US 10000000 // 10 seconds
#define NEXT_RTC_DELAY_US 1000000      // 1 second
#define NEXT_BATTERY_DELAY_US 5000000  // 5 seconds

static const char *TAG = "main";

static esp_timer_handle_t climate_timer;
static esp_timer_handle_t rtc_timer;
static esp_timer_handle_t battery_timer;

// screen
static screen_data_t data = {0};
static uint32_t frame = 0;

// rtos-related synchronization primitives and event handling
static SemaphoreHandle_t data_mutex;
static TaskHandle_t ui_task_handle;

static esp_event_loop_handle_t loop_handle;

static monitor_rtc_t *rtc = NULL;
static shtc3_t *shtc3 = NULL;
static battery_t *battery = NULL;

static void init_screen(void) {
  screen_init();
  data.current_screen = SCREEN_MINIMAL;

  ui_task_handle = xTaskGetCurrentTaskHandle();
}

/* Одна шина на всю плату: SHTC3, PCF85063, ES8311, ES7210. */
static i2c_master_bus_handle_t i2c_bus_init(void) {
  const i2c_master_bus_config_t cfg = {
      .i2c_port = -1, /* драйвер сам вибере вільний порт */
      .sda_io_num = I2C_SDA_GPIO,
      .scl_io_num = I2C_SCL_GPIO,
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .glitch_ignore_cnt = 7,
      .flags.enable_internal_pullup = true,
  };

  i2c_master_bus_handle_t bus;
  ESP_ERROR_CHECK(i2c_new_master_bus(&cfg, &bus));

  return bus;
}

static void update_climate(void *arg) {
  if (!shtc3)
    return;

  float temp_c, humidity_pct;
  esp_err_t err = shtc3_read(shtc3, &temp_c, &humidity_pct);

  xSemaphoreTake(data_mutex, portMAX_DELAY);
  data.temp_valid = data.humidity_valid = (err == ESP_OK);
  if (data.temp_valid) {
    data.temp_c = temp_c;
    data.humidity_pct = humidity_pct;
  }
  xSemaphoreGive(data_mutex);

  xTaskNotifyGive(ui_task_handle);

  if (err != ESP_OK)
    ESP_LOGW(TAG, "SHTC3 read failed: %s", esp_err_to_name(err));
}

static void update_battery(void *arg) {
  if (!battery)
    return;

  int mv;

  bool valid = battery_read_mv(battery, &mv) == ESP_OK;
  int pct = valid ? battery_percent_from_mv(mv) : -1;
  xSemaphoreTake(data_mutex, portMAX_DELAY);
  data.battery_valid = valid;

  if (data.battery_valid) {
    data.battery_mv = mv;
    data.battery_pct = pct;
  }
  xSemaphoreGive(data_mutex);

  xTaskNotifyGive(ui_task_handle);
}

static void set_date_time_from_gps() {
  if (!rtc)
    return;

  struct tm now = {0};

  xSemaphoreTake(data_mutex, portMAX_DELAY);
  bool rtc_was_synced = data.rtc_synced;
  bool gps_was_valid = data.gps_valid;
  xSemaphoreGive(data_mutex);

  if (!rtc_was_synced && gps_was_valid) {
    xSemaphoreTake(data_mutex, portMAX_DELAY);
    now.tm_year = data.utc_year + 100; // years since 1900
    now.tm_mon = data.utc_month - 1;   // months since January
    now.tm_mday = data.utc_day;
    now.tm_hour = data.utc_hh;
    now.tm_min = data.utc_mm;
    now.tm_sec = data.utc_ss;
    xSemaphoreGive(data_mutex);

    esp_err_t err = monitor_rtc_set_time(rtc, &now);
    if (err != ESP_OK)
      ESP_LOGW(TAG, "Failed to set RTC time: %s", esp_err_to_name(err));

    xSemaphoreTake(data_mutex, portMAX_DELAY);
    data.rtc_synced = (err == ESP_OK);
    xSemaphoreGive(data_mutex);
  }
}

static void update_rtc(void *arg) {
  if (!rtc)
    return;

  set_date_time_from_gps(); // Set RTC from GPS if valid
  struct tm now;
  esp_err_t err = monitor_rtc_get_time(rtc, &now);

  xSemaphoreTake(data_mutex, portMAX_DELAY);
  data.rtc_valid = (err == ESP_OK);
  if (data.rtc_valid) {
    data.rtc_hour = now.tm_hour;
    data.rtc_min = now.tm_min;
    data.rtc_sec = now.tm_sec;
    data.rtc_day = now.tm_mday;
    data.rtc_month = now.tm_mon + 1;
    data.rtc_year = now.tm_year % 100;
  }
  xSemaphoreGive(data_mutex);

  xTaskNotifyGive(ui_task_handle);
  if (err != ESP_OK)
    ESP_LOGW(TAG, "RTC read failed: %s", esp_err_to_name(err));
}

static void update_gps(screen_data_t *d, const gps_data_t *gps) {

  d->gps_valid = gps->valid;
  d->satellites = gps->satellites;
  d->hdop = gps->hdop;

  if (!gps->valid)
    return;

  d->lat_deg = gps->lat;
  d->lon_deg = gps->lon;
  d->speed_kmh = gps->speed_kmh;

  d->utc_day = gps->day;
  d->utc_month = gps->month;
  d->utc_year = gps->year;
  d->utc_hh = gps->hh;
  d->utc_mm = gps->mm;
  d->utc_ss = gps->ss;
}

static void draw_current_screen(void) {
  screen_data_t snapshot;
  xSemaphoreTake(data_mutex, portMAX_DELAY);
  snapshot = data;
  xSemaphoreGive(data_mutex);

  snapshot.uptime_s = (uint32_t)(esp_timer_get_time() / 1000000LL);

  int64_t dt =
      screen_draw(&snapshot, snapshot.current_screen); // draw current later

  if ((frame % 10) == 0) {
    ESP_LOGI(
        TAG,
        "frame %lu: SendBuffer took %lld us (max %.1f fps), %.2f C, %.1f %%RH",
        (unsigned long)frame, (long long)dt,
        dt > 0 ? 1000000.0 / (double)dt : 0.0, snapshot.temp_c,
        snapshot.humidity_pct);
  }
  frame++;
}

static void on_gps_data(const gps_data_t *data) {
  esp_err_t err =
      esp_event_post_to(loop_handle, APP_EVENT_ORCHESTRATOR, EVT_GPS_UPDATED,
                        (void *)data, sizeof(gps_data_t), 0);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Failed to post GPS event: %s", esp_err_to_name(err));
  }
}

static void on_gps_event(void *handler_arg, esp_event_base_t base, int32_t id,
                         void *event_data) {
  if (id == EVT_GPS_UPDATED) {
    gps_data_t *gps_data = (gps_data_t *)event_data;

    double now =
        esp_timer_get_time() / 1000000.0; // Convert microseconds to seconds
    trip_update(gps_data->lat, gps_data->lon, gps_data->speed_kmh, now,
                gps_data->valid);
    const trip_t *trip_data = trip_get();

    xSemaphoreTake(data_mutex, portMAX_DELAY);
    update_gps(&data, gps_data);

    data.dist_m = trip_data->dist_km * 1000.0; // Convert km to meters
    data.moving_s = (uint32_t)trip_data->moving_time;
    data.average_speed = trip_data->average_speed;
    data.max_speed_kmh = trip_data->max_speed_kmh;
    xSemaphoreGive(data_mutex);

    // d->stopped_time = trip_data.stopped_time;

    xTaskNotifyGive(ui_task_handle); // Notify the UI task to redraw the screen

    ESP_LOGI(TAG, "GPS updated: lat=%.6f, lon=%.6f, speed=%.2f km/h",
             gps_data->lat, gps_data->lon, gps_data->speed_kmh);
  }
}

static void initialize_timers(void) {
  const esp_timer_create_args_t climate_timer_args = {
      .callback = update_climate,
      .name = "climate_timer",
  };

  const esp_timer_create_args_t rtc_timer_args = {
      .callback = update_rtc,
      .name = "rtc_timer",
  };

  const esp_timer_create_args_t battery_timer_args = {
      .callback = update_battery,
      .name = "battery_timer",
  };

  update_climate(NULL);
  update_rtc(NULL);
  update_battery(NULL);

  ESP_ERROR_CHECK(esp_timer_create(&climate_timer_args, &climate_timer));
  ESP_ERROR_CHECK(esp_timer_create(&rtc_timer_args, &rtc_timer));
  ESP_ERROR_CHECK(esp_timer_create(&battery_timer_args, &battery_timer));

  ESP_ERROR_CHECK(
      esp_timer_start_periodic(climate_timer, NEXT_CLIMATE_DELAY_US));
  ESP_ERROR_CHECK(esp_timer_start_periodic(rtc_timer, NEXT_RTC_DELAY_US));
  ESP_ERROR_CHECK(
      esp_timer_start_periodic(battery_timer, NEXT_BATTERY_DELAY_US));
}

static void on_sd_card_mounted(void *arg) {
  esp_err_t err = esp_event_post_to(loop_handle, APP_EVENT_ORCHESTRATOR,
                                    EVT_SD_MOUNTED, NULL, 0, 0);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Failed to post SD card event: %s", esp_err_to_name(err));
  }
}

static void on_sd_card_event(void *handler_arg, esp_event_base_t base,
                             int32_t id, void *event_data) {
  if (id == EVT_SD_MOUNTED) {
    bool sd_mounted = sd_card_is_mounted();

    xSemaphoreTake(data_mutex, portMAX_DELAY);
    data.sd_mounted = sd_mounted;
    xSemaphoreGive(data_mutex);
  }
}

static void on_button_callback(btn_event_t event) {
  esp_err_t err = esp_event_post_to(loop_handle, APP_EVENT_ORCHESTRATOR,
                                    EVT_BUTTON, &event, sizeof(btn_event_t), 0);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Failed to post button event: %s", esp_err_to_name(err));
  }
}

static void on_button_event(void *handler_arg, esp_event_base_t base,
                            int32_t id, void *event_data) {
  if (id == EVT_BUTTON) {
    btn_event_t *event = (btn_event_t *)event_data;

    current_screen_t local_current_screen;
    xSemaphoreTake(data_mutex, portMAX_DELAY);
    local_current_screen = data.current_screen;
    xSemaphoreGive(data_mutex);

    if (event->btn_id == BTN_KEY && event->gesture == PRESS_SHORT) {
      xSemaphoreTake(data_mutex, portMAX_DELAY);
      data.current_screen = (data.current_screen + 1) % SCREEN_COUNT;
      xSemaphoreGive(data_mutex);
    }

    if (event->btn_id == BTN_BOOT) {
      xSemaphoreTake(data_mutex, portMAX_DELAY);
      data.current_screen = SCREEN_MINIMAL;
      xSemaphoreGive(data_mutex);
    }

    if (local_current_screen != data.current_screen) {
      xTaskNotifyGive(
          ui_task_handle); // Notify the UI task to redraw the screen

      xSemaphoreTake(data_mutex, portMAX_DELAY);
      ESP_LOGI(TAG, "Screen changed to %d", data.current_screen);
      xSemaphoreGive(data_mutex);
    }
  }
}

void initialize_event_orchestrator(void) {
  esp_err_t err = event_orchestrator_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize event orchestrator: %s",
             esp_err_to_name(err));
    return;
  }

  loop_handle = event_orchestrator_get_loop();

  event_orchestrator_add_event_handler(APP_EVENT_ORCHESTRATOR, EVT_GPS_UPDATED,
                                       on_gps_event, NULL);
  event_orchestrator_add_event_handler(APP_EVENT_ORCHESTRATOR, EVT_SD_MOUNTED,
                                       on_sd_card_event, NULL);
  event_orchestrator_add_event_handler(APP_EVENT_ORCHESTRATOR, EVT_BUTTON,
                                       on_button_event, NULL);
}

static esp_err_t create_data_mutex(void) {
  data_mutex = xSemaphoreCreateMutex();
  if (!data_mutex) {
    ESP_LOGE(TAG, "Failed to create data mutex");
    return ESP_FAIL;
  }
  return ESP_OK;
}

void app_main(void) {
  init_screen();

  initialize_event_orchestrator();

  esp_err_t err = create_data_mutex();
  if (err != ESP_OK)
    return;

  i2c_master_bus_handle_t bus = i2c_bus_init();
  static shtc3_t shtc3_dev;
  shtc3 = shtc3_init(&shtc3_dev, bus) == ESP_OK ? &shtc3_dev : NULL;
  if (!shtc3)
    ESP_LOGW(TAG, "SHTC3 not found, temp/humidity will show dashes");
  static monitor_rtc_t rtc_dev;
  rtc = monitor_rtc_init(&rtc_dev, bus) == ESP_OK ? &rtc_dev : NULL;
  if (!rtc)
    ESP_LOGW(TAG, "RTC not found, time will not be set");

  static battery_t battery_dev;
  battery = battery_init(&battery_dev) == ESP_OK ? &battery_dev : NULL;
  if (!battery)
    ESP_LOGW(TAG, "Battery ADC init failed, battery will show dashes");
  initialize_timers();

  gps_start(on_gps_data);

  ble_sensors_init();

  sd_card_mount(on_sd_card_mounted);

  button_init(on_button_callback);

  while (1) {
    draw_current_screen();

    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
  }
}
