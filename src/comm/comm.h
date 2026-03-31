#define KEY_CAPTURE 1
#define KEY_PICTURE_TAKEN 2
#define KEY_PREVIEW_DATA 3
#define KEY_REQUEST_NEXT_FRAME 4
#define KEY_CAPTURE_ACK 5
#define KEY_TOGGLE_CAMERA 6
#define KEY_REQUEST_NEXT_CHUNK 7

// Time to wait for a response from the companion app
#define APP_MESSAGE_TIMEOUT 4000

// Staleness threshold in seconds - messages older than this are dropped
#define STALE_MSG_THRESHOLD_S 10

// Preview data callback type
typedef void(PreviewDataCallback)(uint8_t *data, size_t length);
// Send result callback type (called when message send result is known)
typedef void(SendResultCallback)(bool success);

void send_int_app_message_with_callback(int key, int message, void *timeout_handler);
void send_int_app_message_with_result_callback(int key, int message, SendResultCallback *result_callback, void *timeout_handler);
void send_capture_with_ack(int timer_seconds, SendResultCallback *ack_callback);
bool comm_capture_in_progress();
bool comm_outbox_pending();
void register_picture_taken_callback();
void send_request_next_frame(uint8_t model_enum, uint8_t format, uint8_t dithering_algorithm);
void send_request_next_chunk(uint8_t chunk_number, uint8_t format);
void register_preview_data_callback(PreviewDataCallback *callback);
uint8_t model_name_to_enum(const char *model_name);
void init_comm();
void deinit_comm();
