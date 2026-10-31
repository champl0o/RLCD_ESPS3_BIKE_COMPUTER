#include "gps.h"

static const char *TAG = "gps";

static uint8_t gps_data[UART_BUFFER_SIZE];
static int gps_data_len;

static gps_cb_t gps_callback = NULL;
static gps_data_t current_gps_data = {0};

static void nullify_current_gps_data(void);

static esp_err_t raw_gps_data(void) {
  gps_data_len = uart_read_bytes(UART_NUM, gps_data, UART_BUFFER_SIZE - 1,
                                 pdMS_TO_TICKS(50));
  if (gps_data_len <= 0) {
    gps_data_len = 0;
    return ESP_FAIL;
  }

  gps_data[gps_data_len] = '\0';
  return ESP_OK;
}

static void apply_gga(const nmea_gga_t *g) {
  if (g->fix_quality == 0)
    return;
  current_gps_data.satellites = g->satellites;
  current_gps_data.hdop = g->hdop;
}

static void apply_rmc(const nmea_rmc_t *r, gps_cb_t cb) {
  if (!r->fix) {
    nullify_current_gps_data();
    if (cb)
      cb(&current_gps_data);
    return;
  }
  if (!r->time_valid)
    return;

  current_gps_data.lat = r->lat_deg;
  current_gps_data.lon = r->lon_deg;
  current_gps_data.speed_kmh = r->speed_kmh;
  current_gps_data.hh = r->hh;
  current_gps_data.mm = r->mm;
  current_gps_data.ss = r->ss;
  current_gps_data.day = r->day;
  current_gps_data.month = r->month;
  current_gps_data.year = r->year % 100;
  current_gps_data.valid = true;

  if (cb)
    cb(&current_gps_data);
}

static void handle_line(const char *line, gps_cb_t cb) {
  nmea_rmc_t rmc;
  nmea_gga_t gga;
  if (nmea_parse_rmc(line, &rmc))
    apply_rmc(&rmc, cb);
  else if (nmea_parse_gga(line, &gga))
    apply_gga(&gga);
}

static void parse_gps_data(gps_cb_t cb) {
  char *buf = (char *)gps_data;

  char *line;
  while ((line = strsep(&buf, "\n"))) {
    if (line[0] == '$')
      handle_line(line, cb);
  }
}

esp_err_t gps_init(void) {
  uart_config_t cfg = {
      .baud_rate = 9600,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .source_clk = UART_SCLK_DEFAULT,
  };
  ESP_ERROR_CHECK(uart_param_config(UART_NUM, &cfg));
  ESP_ERROR_CHECK(uart_set_pin(UART_NUM, TX_PIN, RX_PIN, UART_PIN_NO_CHANGE,
                               UART_PIN_NO_CHANGE));
  ESP_ERROR_CHECK(
      uart_driver_install(UART_NUM, UART_BUFFER_SIZE, 0, 0, NULL, 0));
  return ESP_OK;
}

int gps_poll(gps_cb_t cb) {
  if (raw_gps_data() != ESP_OK)
    return 0;

  // ESP_LOGI(TAG, "GPS data: %.*s", gps_data_len, gps_data);

  parse_gps_data(cb);
  return gps_data_len;
}

static void gps_task(void *arg) {
  gps_cb_t cb = (gps_cb_t)arg;

  while (1) {
    gps_poll(cb);
  }
}

esp_err_t gps_start(gps_cb_t cb) {
  gps_init();

  if (cb == NULL)
    return ESP_ERR_INVALID_ARG;

  gps_callback = cb;

  xTaskCreate(gps_task, "gps_task", 4096, (void *)cb, 5, NULL);

  return ESP_OK;
}

gps_data_t gps_get(void) { return current_gps_data; }

static void nullify_current_gps_data(void) {
  current_gps_data.valid = false;
  current_gps_data.lat = 0;
  current_gps_data.lon = 0;
  current_gps_data.satellites = 0;
  current_gps_data.hdop = 0;
  current_gps_data.speed_kmh = 0;
  current_gps_data.hh = 0;
  current_gps_data.mm = 0;
  current_gps_data.ss = 0;
  current_gps_data.day = 0;
  current_gps_data.month = 0;
  current_gps_data.year = 0;
}
