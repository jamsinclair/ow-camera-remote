#pragma once

#include <pebble.h>

typedef struct {
  char model_name[16];
  uint16_t screen_width;
  uint16_t screen_height;
  uint8_t dithering_algorithm;
  uint16_t timer_seconds;
  uint8_t color_format;
  uint8_t timer_vibration_enabled;
  uint8_t preview_enabled;
} AppSettings;

AppSettings* app_get_settings();
