#include "message_header.h"
#include <pebble.h>

bool parse_first_message_header(const uint8_t* data, size_t length,
                                FirstMessageHeader* header_out) {
  if (!data || !header_out) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "message_header: null pointer");
    return false;
  }

  if (length < 5) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "message_header: data too small (need 5+ bytes for preamble)");
    return false;
  }

  uint8_t header_byte = data[0];

  // Verify bit 0 is always 0 (distinguishes from continuation)
  if (header_byte & 0x01) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "message_header: invalid first message (bit 0 should be 0)");
    return false;
  }

  // Extract timestamp from bytes 1-4 (little-endian)
  uint32_t timestamp = data[1] | ((uint32_t)data[2] << 8) | ((uint32_t)data[3] << 16) | ((uint32_t)data[4] << 24);

  // Extract format from bits 3-5
  uint8_t format_bits = (header_byte >> 3) & 0x07;

  // Extract multi-message flag from bit 6
  bool multi_message = (header_byte >> 6) & 0x01;

  // Map format bits to enum
  MessageFormat format;
  switch (format_bits) {
    case 0:
      format = MESSAGE_FORMAT_1BIT_BW;
      break;
    case 3:
      format = MESSAGE_FORMAT_4BIT_COLOR;
      break;
    default:
      APP_LOG(APP_LOG_LEVEL_ERROR, "message_header: invalid format %u", format_bits);
      return false;
  }

  // Fill output structure
  header_out->timestamp = timestamp;
  header_out->format = format;
  header_out->multi_message = multi_message;
  header_out->payload = data + 5;
  header_out->payload_size = length - 5;

  return true;
}

bool parse_continuation_header(const uint8_t* data, size_t length,
                               ContinuationHeader* header_out) {
  if (!data || !header_out) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "continuation_header: null pointer");
    return false;
  }

  if (length < 5) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "continuation_header: data too small (need 5+ bytes for preamble)");
    return false;
  }

  uint8_t header_byte = data[0];

  // Check bit 0 - must be 1 for continuation
  if (!(header_byte & 0x01)) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "continuation_header: not a continuation (bit 0 not set)");
    return false;
  }

  // Extract timestamp from bytes 1-4 (little-endian)
  uint32_t timestamp = data[1] | ((uint32_t)data[2] << 8) | ((uint32_t)data[3] << 16) | ((uint32_t)data[4] << 24);

  // Extract chunk number from bits 3-5
  uint8_t chunk_number = (header_byte >> 3) & 0x07;

  // Extract last chunk flag from bit 6
  bool is_last_chunk = (header_byte >> 6) & 0x01;

  // Fill output structure
  header_out->timestamp = timestamp;
  header_out->chunk_number = chunk_number;
  header_out->is_last_chunk = is_last_chunk;
  header_out->payload = data + 5;
  header_out->payload_size = length - 5;

  return true;
}
