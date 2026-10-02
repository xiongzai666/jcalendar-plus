#include <time.h>
#include <Arduino.h>

int si_calendar_status();
void si_calendar();

int si_wifi_status();
void si_wifi();

int si_weather_status();
void si_weather();

int si_screen_status();
void si_screen();
time_t si_screen_snapshot_time();

void print_status();

void si_warning(const char* str);
void si_portal_screen(const String& password);
