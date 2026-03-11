#include "frame_buffer_manager.h"
#include "../app_settings.h"
#ifdef PBL_COLOR
#include "../settings/color_settings.h"
#endif

// Static variables for frame buffer management
static GBitmap *s_frame_buffer = NULL;
static FrameFormat s_current_format = FRAME_FORMAT_1BIT_BW;
static size_t s_allocated_size = 0;

bool frame_buffer_manager_init(void) {
  // Determine format based on watch capabilities and color mode setting
  FrameFormat format = FRAME_FORMAT_1BIT_BW;

#ifdef PBL_COLOR
  // For color-capable watches, use the user's color format preference
  format = color_get_format();
  APP_LOG(APP_LOG_LEVEL_INFO, "frame_buffer_manager: Using format=%d", format);
#else
  APP_LOG(APP_LOG_LEVEL_INFO, "frame_buffer_manager: Using 1-bit B&W (no PBL_COLOR)");
#endif

  return frame_buffer_manager_init_with_format(format);
}

bool frame_buffer_manager_init_with_format(FrameFormat format) {
  // Destroy old frame buffer if it exists (for reinitialization when color mode changes)
  if (s_frame_buffer) {
    gbitmap_destroy(s_frame_buffer);
    s_frame_buffer = NULL;
  }

  s_current_format = format;

  AppSettings *settings = app_get_settings();

  if (!settings) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "frame_buffer_manager: Failed to get app settings");
    return false;
  }

  // Calculate frame size: screen dimensions minus action bar width (side action bar)
  uint16_t frame_height = settings->screen_height;
  uint16_t frame_width = settings->screen_width - TIMER_OVERLAY_HEIGHT;

  if (frame_height <= 0 || frame_width <= 0) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "frame_buffer_manager: Invalid dimensions %ux%u",
            frame_width, frame_height);
    return false;
  }

  // Create GSize for bitmap
  GSize frame_size = GSize(frame_width, frame_height);

  // Allocate frame buffer with appropriate format
  GBitmapFormat bitmap_format;
  switch (format) {
    case FRAME_FORMAT_1BIT_BW:
      bitmap_format = GBitmapFormat1Bit;
      break;
    case FRAME_FORMAT_4BIT_COLOR:
      bitmap_format = GBitmapFormat4BitPalette;
      break;
    default:
      APP_LOG(APP_LOG_LEVEL_ERROR, "frame_buffer_manager: Unknown format %d", format);
      return false;
  }

  s_frame_buffer = gbitmap_create_blank(frame_size, bitmap_format);

  if (!s_frame_buffer) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "frame_buffer_manager: Failed to allocate frame buffer");
    return false;
  }

  // Calculate allocated size
  // gbitmap_get_bytes_per_row handles the row stride
  uint16_t bytes_per_row = gbitmap_get_bytes_per_row(s_frame_buffer);
  s_allocated_size = bytes_per_row * frame_height;

  APP_LOG(APP_LOG_LEVEL_INFO,
          "frame_buffer_manager: Initialized frame buffer - format=%d, dimensions=%ux%u, bytes_per_row=%u, allocated=%zu bytes",
          format, frame_width, frame_height, bytes_per_row, s_allocated_size);

  return true;
}

void frame_buffer_manager_deinit(void) {
  if (s_frame_buffer) {
    gbitmap_destroy(s_frame_buffer);
    s_frame_buffer = NULL;
  }
  s_allocated_size = 0;
  APP_LOG(APP_LOG_LEVEL_INFO, "frame_buffer_manager: Deinitialized");
}

GBitmap* frame_buffer_manager_get_buffer(void) {
  return s_frame_buffer;
}

FrameFormat frame_buffer_manager_get_format(void) {
  return s_current_format;
}

size_t frame_buffer_manager_get_buffer_size(void) {
  return s_allocated_size;
}
