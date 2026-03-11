#include "bw_format.h"

size_t bw_format_unpack_byte(uint8_t packed_byte, uint8_t *pixels_out) {
  // Extract 8 bits from LSB first (Android app sends in LSB first order to match byte order for pebble)
  // Bit 0 (LSB) = pixel 0, Bit 7 (MSB) = pixel 7
  for (int i = 0; i < 8; i++) {
    pixels_out[i] = (packed_byte >> i) & 1;
  }
  return 8;
}

size_t bw_format_unpack_frame(const uint8_t *packed_data, size_t packed_size,
                               uint8_t *pixels_out, size_t max_pixels) {
  if (!packed_data || !pixels_out) {
    return 0;
  }

  size_t pixel_count = 0;
  uint8_t temp_pixels[8];

  for (size_t byte_idx = 0; byte_idx < packed_size && pixel_count < max_pixels; byte_idx++) {
    bw_format_unpack_byte(packed_data[byte_idx], temp_pixels);

    // Copy unpacked pixels to output, respecting max_pixels limit
    for (int i = 0; i < 8 && pixel_count < max_pixels; i++) {
      pixels_out[pixel_count++] = temp_pixels[i];
    }
  }

  return pixel_count;
}

size_t bw_format_get_packed_size(size_t pixel_count) {
  return (pixel_count + 7) / 8;
}
