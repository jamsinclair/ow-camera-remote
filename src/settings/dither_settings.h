#pragma once

#include <pebble.h>

// Get current dithering algorithm
uint8_t dither_get_algorithm();

// Save dithering algorithm to persistent storage
void dither_save_to_storage(uint8_t algorithm);

// Load dithering algorithm from persistent storage
void dither_load_from_storage();
