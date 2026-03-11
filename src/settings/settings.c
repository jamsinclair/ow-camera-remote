#include "settings.h"
#include "color_settings.h"
#include "timer_settings.h"
#include "dither_settings.h"
#include "preview_settings.h"

void settings_load(void) {
  color_load_from_storage();
  timer_load_from_storage();
  dither_load_from_storage();
  preview_load_from_storage();
}
