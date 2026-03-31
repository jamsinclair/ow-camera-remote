#pragma once

#include <pebble.h>

#if defined(PBL_COLOR)
  #include "../settings/color_settings.h"
#endif

// Push the menu window onto the stack
void menu_window_push();
