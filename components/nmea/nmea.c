#include "nmea.h"

#include <stdlib.h>
#include <string.h>

#define NMEA_MAX_LEN 96 /* стандарт: ≤ 82 символи */
#define MAX_FIELDS 15

static int hex_digit(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}

bool nmea_checksum_ok(const char *line)
{
    if (!line || line[0] != '$')
        return false;

    uint8_t x = 0;
    const char *p = line + 1;
    for (; *p && *p != '*'; p++)
        x ^= (uint8_t)*p;

    if (*p != '*')
        return false;
    int hi = hex_digit(p[1]);
    int lo = hi < 0 ? -1 : hex_digit(p[2]);
    return lo >= 0 && x == (uint8_t)(hi * 16 + lo);
}

/* strsep() лишає NULL для полів, яких у реченні немає. */
static const char *field(char **parts, int i)
{
    return parts[i] ? parts[i] : "";
}

/* Два символи-цифри → число, інакше -1. */
static int two_digits(const char *s)
{
    if (s[0] < '0' || s[0] > '9' || s[1] < '0' || s[1] > '9')
        return -1;
    return (s[0] - '0') * 10 + (s[1] - '0');
}

/* ddmm.mmmm / dddmm.mmmm → десяткові градуси. */
static double to_deg(const char *v, const char *hemi)
{
    double raw = atof(v);
    int deg = (int)(raw / 100);
    double d = deg + (raw - deg * 100) / 60.0;
    return (hemi[0] == 'S' || hemi[0] == 'W') ? -d : d;
}

/* Перевіряє суму, копіює рядок у buf (strsep його псує), відрізає "*HH",
 * розбиває по комах. Вхідний рядок не змінюється. */
static bool split(const char *line, const char *type, char *buf, char **parts)
{
    if (!line || strlen(line) >= NMEA_MAX_LEN || !nmea_checksum_ok(line))
        return false;

    strcpy(buf, line);
    *strchr(buf, '*') = '\0';

    char *p = buf;
    for (int i = 0; i < MAX_FIELDS && p; i++)
        parts[i] = strsep(&p, ",");

    return strcmp(parts[0], type) == 0;
}

bool nmea_parse_rmc(const char *line, nmea_rmc_t *out)
{
    char buf[NMEA_MAX_LEN];
    char *parts[MAX_FIELDS] = {0};
    if (!split(line, "$GPRMC", buf, parts) || !parts[9])
        return false;

    nmea_rmc_t r = {0};
    r.fix = field(parts, 2)[0] == 'A';

    const char *t = field(parts, 1);
    const char *d = field(parts, 9);
    if (strlen(t) >= 6 && strlen(d) == 6)
    {
        int hh = two_digits(t), mm = two_digits(t + 2), ss = two_digits(t + 4);
        int dd = two_digits(d), mo = two_digits(d + 2), yy = two_digits(d + 4);
        if (hh >= 0 && mm >= 0 && ss >= 0 && dd >= 0 && mo >= 0 && yy >= 0)
        {
            r.hh = hh;
            r.mm = mm;
            r.ss = ss;
            r.day = dd;
            r.month = mo;
            r.year = 2000 + yy;
            r.time_valid = true;
        }
    }

    if (r.fix)
    {
        if (!field(parts, 3)[0] || !field(parts, 5)[0])
            return false;
        r.lat_deg = to_deg(field(parts, 3), field(parts, 4));
        r.lon_deg = to_deg(field(parts, 5), field(parts, 6));
        r.speed_kmh = atof(field(parts, 7)) * KMH_MULTIPLIER / 1.944f; /* вузли → км/год */
    }

    *out = r;
    return true;
}

bool nmea_parse_gga(const char *line, nmea_gga_t *out)
{
    char buf[NMEA_MAX_LEN];
    char *parts[MAX_FIELDS] = {0};
    if (!split(line, "$GPGGA", buf, parts) || !parts[8])
        return false;

    nmea_gga_t g = {0};
    g.fix_quality = atoi(field(parts, 6));
    g.satellites = atoi(field(parts, 7));
    g.hdop = atof(field(parts, 8));
    if (g.hdop > HDOP_THRESHOLD)
        g.fix = false;
    else
        g.fix = true;

    *out = g;
    return true;
}
