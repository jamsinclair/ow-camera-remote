#include <pebble.h>
#include <time.h>
#include "menu_window.h"
#include "dither_window.h"
#include "preview_window.h"
#include "../app_settings.h"
#include "../settings/color_settings.h"
#include "../settings/timer_settings.h"
#include "../settings/dither_settings.h"
#include "../settings/preview_settings.h"
#include "../preview/frame_buffer_manager.h"
#include "../comm/comm.h"

#if defined(PBL_COLOR)
  #define NUM_MENU_ITEMS 6
#else
  #define NUM_MENU_ITEMS 5
#endif

static Window *s_menu_window;
static SimpleMenuLayer *s_menu_layer;

// Menu items and sections must be static (not local) so they persist
static SimpleMenuItem s_menu_items[NUM_MENU_ITEMS];
static SimpleMenuSection s_menu_sections[1];

// Static buffers for settings subtitles
static char s_dither_subtitle[32];
static char s_timer_subtitle[32];
static char s_preview_subtitle[32];
static char s_timer_vibration_subtitle[32];
#if defined(PBL_COLOR)
  static char s_color_mode_subtitle[32];
#endif

#define MENU_SECTION_MODES 0
#define MENU_ROW_CAMERA 0
#define MENU_ROW_PREVIEW 1
#define MENU_ROW_TIMER 2
#define MENU_ROW_TIMER_VIBRATION 3
#if defined(PBL_COLOR)
  #define MENU_ROW_COLOR_MODE 4
  #define MENU_ROW_SETTINGS 5
#else
  #define MENU_ROW_SETTINGS 4
#endif

static void menu_select_callback(int index, void *ctx) {
  switch (index) {
    case MENU_ROW_CAMERA:
      {
        DictionaryIterator *out_iter;
        app_message_outbox_begin(&out_iter);
        // Toggle camera format: [byte 0: reserved] [bytes 1-4: timestamp]
        uint32_t timestamp = (uint32_t)time(NULL);
        uint8_t toggle_request[5] = {
          0x00,                                   // Byte 0: reserved
          (timestamp & 0xFF),                     // Byte 1: timestamp LSB
          ((timestamp >> 8) & 0xFF),              // Byte 2: timestamp
          ((timestamp >> 16) & 0xFF),             // Byte 3: timestamp
          ((timestamp >> 24) & 0xFF)              // Byte 4: timestamp MSB
        };
        dict_write_data(out_iter, KEY_TOGGLE_CAMERA, toggle_request, sizeof(toggle_request));
        app_message_outbox_send();
      }
      break;
    case MENU_ROW_SETTINGS:
      dither_window_push();
      break;
    case MENU_ROW_TIMER:
      timer_increment();
      // Update subtitle immediately to show new value
      uint16_t timer_val = timer_get_value();
      if (timer_val == 0) {
        snprintf(s_timer_subtitle, sizeof(s_timer_subtitle), "Off");
      } else {
        snprintf(s_timer_subtitle, sizeof(s_timer_subtitle), "%us", timer_val);
      }
      s_menu_items[MENU_ROW_TIMER].subtitle = s_timer_subtitle;
      // Mark menu layer dirty to refresh display
      layer_mark_dirty(simple_menu_layer_get_layer(s_menu_layer));
      break;
    case MENU_ROW_TIMER_VIBRATION:
      timer_toggle_vibration();
      // Update subtitle immediately
      uint8_t vibration_enabled = timer_is_vibration_enabled();
      snprintf(s_timer_vibration_subtitle, sizeof(s_timer_vibration_subtitle), "%s", vibration_enabled ? "On" : "Off");
      s_menu_items[MENU_ROW_TIMER_VIBRATION].subtitle = s_timer_vibration_subtitle;
      layer_mark_dirty(simple_menu_layer_get_layer(s_menu_layer));
      break;
    #if defined(PBL_COLOR)
    case MENU_ROW_COLOR_MODE:
      color_toggle_format();
      // Update subtitle immediately
      FrameFormat format = color_get_format();
      snprintf(s_color_mode_subtitle, sizeof(s_color_mode_subtitle),
               "%s", format == FRAME_FORMAT_4BIT_COLOR ? "4-bit color" : "1-bit grayscale");
      s_menu_items[MENU_ROW_COLOR_MODE].subtitle = s_color_mode_subtitle;
      layer_mark_dirty(simple_menu_layer_get_layer(s_menu_layer));
      break;
    #endif
    case MENU_ROW_PREVIEW:
      preview_toggle();
      // Update subtitle immediately
      uint8_t preview_enabled = preview_is_enabled();
      snprintf(s_preview_subtitle, sizeof(s_preview_subtitle), "%s", preview_enabled ? "On" : "Off");
      s_menu_items[MENU_ROW_PREVIEW].subtitle = s_preview_subtitle;
      layer_mark_dirty(simple_menu_layer_get_layer(s_menu_layer));
      break;
  }
}

static void menu_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_menu_layer = simple_menu_layer_create(bounds, window, s_menu_sections, 1, NULL);
  layer_add_child(window_layer, simple_menu_layer_get_layer(s_menu_layer));
}


static void menu_window_appear(Window *window) {
  // Update dither subtitle
  uint8_t dithering = dither_get_algorithm();
  const char *dither_names[] = {"Floyd-Steinberg", "Bayer 2x2", "Bayer 4x4", "Bayer 8x8", "Atkinson"};
  snprintf(s_dither_subtitle, sizeof(s_dither_subtitle), "%s", dither_names[dithering]);
  s_menu_items[MENU_ROW_SETTINGS].subtitle = s_dither_subtitle;

  // Update timer subtitle
  uint16_t timer_val = timer_get_value();
  if (timer_val == 0) {
    snprintf(s_timer_subtitle, sizeof(s_timer_subtitle), "Off");
  } else {
    snprintf(s_timer_subtitle, sizeof(s_timer_subtitle), "%us", timer_val);
  }
  s_menu_items[MENU_ROW_TIMER].subtitle = s_timer_subtitle;

  // Update timer vibration subtitle
  uint8_t vibration_enabled = timer_is_vibration_enabled();
  snprintf(s_timer_vibration_subtitle, sizeof(s_timer_vibration_subtitle), "%s", vibration_enabled ? "On" : "Off");
  s_menu_items[MENU_ROW_TIMER_VIBRATION].subtitle = s_timer_vibration_subtitle;

  #if defined(PBL_COLOR)
  // Update color mode subtitle
  FrameFormat color_format = color_get_format();
  snprintf(s_color_mode_subtitle, sizeof(s_color_mode_subtitle),
           "%s", color_format == FRAME_FORMAT_4BIT_COLOR ? "4-bit color" : "1-bit grayscale");
  s_menu_items[MENU_ROW_COLOR_MODE].subtitle = s_color_mode_subtitle;
  #endif

  // Update preview subtitle
  uint8_t preview_enabled = preview_is_enabled();
  snprintf(s_preview_subtitle, sizeof(s_preview_subtitle), "%s", preview_enabled ? "On" : "Off");
  s_menu_items[MENU_ROW_PREVIEW].subtitle = s_preview_subtitle;

  // Mark the menu layer as dirty to refresh the display
  layer_mark_dirty(simple_menu_layer_get_layer(s_menu_layer));
}

static void menu_window_unload(Window *window) {
  simple_menu_layer_destroy(s_menu_layer);
}

void menu_window_push() {
  // Initialize menu items
  s_menu_items[MENU_ROW_CAMERA] = (SimpleMenuItem){
    .title = "Switch Camera",
    .subtitle = "Toggle front or back",
    .callback = menu_select_callback,
  };

  // Initialize preview menu item
  uint8_t preview_enabled = preview_is_enabled();
  snprintf(s_preview_subtitle, sizeof(s_preview_subtitle), "%s", preview_enabled ? "On" : "Off");

  s_menu_items[MENU_ROW_PREVIEW] = (SimpleMenuItem){
    .title = "Preview",
    .subtitle = s_preview_subtitle,
    .callback = menu_select_callback,
  };

  // Get timer value for subtitle
  uint16_t timer_val = timer_get_value();
  if (timer_val == 0) {
    snprintf(s_timer_subtitle, sizeof(s_timer_subtitle), "Off");
  } else {
    snprintf(s_timer_subtitle, sizeof(s_timer_subtitle), "%us", timer_val);
  }

  s_menu_items[MENU_ROW_TIMER] = (SimpleMenuItem){
    .title = "Timer",
    .subtitle = s_timer_subtitle,
    .callback = menu_select_callback,
  };

  // Get timer vibration setting
  uint8_t vibration_enabled = timer_is_vibration_enabled();
  snprintf(s_timer_vibration_subtitle, sizeof(s_timer_vibration_subtitle), "%s", vibration_enabled ? "On" : "Off");

  s_menu_items[MENU_ROW_TIMER_VIBRATION] = (SimpleMenuItem){
    .title = "Timer Vibration",
    .subtitle = s_timer_vibration_subtitle,
    .callback = menu_select_callback,
  };

  #if defined(PBL_COLOR)
  // Get color mode for subtitle
  FrameFormat color_format = color_get_format();
  snprintf(s_color_mode_subtitle, sizeof(s_color_mode_subtitle),
           "%s", color_format == FRAME_FORMAT_4BIT_COLOR ? "4-bit color" : "1-bit grayscale");

  s_menu_items[MENU_ROW_COLOR_MODE] = (SimpleMenuItem){
    .title = "Color Mode",
    .subtitle = s_color_mode_subtitle,
    .callback = menu_select_callback,
  };
  #endif

  // Get dither algorithm for subtitle
  uint8_t dithering = dither_get_algorithm();
  const char *dither_names[] = {"Floyd-Steinberg", "Bayer 2x2", "Bayer 4x4", "Bayer 8x8", "Atkinson"};
  snprintf(s_dither_subtitle, sizeof(s_dither_subtitle), "%s", dither_names[dithering]);

  s_menu_items[MENU_ROW_SETTINGS] = (SimpleMenuItem){
    .title = "Dither Setting",
    .subtitle = s_dither_subtitle,
    .callback = menu_select_callback,
  };

  s_menu_sections[0] = (SimpleMenuSection){
    .num_items = NUM_MENU_ITEMS,
    .items = s_menu_items,
  };

  if (!s_menu_window) {
    s_menu_window = window_create();
    window_set_background_color(s_menu_window, GColorBlack);
    window_set_window_handlers(s_menu_window, (WindowHandlers) {
      .load = menu_window_load,
      .appear = menu_window_appear,
      .unload = menu_window_unload,
    });
  }
  window_stack_push(s_menu_window, true);
}
