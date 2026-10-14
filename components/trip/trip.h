#pragma once

#include "nmea.h"
#include <math.h>
#include <stdbool.h>

#define MIN_SPEED 2           // km/h
#define EARTH_RADIUS_KM .6371 // Earth radius in kilometers
#define DEG_TO_RAD (3.141592653589793 / 180.0)

typedef struct
{
    bool has_last_coordinates;
    float max_speed_kmh;
    double last_lon, last_lat;
    double total_time;
    double dt;
    // double stopped_time; - placeholder for future use
    double dist_km;
    double moving_time;
    double average_speed;
} trip_t;

const trip_t *trip_get(void);
void trip_reset(void);
void trip_update(double lat, double lon, double speed_kmh, double time, bool valid);
