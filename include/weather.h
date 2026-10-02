#ifndef __WEATHER_H__
#define __WEATHER_H__

#include <API.hpp>

int8_t weather_type();
int8_t weather_status();
Weather* weather_data_now();
DailyForecast* weather_data_daily();
void weather_exec(int status = 0);
void weather_stop();
bool weather_restore_cached();
bool weather_is_fallback();
uint32_t weather_now_read_at();
uint32_t weather_daily_read_at();
bool weather_now_failed();
bool weather_daily_failed();
bool weather_now_available();
bool weather_daily_available(int index);
String weather_dismissal_hint(bool night, time_t snapshot);

#endif
