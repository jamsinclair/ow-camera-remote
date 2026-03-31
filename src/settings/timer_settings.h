#pragma once

#include <pebble.h>

// Get current timer value in seconds
uint16_t timer_get_value();

// Set timer to next value in cycle (0 → 3 → 5 → 10 → 15 → 0)
void timer_increment();

// Get whether timer vibration is enabled
uint8_t timer_is_vibration_enabled();

// Toggle timer vibration on/off
void timer_toggle_vibration();

// Load timer setting from persistent storage
void timer_load_from_storage();
