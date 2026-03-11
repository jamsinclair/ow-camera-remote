#include <pebble.h>
#include "timer_settings.h"
#include "../app_settings.h"
#include "storage_keys.h"

// Preset timer values in seconds
static const uint16_t TIMER_VALUES[] = {0, 3, 5, 10, 15};
#define NUM_TIMER_VALUES 5

uint16_t timer_get_value() {
  return app_get_settings()->timer_seconds;
}

void timer_increment() {
  uint16_t current = app_get_settings()->timer_seconds;

  // Find current index and move to next
  uint16_t next_index = 0;
  for (uint16_t i = 0; i < NUM_TIMER_VALUES; i++) {
    if (TIMER_VALUES[i] == current) {
      next_index = (i + 1) % NUM_TIMER_VALUES;
      break;
    }
  }

  uint16_t next_value = TIMER_VALUES[next_index];
  persist_write_int(STORAGE_KEY_TIMER, next_value);
  app_get_settings()->timer_seconds = next_value;
}

uint8_t timer_is_vibration_enabled() {
  return app_get_settings()->timer_vibration_enabled;
}

void timer_toggle_vibration() {
  uint8_t current = app_get_settings()->timer_vibration_enabled;
  uint8_t next = !current;
  persist_write_int(STORAGE_KEY_TIMER_VIBRATION, next);
  app_get_settings()->timer_vibration_enabled = next;
}

void timer_load_from_storage() {
  uint16_t timer_value = 0;  // Default to no timer
  uint8_t vibration_enabled = 1;  // Default to enabled

  if (persist_exists(STORAGE_KEY_TIMER)) {
    timer_value = persist_read_int(STORAGE_KEY_TIMER);
  }

  if (persist_exists(STORAGE_KEY_TIMER_VIBRATION)) {
    vibration_enabled = persist_read_int(STORAGE_KEY_TIMER_VIBRATION);
  }

  app_get_settings()->timer_seconds = timer_value;
  app_get_settings()->timer_vibration_enabled = vibration_enabled;
}
