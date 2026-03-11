#pragma once

#include <pebble.h>
#include "../preview/frame_buffer_manager.h"

// Get current frame format
FrameFormat color_get_format();

// Toggle to next frame format in cycle (4-bit → 1-bit → 4-bit)
void color_toggle_format();

// Load frame format from persistent storage
void color_load_from_storage();
