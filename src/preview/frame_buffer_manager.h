#pragma once
#include <pebble.h>

// Timer overlay height used across the app
#define TIMER_OVERLAY_HEIGHT 30

// Frame format enum (extensible for future color support)
typedef enum {
  FRAME_FORMAT_1BIT_BW = 0,    // 1-bit black & white
  FRAME_FORMAT_4BIT_COLOR = 3, // 4-bit color (matches MESSAGE_FORMAT_4BIT_COLOR)
} FrameFormat;

// Initialize frame buffer (call at app startup)
bool frame_buffer_manager_init(void);

// Initialize frame buffer with specific format
bool frame_buffer_manager_init_with_format(FrameFormat format);

// Cleanup frame buffer (call at app shutdown)
void frame_buffer_manager_deinit(void);

// Get the preallocated frame buffer
GBitmap* frame_buffer_manager_get_buffer(void);

// Get current format
FrameFormat frame_buffer_manager_get_format(void);

// Get allocated buffer size in bytes
size_t frame_buffer_manager_get_buffer_size(void);

// Convert FrameFormat to wire format (MessageFormat value)
static inline uint8_t frame_buffer_manager_get_message_format(void) {
  FrameFormat format = frame_buffer_manager_get_format();
  switch (format) {
    case FRAME_FORMAT_1BIT_BW:
      return 0;  // MESSAGE_FORMAT_1BIT_BW
    case FRAME_FORMAT_4BIT_COLOR:
      return 3;  // MESSAGE_FORMAT_4BIT_COLOR
    default:
      return 0;  // Default to 1-bit
  }
}
