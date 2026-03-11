#pragma once

#include <pebble.h>

// Render preview frame data to a GBitmap
// Expects frame_data to start with a message header byte followed by pixel data
// Returns true on success, false on header parse error or format mismatch
bool frame_renderer_render(GBitmap *bitmap, uint8_t *frame_data, size_t frame_size);

// Get expected frame size in bytes for given dimensions (includes 1 header byte)
size_t frame_renderer_get_expected_size(uint16_t width, uint16_t height);
