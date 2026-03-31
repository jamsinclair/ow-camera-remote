#include <pebble.h>
#include "dither_settings.h"
#include "../app_settings.h"
#include "storage_keys.h"

uint8_t dither_get_algorithm() {
  return app_get_settings()->dithering_algorithm;
}

void dither_save_to_storage(uint8_t algorithm) {
  persist_write_int(STORAGE_KEY_DITHERING, algorithm);
  app_get_settings()->dithering_algorithm = algorithm;
}

void dither_load_from_storage() {
  uint8_t algorithm = 0;  // Default to Floyd-Steinberg

  if (persist_exists(STORAGE_KEY_DITHERING)) {
    algorithm = persist_read_int(STORAGE_KEY_DITHERING);
  }

  app_get_settings()->dithering_algorithm = algorithm;
}
