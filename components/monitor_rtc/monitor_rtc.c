#include "monitor_rtc.h"

#define PCF85063A_ADDR 0x51
#define REG_CTRL1 0x00
#define REG_CTRL2 0x01
#define REG_SECONDS 0x04      // §7.1: час 04h..0Ah
#define CTRL1_CAP_12P5 0x01   // CAP_SEL: під кварц з CL = 12.5 pF
#define CTRL1_SW_RESET 0x58   // §7.2.1.3
#define CTRL2_CLKOUT_OFF 0x07 // COF = 111, CLKOUT вимкнено
#define SEC_OS 0x80           // Oscillator Stop
#define TIMEOUT_MS 100

static inline uint8_t bcd2bin(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
static inline uint8_t bin2bcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }

static esp_err_t write_reg(monitor_rtc_t *rtc, uint8_t reg, uint8_t val) {
  uint8_t buf[2] = {reg, val};
  return i2c_master_transmit(rtc->dev, buf, 2, TIMEOUT_MS);
}

esp_err_t monitor_rtc_init(monitor_rtc_t *rtc, i2c_master_bus_handle_t bus) {
  i2c_device_config_t dev_cfg = {.dev_addr_length = I2C_ADDR_BIT_LEN_7,
                                 .scl_speed_hz = 300000,
                                 .device_address = PCF85063A_ADDR};

  esp_err_t err = i2c_master_bus_add_device(bus, &dev_cfg, &rtc->dev);

  if (err != ESP_OK)
    return err;

  err = write_reg(rtc, REG_CTRL1, 0x00); // 24h, STOP=0, 7 pF
  if (err != ESP_OK)
    return err;

  return write_reg(rtc, REG_CTRL2, CTRL2_CLKOUT_OFF);
}

esp_err_t monitor_rtc_get_time(monitor_rtc_t *rtc, struct tm *out) {
  uint8_t reg = REG_SECONDS,
          r[7]; // SEC, MIN, HOUR, DAY, WEEKDAY, MONTH, YEAR (§7.1)
  esp_err_t err =
      i2c_master_transmit_receive(rtc->dev, &reg, 1, r, 7, TIMEOUT_MS);
  if (err != ESP_OK)
    return err;

  out->tm_sec = bcd2bin(r[0] & 0x7F);
  out->tm_min = bcd2bin(r[1] & 0x7F);
  out->tm_hour = bcd2bin(r[2] & 0x3F);
  out->tm_mday = bcd2bin(r[3] & 0x3F);
  out->tm_wday = r[4] & 0x07;
  out->tm_mon = bcd2bin(r[5] & 0x1F) - 1;
  out->tm_year = bcd2bin(r[6]) + 100; // Assuming 20xx

  return ESP_OK;
}

esp_err_t monitor_rtc_set_time(monitor_rtc_t *rtc, const struct tm *in) {
  uint8_t buf[8] = {
      REG_SECONDS,
      bin2bcd(in->tm_sec), // OS=0: скидаємо прапорець
      bin2bcd(in->tm_min),
      bin2bcd(in->tm_hour),
      bin2bcd(in->tm_mday),
      in->tm_wday & 0x07,
      bin2bcd(in->tm_mon + 1),
      bin2bcd((in->tm_year + 1900) % 100),
  };
  return i2c_master_transmit(rtc->dev, buf, sizeof(buf), TIMEOUT_MS);
}
