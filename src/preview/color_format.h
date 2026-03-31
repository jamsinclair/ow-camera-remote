#pragma once

#include <stddef.h>
#include <stdint.h>
#include <pebble.h>

#define COLOR_PALETTE_SIZE 16

// Parse 16-byte palette into GColor array
// palette_bytes: raw 16 bytes from message (GColor8 indices)
// colors_out: array of 16 GColor values
void color_format_parse_palette(const uint8_t *palette_bytes, GColor *colors_out);

// Calculate packed size for 4-bit format (2 pixels per byte)
size_t color_format_get_packed_size(size_t pixel_count);
