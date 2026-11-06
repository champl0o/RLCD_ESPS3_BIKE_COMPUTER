#pragma once

void sensors_init(void);

void sensors_update_climate(void);
void sensors_update_rtc(void);
void sensors_update_battery(void);
void sensors_set_date_time_from_gps(void);
