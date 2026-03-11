#include <pebble.h>
#include "color_settings.h"
#include "../app_settings.h"
#include "storage_keys.h"

FrameFormat color_get_format() {
  return (FrameFormat)app_get_settings()->color_format;
}

void color_toggle_format() {
  FrameFormat current = (FrameFormat)app_get_settings()->color_format;
  FrameFormat next = (current == FRAME_FORMAT_4BIT_COLOR)
                     ? FRAME_FORMAT_1BIT_BW
                     : FRAME_FORMAT_4BIT_COLOR;

  persist_write_int(STORAGE_KEY_COLOR_MODE, (int)next);
  app_get_settings()->color_format = next;
}

void color_load_from_storage() {
#ifdef PBL_COLOR
  FrameFormat format = FRAME_FORMAT_4BIT_COLOR;  // default for color displays
#else
  FrameFormat format = FRAME_FORMAT_1BIT_BW;    // default for B&W displays
#endif

  if (persist_exists(STORAGE_KEY_COLOR_MODE)) {
    int stored = persist_read_int(STORAGE_KEY_COLOR_MODE);

    if (stored == 0 || stored == 3) {
      // Valid frame format values
      format = (FrameFormat)stored;
    } else {
      // Invalid value, persist default
      persist_write_int(STORAGE_KEY_COLOR_MODE, (int)format);
    }
  }

  app_get_settings()->color_format = format;
}
