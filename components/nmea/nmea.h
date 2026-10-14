#pragma once

#include <stdbool.h>
#include <stdint.h>

#define HDOP_THRESHOLD 4
#define KMH_MULTIPLIER 3.6

typedef struct
{
    bool fix;
    bool time_valid;
    uint8_t hh, mm, ss;
    uint8_t day, month;
    uint16_t year;
    double lat_deg, lon_deg;
    float speed_kmh;
} nmea_rmc_t;

typedef struct
{
    bool fix;
    uint8_t fix_quality;
    uint8_t satellites;
    float hdop;
} nmea_gga_t;

/* "$....*HH": XOR байтів між '$' і '*' має збігатися з двома hex-цифрами. */
bool nmea_checksum_ok(const char *line);

bool nmea_parse_rmc(const char *line, nmea_rmc_t *out);
bool nmea_parse_gga(const char *line, nmea_gga_t *out);
