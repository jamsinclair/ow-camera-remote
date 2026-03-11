#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Universal message preamble (all messages, both directions)
// Byte 0: Flags / control byte (message-type-specific)
// Bytes 1-4: uint32_t timestamp (unix epoch, little-endian)
// Bytes 5+: Payload (message-type-specific)

// Header byte format for first message
// Bit 0: Always 0 (distinguishes from continuation)
// Bits 1-2: Reserved (0)
// Bits 3-5: Format (0 = B&W, 3 = 4-bit color)
// Bit 6: Multi-message flag (1 = multiple messages, 0 = single message)
// Bit 7: Reserved

// Header byte format for continuation message
// Bit 0: Always 1 (continuation marker)
// Bits 1-2: Reserved (0)
// Bits 3-5: Chunk number (0-7)
// Bit 6: Last chunk flag
// Bit 7: Reserved

// Format enum matching MESSAGE_FORMAT.md
typedef enum {
  MESSAGE_FORMAT_1BIT_BW = 0,
  MESSAGE_FORMAT_4BIT_COLOR = 3,
} MessageFormat;

// Parsed first message header
typedef struct {
  uint32_t timestamp;          // Timestamp from bytes 1-4 (unix epoch)
  MessageFormat format;        // Format type from bits 3-5 of byte 0
  bool multi_message;          // Multi-message flag from bit 6 of byte 0
  const uint8_t* payload;      // Pointer to data after 5-byte preamble
  size_t payload_size;         // Size of payload data
} FirstMessageHeader;

// Parsed continuation message header
typedef struct {
  uint32_t timestamp;          // Timestamp from bytes 1-4 (unix epoch)
  uint8_t chunk_number;        // Chunk number from bits 3-5 of byte 0 (0-7)
  bool is_last_chunk;          // Last chunk flag from bit 6 of byte 0
  const uint8_t* payload;      // Pointer to data after 5-byte preamble
  size_t payload_size;         // Size of payload data
} ContinuationHeader;

// Parse the header byte of a first message
// Returns true on success, false if data is too small
bool parse_first_message_header(const uint8_t* data, size_t length,
                                FirstMessageHeader* header_out);

// Parse the header byte of a continuation message
// Returns true on success, false if not a valid continuation header
bool parse_continuation_header(const uint8_t* data, size_t length,
                               ContinuationHeader* header_out);
