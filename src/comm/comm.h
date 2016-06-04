#define KEY_APP_STATUS_CHECK 0
#define KEY_APP_STATUS_OKAY 200
#define KEY_CAPTURE 1
// Time to wait for a response from the companion app
#define APP_MESSAGE_TIMEOUT 1500

void send_int_app_message_with_callback();
void init_comm();
void deinit_comm();
