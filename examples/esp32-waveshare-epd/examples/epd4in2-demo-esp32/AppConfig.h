#ifndef APP_CONFIG_H
#define APP_CONFIG_H

struct Config {
  char wifi_ssid[32] = "Linksys-2.4G-wifi5";
  char wifi_pass[32] = "19811201";
  char mqtt_server[64] = "192.168.1.250";
  int  mqtt_port = 1883;
  char mqtt_user[32] = "";
  char mqtt_pass[32] = "";
  char mqtt_topic[64] = "epd/text";
  char mqtt_weather_topic[64] = "epd/weather";
  char mqtt_date_topic[64] = "epd/date";
  char mqtt_env_topic[64] = "epd/env";
  char mqtt_calendar_topic[64] = "epd/calendar";
  char mqtt_shift_topic[64] = "epd/shift";
  char mqtt_air_quality_topic[64] = "epd/air_quality";
  bool invert_display = false;
};

enum Page {
    PAGE_WEATHER,
    PAGE_CALENDAR
};

#endif
