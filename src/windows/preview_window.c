#include <pebble.h>
#include <time.h>
#include "preview_window.h"
#include "menu_window.h"
#include "dither_window.h"
#include "../settings/timer_settings.h"
#include "../settings/color_settings.h"
#include "../comm/comm.h"
#include "../preview/frame_buffer_manager.h"
#include "../preview/message_assembler.h"
#include "../preview/message_header.h"
#include "../settings/preview_settings.h"
#include "../app_settings.h"

static Window *s_preview_window;
static Layer *s_canvas_layer;
static ActionBarLayer *s_action_bar;
static TextLayer *s_countdown_layer;
static GBitmap *s_frame_buffer;
static GBitmap *s_camera_bitmap;
static GBitmap *s_gear_bitmap;
static GBitmap *s_timer_bitmaps[5];  // Bitmaps for 0, 3, 5, 10, 15 seconds

// Frame request state
static bool s_frame_requested = false;
static AppTimer *s_request_retry_timer = NULL;
#define REQUEST_TIMEOUT_MS 4000
static uint8_t s_timeout_count = 0;  // Track consecutive timeouts
#define TIMEOUT_THRESHOLD 2  // Show "app open?" message after 2 timeouts
static bool s_frame_received = false;  // Track if we've received any frame data


// Countdown state
static AppTimer *s_countdown_timer = NULL;
static AppTimer *s_countdown_clear_timer = NULL;
static uint16_t s_countdown_remaining = 0;
static char s_countdown_text[16];

// Picture taken protection
static bool s_expecting_picture_taken = false;

// External color globals
extern GColor TEXT_COLOR;

// Timer icon resource IDs (need to be defined in resources)
static const uint32_t TIMER_RESOURCE_IDS[] = {
  RESOURCE_ID_TIMER_0,
  RESOURCE_ID_TIMER_3,
  RESOURCE_ID_TIMER_5,
  RESOURCE_ID_TIMER_10,
  RESOURCE_ID_TIMER_15
};

/********************************* Timer Icon ************************************/

static uint8_t get_timer_index() {
  uint16_t timer_val = timer_get_value();
  // Match timer value to index (0, 3, 5, 10, 15)
  const uint16_t timer_values[] = {0, 3, 5, 10, 15};
  for (uint8_t i = 0; i < 5; i++) {
    if (timer_values[i] == timer_val) {
      return i;
    }
  }
  return 0;  // Default to index 0 if not found
}

static void update_timer_icon() {
  uint8_t index = get_timer_index();
  action_bar_layer_set_icon(s_action_bar, BUTTON_ID_UP, s_timer_bitmaps[index]);
}


/********************************* Frame Request and Rendering ************************************/

// Forward declarations
static void request_retry_handler(void *data);

static void assembler_timeout_handler(void) {
  APP_LOG(APP_LOG_LEVEL_INFO, "assembler_timeout_handler: multi-message frame assembly timed out");
  // Don't request frames if preview is disabled
  if (!preview_is_enabled()) {
    return;
  }
  // Request next frame when assembly times out
  AppSettings *settings = app_get_settings();
  uint8_t format = (uint8_t)color_get_format();
  send_request_next_frame(model_name_to_enum(settings->model_name), format, settings->dithering_algorithm);
  s_frame_requested = true;
  s_request_retry_timer = app_timer_register(REQUEST_TIMEOUT_MS, request_retry_handler, NULL);
}

static void request_retry_handler(void *data) {
  s_request_retry_timer = NULL;
  if (s_frame_requested && preview_is_enabled()) {
    s_timeout_count++;
    APP_LOG(APP_LOG_LEVEL_WARNING, "Frame request timeout, retrying... (count: %d)", s_timeout_count);

    if (s_timeout_count >= 100) {
      s_timeout_count = TIMEOUT_THRESHOLD; // Cap at threshold to avoid overflow
    }

    // Redraw canvas to update timeout message
    layer_mark_dirty(s_canvas_layer);

    AppSettings *settings = app_get_settings();
    uint8_t format = (uint8_t)color_get_format();
    send_request_next_frame(model_name_to_enum(settings->model_name), format, settings->dithering_algorithm);
    s_request_retry_timer = app_timer_register(REQUEST_TIMEOUT_MS, request_retry_handler, NULL);
  }
}

/********************************* Frame Assembly Callback ************************************/

// Callback invoked when a complete frame is assembled and ready to render
static void frame_assembly_complete_callback(MessageFormat format, GBitmap *bitmap) {
  if (!bitmap) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "frame_assembly_complete_callback: bitmap is NULL");
    return;
  }

  // Reset timeout counter on successful frame reception
  s_timeout_count = 0;

  // Trigger canvas redraw
  layer_mark_dirty(s_canvas_layer);

  // Only request next frame if preview is enabled
  if (!preview_is_enabled()) {
    return;
  }

  // Request next frame immediately (processing is done, request early)
  AppSettings *settings = app_get_settings();
  uint8_t format_to_send = (uint8_t)color_get_format();
  send_request_next_frame(model_name_to_enum(settings->model_name), format_to_send, settings->dithering_algorithm);
  s_frame_requested = true;
  s_request_retry_timer = app_timer_register(REQUEST_TIMEOUT_MS, request_retry_handler, NULL);
}

static void preview_data_callback(uint8_t *data, size_t length) {
  if (!s_frame_buffer) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "preview_data_callback: frame buffer is NULL");
    return;
  }

  // Validate timestamp to detect and drop stale frames
  if (length >= 5) {
    uint32_t msg_timestamp = data[1] | ((uint32_t)data[2] << 8) | ((uint32_t)data[3] << 16) | ((uint32_t)data[4] << 24);
    uint32_t now = (uint32_t)time(NULL);
    uint32_t age = (now >= msg_timestamp) ? (now - msg_timestamp) : 0;

    if (now >= msg_timestamp && age > STALE_MSG_THRESHOLD_S) {
      APP_LOG(APP_LOG_LEVEL_WARNING, "Dropping stale frame (age=%us)", (unsigned int)age);
      return;
    }
  }

  // Cancel retry timer (frame is valid)
  if (s_request_retry_timer) {
    app_timer_cancel(s_request_retry_timer);
    s_request_retry_timer = NULL;
  }
  s_frame_requested = false;
  if (!s_frame_received) {
    APP_LOG(APP_LOG_LEVEL_INFO, "preview_data_callback: first frame accepted at t=%lu", (unsigned long)time(NULL));
  }
  s_frame_received = true;

  // REQUEST EARLY: Parse header quickly and request next item before processing
  if (length >= 1) {
    uint8_t header_byte = data[0];

    // Check if continuation message (bit 0 = 1)
    if (header_byte & 0x01) {
      // Continuation message - extract is_last_chunk from bit 6
      bool is_last_chunk = (header_byte >> 6) & 0x01;

      if (!is_last_chunk) {
        // More chunks coming - request next chunk immediately
        uint8_t chunk_number = (header_byte >> 3) & 0x07;
        uint8_t next_chunk = chunk_number + 1;
        FrameFormat format = frame_buffer_manager_get_format();
        send_request_next_chunk(next_chunk, format);
      }
      // If is_last_chunk, the frame_assembly_complete_callback will request next frame
    } else {
      // First message - check if multi-message
      uint8_t format_bits = (header_byte >> 3) & 0x07;
      bool multi_message = (header_byte >> 6) & 0x01;

      if (multi_message) {
        // Multi-message frame - request chunk 1 immediately
        send_request_next_chunk(1, format_bits);
      }
      // If single message, the frame_assembly_complete_callback will request next frame
    }
  }

  // Process frame through message assembler
  if (!message_assembler_process(data, length, frame_assembly_complete_callback)) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Failed to process frame through assembler");
    return;
  }
}

static void countdown_clear_timeout_handler(void *data);

static void picture_taken_callback() {
  APP_LOG(APP_LOG_LEVEL_INFO, "picture_taken_callback: Phone sent KEY_PICTURE_TAKEN");

  // Only vibrate if we're actually expecting this picture
  if (!s_expecting_picture_taken) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "picture_taken_callback: Ignoring unexpected picture_taken message (not expecting one)");
    return;
  }

  APP_LOG(APP_LOG_LEVEL_INFO, "picture_taken_callback: Picture confirmed - vibrating");
  vibes_double_pulse();
  s_expecting_picture_taken = false;

  // Cancel countdown timer if running
  if (s_countdown_timer) {
    app_timer_cancel(s_countdown_timer);
    s_countdown_timer = NULL;
  }

  // Cancel clear timer if running (picture arrived in time)
  if (s_countdown_clear_timer) {
    app_timer_cancel(s_countdown_clear_timer);
    s_countdown_clear_timer = NULL;
  }

  s_countdown_remaining = 0;
  if (s_countdown_layer) {
    text_layer_set_text(s_countdown_layer, "");
    layer_mark_dirty(text_layer_get_layer(s_countdown_layer));
  }

  // Resume frame requests after capture completes (only if preview is enabled)
  if (preview_is_enabled()) {
    AppSettings *settings = app_get_settings();
    uint8_t format_to_send = (uint8_t)color_get_format();
    send_request_next_frame(model_name_to_enum(settings->model_name), format_to_send, settings->dithering_algorithm);
    s_frame_requested = true;
    s_request_retry_timer = app_timer_register(REQUEST_TIMEOUT_MS, request_retry_handler, NULL);
  }
}

static void countdown_tick_handler(void *data) {
  s_countdown_timer = NULL;

  if (s_countdown_remaining > 0) {
    s_countdown_remaining--;

    if (s_countdown_layer) {
      snprintf(s_countdown_text, sizeof(s_countdown_text), "%u", s_countdown_remaining);
      text_layer_set_text(s_countdown_layer, s_countdown_text);
      layer_mark_dirty(text_layer_get_layer(s_countdown_layer));
    }

    if (s_countdown_remaining > 0) {
      // Vibrate if enabled (but not on the final tick when reaching 0)
      if (timer_is_vibration_enabled()) {
        vibes_short_pulse();
      }
      s_countdown_timer = app_timer_register(1000, countdown_tick_handler, NULL);
    } else {
      // Countdown reached 0 - schedule clear timer in case picture_taken doesn't arrive
      s_countdown_clear_timer = app_timer_register(2000, countdown_clear_timeout_handler, NULL);
    }
  }
}

static void countdown_clear_timeout_handler(void *data) {
  s_countdown_clear_timer = NULL;

  // Stop expecting picture if it didn't arrive within timeout
  s_expecting_picture_taken = false;

  s_countdown_remaining = 0;
  if (s_countdown_layer) {
    text_layer_set_text(s_countdown_layer, "");
    layer_mark_dirty(text_layer_get_layer(s_countdown_layer));
  }
}

static void start_countdown_overlay(uint16_t seconds) {
  if (!s_countdown_layer || seconds == 0) {
    return;
  }

  // Cancel any existing countdown
  if (s_countdown_timer) {
    app_timer_cancel(s_countdown_timer);
  }

  // Cancel any existing clear timer
  if (s_countdown_clear_timer) {
    app_timer_cancel(s_countdown_clear_timer);
    s_countdown_clear_timer = NULL;
  }

  s_countdown_remaining = seconds;
  snprintf(s_countdown_text, sizeof(s_countdown_text), "%u", s_countdown_remaining);
  text_layer_set_text(s_countdown_layer, s_countdown_text);
  layer_mark_dirty(text_layer_get_layer(s_countdown_layer));

  // Vibrate immediately if vibration enabled
  if (timer_is_vibration_enabled()) {
    vibes_short_pulse();
  }

  s_countdown_timer = app_timer_register(1000, countdown_tick_handler, NULL);
}

static void capture_send_result_callback(bool success) {
  if (success) {
    APP_LOG(APP_LOG_LEVEL_INFO, "capture_send_result_callback: Companion app acknowledged capture command");
    // Mark that we're expecting a picture from this capture
    s_expecting_picture_taken = true;
    // Start countdown overlay now that companion app acknowledged the capture
    extern uint16_t timer_get_value();
    uint16_t timer_seconds = timer_get_value();
    if (timer_seconds > 0) {
      start_countdown_overlay(timer_seconds);
    }
  }
  // Silently ignore timeout - no feedback if companion app doesn't acknowledge
}

static void start_camera_countdown() {
  extern uint16_t timer_get_value();
  uint16_t timer_seconds = timer_get_value();
  APP_LOG(APP_LOG_LEVEL_INFO, "start_camera_countdown: preparing to send capture message with %u second timer", timer_seconds);
  // Send capture message to phone with timer value
  // Use capture_send_result_callback to start countdown only if companion app acknowledges the capture command
  extern void send_capture_with_ack(int timer_seconds, SendResultCallback *ack_callback);
  send_capture_with_ack(timer_seconds, capture_send_result_callback);
}

/********************************* Canvas Layer ************************************/

static void canvas_update_proc(Layer *this_layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(this_layer);

  // Draw frame buffer only if preview is enabled and we've received data and haven't timed out
  if (preview_is_enabled() && s_frame_received && s_frame_buffer && s_timeout_count < TIMEOUT_THRESHOLD) {
    graphics_draw_bitmap_in_rect(ctx, s_frame_buffer, bounds);
  } else if (!preview_is_enabled()) {
    // Preview is disabled - show "Preview Off" message
    graphics_context_set_fill_color(ctx, TEXT_COLOR);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);
    graphics_context_set_text_color(ctx, GColorBlack);

    GRect text_bounds;
    #ifdef PBL_ROUND
    text_bounds.origin.x = 15;
    text_bounds.size.w = bounds.size.w - 30;
    text_bounds.size.h = 70;
    text_bounds.origin.y = (bounds.size.h / 2) - 35;
    #else
    text_bounds.origin.x = 10;
    text_bounds.size.w = bounds.size.w - 20;
    text_bounds.size.h = 80;
    text_bounds.origin.y = (bounds.size.h / 2) - 40;
    #endif

    graphics_draw_text(ctx, "Preview\n\nOff", fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                       text_bounds, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  } else {
    // Draw light background (waiting for frames)
    graphics_context_set_fill_color(ctx, TEXT_COLOR);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);

    // Draw text with dark color on light background
    graphics_context_set_text_color(ctx, GColorBlack);

    GRect text_bounds;
    #ifdef PBL_ROUND
    // For round displays, center text better within the circular area
    text_bounds.origin.x = 15;
    text_bounds.size.w = bounds.size.w - 30;
    text_bounds.size.h = 70;
    text_bounds.origin.y = (bounds.size.h / 2) - 35;
    #else
    // For square displays, use standard centering
    text_bounds.origin.x = 10;
    text_bounds.size.w = bounds.size.w - 20;
    text_bounds.size.h = 80;
    text_bounds.origin.y = (bounds.size.h / 2) - 40;
    #endif

    if (s_timeout_count >= TIMEOUT_THRESHOLD) {
      // Show message asking if app is open on phone
      graphics_draw_text(ctx, "Waiting...\nApp open?", fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                         text_bounds, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    } else {
      graphics_draw_text(ctx, "Waiting for preview...", fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                         text_bounds, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    }
  }
}

/********************************* Button Handlers ************************************/

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  timer_increment();
  update_timer_icon();
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  extern uint16_t timer_get_value();
  uint16_t timer_val = timer_get_value();
  APP_LOG(APP_LOG_LEVEL_INFO, "SELECT button clicked - capturing with timer=%u", timer_val);
  start_camera_countdown();
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  menu_window_push();
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

/********************************* Preview Window ************************************/

static void preview_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  window_set_background_color(window, GColorBlack);
  window_set_click_config_provider(s_preview_window, click_config_provider);

  // Create canvas layer for frames (accounting for action bar width on rect displays)
  GRect canvas_bounds = GRect(0, 0,
                               bounds.size.w - PBL_IF_RECT_ELSE(ACTION_BAR_WIDTH, 0),
                               bounds.size.h);
  s_canvas_layer = layer_create(canvas_bounds);
  layer_set_update_proc(s_canvas_layer, canvas_update_proc);
  layer_add_child(window_layer, s_canvas_layer);

  // Create countdown text layer as overlay
  s_countdown_layer = text_layer_create(canvas_bounds);
  text_layer_set_background_color(s_countdown_layer, GColorClear);
  text_layer_set_text_color(s_countdown_layer, TEXT_COLOR);
  text_layer_set_font(s_countdown_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  text_layer_set_text_alignment(s_countdown_layer, GTextAlignmentCenter);
  // Center text vertically
  GRect countdown_bounds = canvas_bounds;
  countdown_bounds.origin.y = (canvas_bounds.size.h / 2) - 30;
  layer_set_frame(text_layer_get_layer(s_countdown_layer), countdown_bounds);
  layer_add_child(window_layer, text_layer_get_layer(s_countdown_layer));

  // Use preallocated frame buffer from manager
  s_frame_buffer = frame_buffer_manager_get_buffer();

  if (!s_frame_buffer) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Frame buffer not available");
    return;
  }

  // Initialize message assembler with frame buffer
  message_assembler_init(s_frame_buffer);
  message_assembler_register_timeout_callback(assembler_timeout_handler);

  register_preview_data_callback(preview_data_callback);
  register_picture_taken_callback(picture_taken_callback);

  AppSettings *settings = app_get_settings();

  // Only request frames if preview is enabled
  if (preview_is_enabled()) {
    uint8_t format = (uint8_t)color_get_format();
    APP_LOG(APP_LOG_LEVEL_INFO, "preview_window_load: sending initial frame request at t=%lu", (unsigned long)time(NULL));
    send_request_next_frame(model_name_to_enum(settings->model_name), format, settings->dithering_algorithm);
    s_frame_requested = true;
    s_request_retry_timer = app_timer_register(REQUEST_TIMEOUT_MS, request_retry_handler, NULL);
  }

  // Mark canvas dirty to show waiting message
  layer_mark_dirty(s_canvas_layer);

  // Load bitmap icons
  s_camera_bitmap = gbitmap_create_with_resource(RESOURCE_ID_CAMERA);
  s_gear_bitmap = gbitmap_create_with_resource(RESOURCE_ID_GEAR);

  // Load timer icon bitmaps
  for (uint8_t i = 0; i < 5; i++) {
    s_timer_bitmaps[i] = gbitmap_create_with_resource(TIMER_RESOURCE_IDS[i]);
  }

  // Set up action bar layer with icons
  s_action_bar = action_bar_layer_create();
  uint8_t timer_index = get_timer_index();
  action_bar_layer_set_icon(s_action_bar, BUTTON_ID_UP, s_timer_bitmaps[timer_index]);
  action_bar_layer_set_icon(s_action_bar, BUTTON_ID_SELECT, s_camera_bitmap);
  action_bar_layer_set_icon(s_action_bar, BUTTON_ID_DOWN, s_gear_bitmap);
  action_bar_layer_set_click_config_provider(s_action_bar, (ClickConfigProvider)click_config_provider);
  action_bar_layer_add_to_window(s_action_bar, window);
}

static void preview_window_appear(Window *window) {
  update_timer_icon();

  // Reinitialize frame buffer in case color mode changed while menu was open
  frame_buffer_manager_init();

  // Get updated frame buffer after potential reinitialization
  s_frame_buffer = frame_buffer_manager_get_buffer();
  if (!s_frame_buffer) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "preview_window_appear: Failed to get frame buffer after reinit");
    return;
  }

  // Reinitialize message assembler with updated frame buffer
  message_assembler_init(s_frame_buffer);
  message_assembler_register_timeout_callback(assembler_timeout_handler);

  // Resume frame requests if preview is enabled
  if (preview_is_enabled() && !s_frame_requested) {
    AppSettings *settings = app_get_settings();
    uint8_t format = (uint8_t)color_get_format();
    send_request_next_frame(model_name_to_enum(settings->model_name), format, settings->dithering_algorithm);
    s_frame_requested = true;
    s_request_retry_timer = app_timer_register(REQUEST_TIMEOUT_MS, request_retry_handler, NULL);
  }
}

static void preview_window_disappear(Window *window) {
  // Stop requesting frames when window is not visible (e.g., menu is open)
  if (s_request_retry_timer) {
    app_timer_cancel(s_request_retry_timer);
    s_request_retry_timer = NULL;
  }
  s_frame_requested = false;
}

static void preview_window_unload(Window *window) {
  if (s_request_retry_timer) {
    app_timer_cancel(s_request_retry_timer);
    s_request_retry_timer = NULL;
  }
  if (s_countdown_timer) {
    app_timer_cancel(s_countdown_timer);
    s_countdown_timer = NULL;
  }
  if (s_countdown_clear_timer) {
    app_timer_cancel(s_countdown_clear_timer);
    s_countdown_clear_timer = NULL;
  }
  s_frame_requested = false;
  s_frame_received = false;
  s_countdown_remaining = 0;
  s_expecting_picture_taken = false;
  message_assembler_deinit();
  layer_destroy(s_canvas_layer);
  if (s_countdown_layer) {
    text_layer_destroy(s_countdown_layer);
    s_countdown_layer = NULL;
  }
  if (s_action_bar) {
    action_bar_layer_destroy(s_action_bar);
    s_action_bar = NULL;
  }
  for (uint8_t i = 0; i < 5; i++) {
    if (s_timer_bitmaps[i]) {
      gbitmap_destroy(s_timer_bitmaps[i]);
      s_timer_bitmaps[i] = NULL;
    }
  }
  if (s_camera_bitmap) {
    gbitmap_destroy(s_camera_bitmap);
    s_camera_bitmap = NULL;
  }
  if (s_gear_bitmap) {
    gbitmap_destroy(s_gear_bitmap);
    s_gear_bitmap = NULL;
  }
  s_frame_buffer = NULL;
}

void preview_window_push() {
  if (!s_preview_window) {
    s_preview_window = window_create();
    window_set_window_handlers(s_preview_window, (WindowHandlers) {
      .load = preview_window_load,
      .appear = preview_window_appear,
      .disappear = preview_window_disappear,
      .unload = preview_window_unload,
    });
  }
  window_stack_push(s_preview_window, true);
}

void preview_toggle() {
  uint8_t current = preview_is_enabled();
  preview_set_enabled(!current);

  // If preview window is loaded, update its display
  if (s_preview_window && window_stack_contains_window(s_preview_window)) {
    // Mark canvas dirty to refresh with new state
    if (s_canvas_layer) {
      layer_mark_dirty(s_canvas_layer);
    }
  }
}
