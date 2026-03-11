#include <pebble.h>
#include "dither_window.h"
#include "../settings/dither_settings.h"

typedef struct {
  uint8_t value;
  const char *label;
} DitheringOption;

static const DitheringOption DITHERING_OPTIONS[] = {
  {0, "Floyd-Steinberg"},
  {1, "Bayer 2x2"},
  {2, "Bayer 4x4"},
  {3, "Bayer 8x8"},
  {4, "Atkinson"}
};
#define NUM_DITHERING_OPTIONS 5

static Window *s_dither_window;
static SimpleMenuLayer *s_menu_layer;

// Menu items and section (must be static, not local)
static SimpleMenuItem s_menu_items[NUM_DITHERING_OPTIONS];
static SimpleMenuSection s_menu_sections[1];

// Static title buffers for menu items
static char s_menu_titles[NUM_DITHERING_OPTIONS][32];

static void menu_select_callback(int index, void *context) {
  uint8_t selected_algorithm = DITHERING_OPTIONS[index].value;
  dither_save_to_storage(selected_algorithm);

  // Pop the settings window to return to menu
  window_stack_pop(true);
}

static void dither_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // Create SimpleMenuLayer with the static arrays
  s_menu_layer = simple_menu_layer_create(bounds, window, s_menu_sections, 1, NULL);

  // Set initial selection to current dithering algorithm
  uint8_t current_algorithm = dither_get_algorithm();
  uint16_t selected_index = 0;
  for (uint16_t i = 0; i < NUM_DITHERING_OPTIONS; i++) {
    if (DITHERING_OPTIONS[i].value == current_algorithm) {
      selected_index = i;
      break;
    }
  }
  simple_menu_layer_set_selected_index(s_menu_layer, selected_index, false);

  layer_add_child(window_layer, simple_menu_layer_get_layer(s_menu_layer));
}

static void dither_window_unload(Window *window) {
  simple_menu_layer_destroy(s_menu_layer);
}

void dither_window_push() {
  // Initialize menu items with dithering options
  for (uint16_t i = 0; i < NUM_DITHERING_OPTIONS; i++) {
    snprintf(s_menu_titles[i], sizeof(s_menu_titles[i]), "%s", DITHERING_OPTIONS[i].label);

    s_menu_items[i] = (SimpleMenuItem) {
      .title = s_menu_titles[i],
      .callback = menu_select_callback,
    };
  }

  // Initialize section with menu items
  s_menu_sections[0] = (SimpleMenuSection) {
    .num_items = NUM_DITHERING_OPTIONS,
    .items = s_menu_items,
  };

  if (!s_dither_window) {
    s_dither_window = window_create();
    window_set_background_color(s_dither_window, GColorBlack);
    window_set_window_handlers(s_dither_window, (WindowHandlers) {
      .load = dither_window_load,
      .unload = dither_window_unload,
    });
  }
  window_stack_push(s_dither_window, true);
}
