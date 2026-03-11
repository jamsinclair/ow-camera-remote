#include <pebble.h>
#include "preview_settings.h"
#include "../app_settings.h"
#include "storage_keys.h"

uint8_t preview_is_enabled(void) {
  return app_get_settings()->preview_enabled;
}

void preview_set_enabled(uint8_t enabled) {
  uint8_t next = enabled ? 1 : 0;
  persist_write_int(STORAGE_KEY_PREVIEW_ENABLED, next);
  app_get_settings()->preview_enabled = next;
}

void preview_load_from_storage(void) {
  uint8_t enabled = 1;  // Default: enabled

  if (persist_exists(STORAGE_KEY_PREVIEW_ENABLED)) {
    enabled = persist_read_int(STORAGE_KEY_PREVIEW_ENABLED);
  }

  app_get_settings()->preview_enabled = enabled;
}
