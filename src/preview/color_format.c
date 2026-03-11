#include "color_format.h"

void color_format_parse_palette(const uint8_t *palette_bytes, GColor *colors_out) {
  if (!palette_bytes || !colors_out) {
    return;
  }

  for (int i = 0; i < COLOR_PALETTE_SIZE; i++) {
    // GColor8 format: AARRGGBB (2-2-2-2 bits)
    colors_out[i] = (GColor){.argb = palette_bytes[i]};
  }
}

size_t color_format_get_packed_size(size_t pixel_count) {
  // 2 pixels per byte (4 bits each), round up
  return (pixel_count + 1) / 2;
}
