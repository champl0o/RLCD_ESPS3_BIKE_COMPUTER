#include "trip.h"

static trip_t trip = {0};

static double haversine_distance(double lat1, double lon1, double lat2, double lon2);

static bool is_moving(double speed_kmh)
{
    return speed_kmh > MIN_SPEED; // km/h
}

static void set_initial_position(double lat, double lon, double time)
{
    trip.last_lat = lat;
    trip.last_lon = lon;
    trip.dt = time; // Use the provided time in seconds
    trip.has_last_coordinates = true;
}

static void update_last_position(double lat, double lon)
{
    trip.last_lat = lat;
    trip.last_lon = lon;
}
static void update_trip_distance(double lat, double lon)
{
    trip.dist_km += haversine_distance(trip.last_lat, trip.last_lon, lat, lon);
    update_last_position(lat, lon);
}
static void update_trip_time(bool moving, double time)
{
    double now = time;

    if (moving)
        trip.moving_time += now - trip.dt;
    trip.total_time += now - trip.dt;
    trip.dt = now;
}
static void update_trip_average_speed(void)
{
    if (trip.moving_time > 0)
        trip.average_speed = trip.dist_km / (trip.moving_time / 3600.0); // km/h
}
static void update_trip_max_speed(double speed_kmh)
{
    if (speed_kmh > trip.max_speed_kmh)
        trip.max_speed_kmh = speed_kmh;
}

const trip_t *trip_get(void)
{
    return &trip;
}

void trip_reset(void)
{
    trip = (trip_t){0};
}

void trip_update(double lat, double lon, double speed_kmh, double time, bool valid)
{
    if (!valid)
        return;
    else if (!trip.has_last_coordinates)
    {
        set_initial_position(lat, lon, time);
        return;
    }

    if (is_moving(speed_kmh))
    {

        update_trip_distance(lat, lon);
        update_trip_time(true, time);
        update_trip_average_speed();
        update_trip_max_speed(speed_kmh);
    }
    else
    {
        update_trip_time(false, time);
        update_trip_average_speed();
    }
    // return trip;
}

static double haversine_distance(double lat1, double lon1, double lat2, double lon2)
{
    double dlat = (lat2 - lat1) * DEG_TO_RAD;
    double dlon = (lon2 - lon1) * DEG_TO_RAD;
    double a = (sin(dlat / 2) * sin(dlat / 2)) +
               (cos(lat1 * DEG_TO_RAD) * cos(lat2 * DEG_TO_RAD)) *
                   (sin(dlon / 2) * sin(dlon / 2));
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));
    return EARTH_RADIUS_KM * c;
}
