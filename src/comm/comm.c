#include <pebble.h>
#include "comm/comm.h"
#include "windows/alert_window.h"

static AppTimer *response_wait_timer;

static char *translate_error(AppMessageResult result) {
  switch (result) {
    case APP_MSG_OK: return "APP_MSG_OK";
    case APP_MSG_SEND_TIMEOUT: return "APP_MSG_SEND_TIMEOUT";
    case APP_MSG_SEND_REJECTED: return "APP_MSG_SEND_REJECTED";
    case APP_MSG_NOT_CONNECTED: return "APP_MSG_NOT_CONNECTED";
    case APP_MSG_APP_NOT_RUNNING: return "APP_MSG_APP_NOT_RUNNING";
    case APP_MSG_INVALID_ARGS: return "APP_MSG_INVALID_ARGS";
    case APP_MSG_BUSY: return "APP_MSG_BUSY";
    case APP_MSG_BUFFER_OVERFLOW: return "APP_MSG_BUFFER_OVERFLOW";
    case APP_MSG_ALREADY_RELEASED: return "APP_MSG_ALREADY_RELEASED";
    case APP_MSG_CALLBACK_ALREADY_REGISTERED: return "APP_MSG_CALLBACK_ALREADY_REGISTERED";
    case APP_MSG_CALLBACK_NOT_REGISTERED: return "APP_MSG_CALLBACK_NOT_REGISTERED";
    case APP_MSG_OUT_OF_MEMORY: return "APP_MSG_OUT_OF_MEMORY";
    case APP_MSG_CLOSED: return "APP_MSG_CLOSED";
    case APP_MSG_INTERNAL_ERROR: return "APP_MSG_INTERNAL_ERROR";
    default: return "UNKNOWN ERROR";
  }
}

void send_int_app_message_with_callback(int key, int message, void *timeout_handler) {
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);

  dict_write_int(iter, key, &message, sizeof(int), true);

  APP_LOG(APP_LOG_LEVEL_INFO, "Sending app message");
  AppMessageResult send_result_code = app_message_outbox_send();

  if (send_result_code == APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Inital send okay");

    response_wait_timer = app_timer_register(APP_MESSAGE_TIMEOUT, timeout_handler, NULL);
  } else {
    APP_LOG(APP_LOG_LEVEL_INFO, "Failed initial send with: %s\n", translate_error(send_result_code));

    AppTimerCallback handler = timeout_handler;
    // Call timeout_handler
    handler(NULL);
  }
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  Tuple *t = dict_read_first(iterator);

  while(t != NULL) {
    switch(t->key) {
      case KEY_APP_STATUS_OKAY:
        APP_LOG(APP_LOG_LEVEL_INFO, "Companion App responded, cancel timer");
        app_timer_cancel(response_wait_timer);
        // Handle Companion Open here....
        break;
      default:
        APP_LOG(APP_LOG_LEVEL_INFO, "Unknown key: %d", (int)t->key);
        break;
    }

    t = dict_read_next(iterator);
  }
}

static void outbox_failed_handler(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed: %s", translate_error(reason));
}

static void outbox_sent_handler(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");

  app_timer_cancel(response_wait_timer);
}

void init_comm() {
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_outbox_sent(outbox_sent_handler);
  app_message_register_outbox_failed(outbox_failed_handler);

  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
}

void deinit_comm() {
  app_message_deregister_callbacks();
}
