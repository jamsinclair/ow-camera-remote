#define KEY_APP_STATUS_CHECK 0
#define KEY_CAPTURE 1
#define KEY_PICTURE_TAKEN 2
// Time to wait for a response from the companion app
#define APP_MESSAGE_TIMEOUT 4000

void send_int_app_message_with_callback();
void register_picture_taken_callback();
void init_comm();
void deinit_comm();
