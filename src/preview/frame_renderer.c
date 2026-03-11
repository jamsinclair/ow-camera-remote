#include "frame_renderer.h"
#include "frame_buffer_manager.h"
#include "bw_format.h"
#include "message_header.h"
#include <string.h>

size_t frame_renderer_get_expected_size(uint16_t width, uint16_t height) {
  // 1-bit black & white format: 1 pixel per bit, plus 1 header byte
  size_t pixel_count = width * height;
  return 1 + bw_format_get_packed_size(pixel_count);
}

bool frame_renderer_render(GBitmap *bitmap, uint8_t *frame_data, size_t frame_size) {
  if (!bitmap || !frame_data) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "frame_renderer: null bitmap or data");
    return false;
  }

  // Parse message header
  FirstMessageHeader header;
  if (!parse_first_message_header(frame_data, frame_size, &header)) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "frame_renderer: failed to parse header");
    return false;
  }

  // Validate format matches expected 1-bit B&W
  if (header.format != MESSAGE_FORMAT_1BIT_BW) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "frame_renderer: unexpected format %d (expected B&W)", header.format);
    return false;
  }

  if (header.multi_message) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "frame_renderer: multi-message flag set for B&W (unexpected)");
  }

  GRect bounds = gbitmap_get_bounds(bitmap);
  uint16_t width = bounds.size.w;
  uint16_t height = bounds.size.h;

  APP_LOG(APP_LOG_LEVEL_DEBUG, "frame_renderer: bitmap size %ux%u, payload_size=%zu",
          width, height, header.payload_size);

  GBitmapFormat format = gbitmap_get_format(bitmap);
  uint8_t *bitmap_data = gbitmap_get_data(bitmap);
  uint16_t bytes_per_row = gbitmap_get_bytes_per_row(bitmap);

  if (!bitmap_data) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "frame_renderer: failed to get bitmap data");
    return false;
  }

  // Validate format matches manager's current format
  FrameFormat manager_format = frame_buffer_manager_get_format();
  if (manager_format != FRAME_FORMAT_1BIT_BW) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "frame_renderer: manager format mismatch (%d)", manager_format);
    return false;
  }

  if (format != GBitmapFormat1Bit) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "frame_renderer: expected GBitmapFormat1Bit, got %d", format);
    return false;
  }

  // Calculate expected packed size per row
  uint16_t expected_bytes_per_row = (width + 7) / 8;
  size_t expected_packed_size = expected_bytes_per_row * height;

  APP_LOG(APP_LOG_LEVEL_DEBUG, "frame_renderer: expected_packed_size=%zu, actual_payload_size=%zu, bytes_per_row=%u, expected_bytes_per_row=%u",
          expected_packed_size, header.payload_size, bytes_per_row, expected_bytes_per_row);

  // Phone sends continuous bit stream, need to extract bits and repack with row stride
  // No temporary allocation - extract bits directly from payload
  size_t total_pixels = width * height;

  // Clear bitmap data first
  memset(bitmap_data, 0, bytes_per_row * height);

  // Unpack bits from packed payload and pack into bitmap rows
  uint8_t unpacked_pixels[8];
  size_t pixel_idx = 0;

  for (size_t byte_idx = 0; byte_idx < header.payload_size && pixel_idx < total_pixels; byte_idx++) {
    bw_format_unpack_byte(header.payload[byte_idx], unpacked_pixels);

    // Process the 8 unpacked pixels
    for (int i = 0; i < 8 && pixel_idx < total_pixels; i++, pixel_idx++) {
      uint8_t pixel = unpacked_pixels[i];

      // Calculate row and column
      uint16_t row = pixel_idx / width;
      uint16_t col = pixel_idx % width;

      // Pack into bitmap row (LSB first for bitmap display)
      uint16_t bitmap_byte_offset = col / 8;
      uint8_t bitmap_bit_idx = col % 8;
      uint8_t *bitmap_byte = bitmap_data + row * bytes_per_row + bitmap_byte_offset;

      if (pixel) {
        *bitmap_byte |= (1 << bitmap_bit_idx);
      }
    }
  }

  APP_LOG(APP_LOG_LEVEL_DEBUG, "frame_renderer: rendered frame %ux%u from %zu bytes", width, height, header.payload_size);
  return true;
}
