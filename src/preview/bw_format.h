#pragma once

#include <stdint.h>
#include <stddef.h>

// 1-bit black & white format
// Packs 8 pixels per byte, LSB first (least significant bit first)
// 1 = white, 0 = black
// Byte layout: bit 0 (LSB) = pixel 0, bit 7 (MSB) = pixel 7

// Unpack 8 pixels from a single byte
// pixels_out should have space for 8 uint8_t values
size_t bw_format_unpack_byte(uint8_t packed_byte, uint8_t *pixels_out);

// Unpack a full frame from packed 1-bit data
// pixels_out should have space for pixel_count values (1 byte per pixel)
size_t bw_format_unpack_frame(const uint8_t *packed_data, size_t packed_size,
                               uint8_t *pixels_out, size_t max_pixels);

// Get the packed size needed for a given pixel count
size_t bw_format_get_packed_size(size_t pixel_count);
