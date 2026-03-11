#pragma once

#include <pebble.h>

// Get current preview enabled state
uint8_t preview_is_enabled(void);

// Set preview enabled state and save to storage
void preview_set_enabled(uint8_t enabled);

// Load preview settings from persistent storage
void preview_load_from_storage(void);
