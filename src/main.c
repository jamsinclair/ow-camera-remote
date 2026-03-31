#include <pebble.h>
#include "comm/comm.h"
#include "windows/menu_window.h"
#include "windows/preview_window.h"
#include "app_settings.h"
#include "preview/frame_buffer_manager.h"
#include "settings/settings.h"
#include "settings/storage_keys.h"

// Global colors used by windows
GColor BG_COLOR;
GColor TEXT_COLOR;

// Global app settings
static AppSettings g_app_settings;

AppSettings* app_get_settings() {
  return &g_app_settings;
}

static void detect_watch_model() {
  g_app_settings.screen_width = PBL_DISPLAY_WIDTH;
  g_app_settings.screen_height = PBL_DISPLAY_HEIGHT;

#ifdef PBL_PLATFORM_APLITE
  strcpy(g_app_settings.model_name, "aplite");
#elif defined(PBL_PLATFORM_BASALT)
  strcpy(g_app_settings.model_name, "basalt");
#elif defined(PBL_PLATFORM_CHALK)
  strcpy(g_app_settings.model_name, "chalk");
#elif defined(PBL_PLATFORM_DIORITE)
  strcpy(g_app_settings.model_name, "diorite");
#elif defined(PBL_PLATFORM_FLINT)
  strcpy(g_app_settings.model_name, "flint");
#elif defined(PBL_PLATFORM_EMERY)
  strcpy(g_app_settings.model_name, "emery");
#else
  strcpy(g_app_settings.model_name, "basalt");
#endif

  APP_LOG(APP_LOG_LEVEL_INFO, "Detected watch model: %s (%ux%u)",
          g_app_settings.model_name, g_app_settings.screen_width, g_app_settings.screen_height);
}

static void init(void) {
  BG_COLOR = COLOR_FALLBACK(GColorBlueMoon, GColorBlack);
  TEXT_COLOR = GColorWhite;

  detect_watch_model();
  settings_load();

  // Initialize frame buffer
  if (!frame_buffer_manager_init()) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to allocate frame buffer");
  }

  init_comm();

  // Push the preview window as default
  preview_window_push();
}

static void deinit(void) {
  frame_buffer_manager_deinit();
  deinit_comm();
}

int main(void) {
  init();
  app_event_loop();
  APP_LOG(APP_LOG_LEVEL_INFO, "Exiting app");
  deinit();
}
