#include "battery.h"
#include <esp_check.h>

static const char *TAG = "battery";

#define BATTERY_CHANNEL ADC_CHANNEL_3 /* GPIO4 */
#define DIVIDER_RATIO 3               /* the board has a 1:3 divider, see HARDWARE.md */
#define SAMPLES 16                    /* averaging damps ADC noise */

esp_err_t battery_init(battery_t *battery)
{
    const adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&unit_cfg, &battery->adc), TAG, "new unit");

    /* DB_12: without it the ceiling is ~0.95 V, while the pin can reach 1.4 V. */
    const adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(battery->adc, BATTERY_CHANNEL, &chan_cfg),
                        TAG, "config channel");

    const adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT_1,
        .chan = BATTERY_CHANNEL,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    return adc_cali_create_scheme_curve_fitting(&cali_cfg, &battery->cali);
}

esp_err_t battery_read_mv(battery_t *battery, int *mv)
{
    int sum = 0;
    for (int i = 0; i < SAMPLES; i++)
    {
        int raw;
        ESP_RETURN_ON_ERROR(adc_oneshot_read(battery->adc, BATTERY_CHANNEL, &raw), TAG, "read");
        sum += raw;
    }

    int pin_mv; /* the calibration returns millivolts, as an integer */
    ESP_RETURN_ON_ERROR(adc_cali_raw_to_voltage(battery->cali, sum / SAMPLES, &pin_mv),
                        TAG, "cali");

    *mv = pin_mv * DIVIDER_RATIO;
    return ESP_OK;
}

/* Li-ion voltage drops unevenly: fast near 4.2 V, then a long plateau
 * ~3.7–3.9 V, where most of the charge sits, and a cliff at the end.
 * A linear formula lies by 20–30 % on the plateau. Hence a table and linear
 * interpolation between neighbouring points.
 * Values are approximate, for a resting cell. Under load the voltage
 * sags by 0.1–0.2 V; while charging it reads too high. */
static const struct
{
    int mv;
    uint8_t pct;
} curve[] = {
    {4200, 100},
    {4100, 90},
    {3980, 80},
    {3920, 70},
    {3870, 60},
    {3820, 50},
    {3790, 40},
    {3770, 30},
    {3740, 20},
    {3680, 10},
    {3450, 5},
    {3000, 0},
};

uint8_t battery_percent_from_mv(int mv)
{
    const int n = sizeof(curve) / sizeof(curve[0]);

    if (mv >= curve[0].mv)
        return 100;
    if (mv <= curve[n - 1].mv)
        return 0;

    for (int i = 1; i < n; i++)
    {
        if (mv >= curve[i].mv)
        {
            /* mv lies between curve[i] (lower point) and curve[i-1] (upper). */
            int span_mv = curve[i - 1].mv - curve[i].mv;
            int span_pct = curve[i - 1].pct - curve[i].pct;
            return curve[i].pct + (mv - curve[i].mv) * span_pct / span_mv;
        }
    }
    return 0; /* unreachable: bounds are checked above */
}
