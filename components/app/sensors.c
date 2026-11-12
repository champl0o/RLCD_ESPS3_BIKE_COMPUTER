#include "sensors.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "model.h"
#include <battery.h>
#include <monitor_rtc.h>
#include <shtc3.h>

#define I2C_SDA_GPIO 13
#define I2C_SCL_GPIO 14

static monitor_rtc_t *rtc = NULL;
static shtc3_t *shtc3 = NULL;
static battery_t *battery = NULL;

static const char *TAG = "sensors";

static i2c_master_bus_handle_t i2c_bus_init(void) {
  const i2c_master_bus_config_t cfg = {
      .i2c_port = -1, /* the driver picks a free port itself */
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

static void shtc3_init_sensor(i2c_master_bus_handle_t bus) {
  static shtc3_t shtc3_dev;

  shtc3 = shtc3_init(&shtc3_dev, bus) == ESP_OK ? &shtc3_dev : NULL;
  if (!shtc3)
    ESP_LOGW(TAG, "SHTC3 not found, temp/humidity will show dashes");
}

static void rtc_init_sensor(i2c_master_bus_handle_t bus) {
  static monitor_rtc_t rtc_dev;

  rtc = monitor_rtc_init(&rtc_dev, bus) == ESP_OK ? &rtc_dev : NULL;
  if (!rtc)
    ESP_LOGW(TAG, "RTC not found, time will not be set");
}

static void battery_init_sensor(void) {
  static battery_t battery_dev;

  battery = battery_init(&battery_dev) == ESP_OK ? &battery_dev : NULL;
  if (!battery)
    ESP_LOGW(TAG, "Battery sensor not found");
}

void sensors_init(void) {
  i2c_master_bus_handle_t bus = i2c_bus_init();
  shtc3_init_sensor(bus);
  rtc_init_sensor(bus);
  battery_init_sensor();
}

void sensors_update_climate(void) {
  if (!shtc3)
    return;

  float temp_c, humidity_pct;
  esp_err_t err = shtc3_read(shtc3, &temp_c, &humidity_pct);

  model_set_climate(temp_c, humidity_pct, err == ESP_OK);

  if (err != ESP_OK)
    ESP_LOGW(TAG, "SHTC3 read failed: %s", esp_err_to_name(err));
}

void sensors_update_rtc(void) {
  if (!rtc)
    return;

  sensors_set_date_time_from_gps(); // Set RTC from GPS if valid
  struct tm now;
  esp_err_t err = monitor_rtc_get_time(rtc, &now);

  model_set_rtc(&now, err == ESP_OK);

  if (err != ESP_OK)
    ESP_LOGW(TAG, "RTC read failed: %s", esp_err_to_name(err));
}

void sensors_update_battery(void) {
  if (!battery)
    return;

  int mv;

  bool valid = battery_read_mv(battery, &mv) == ESP_OK;
  int pct = valid ? battery_percent_from_mv(mv) : -1;
  model_set_battery(mv, pct, valid);
}

void sensors_set_date_time_from_gps(void) {
  if (!rtc)
    return;

  struct tm now = {0};
  if (!model_gps_time_for_rtc(&now))
    return;

  esp_err_t err = monitor_rtc_set_time(rtc, &now);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Failed to set RTC time: %s", esp_err_to_name(err));
  }

  model_set_rtc_synced(err == ESP_OK);
}
