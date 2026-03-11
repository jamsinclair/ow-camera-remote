#include "message_assembler.h"
#include "frame_buffer_manager.h"
#include "bw_format.h"
#include "../lib/tinflate/tinflate.h"
#include <pebble.h>
#include <string.h>
#include <time.h>

// State for multi-message assembly
typedef struct {
  GBitmap *target_bitmap;
  size_t decompressed_offset;  // Current write position in GBitmap
  uint8_t palette[16];         // Palette for 4-bit frames
  MessageFormat format;
  uint8_t expected_chunk;      // Next chunk number expected
  bool is_assembling;
  AppTimer *timeout_timer;     // Timer for assembly timeout
} AssemblerState;

static AssemblerState s_state = {
  .target_bitmap = NULL,
  .decompressed_offset = 0,
  .format = MESSAGE_FORMAT_1BIT_BW,
  .expected_chunk = 0,
  .is_assembling = false,
  .timeout_timer = NULL,
};

// Timeout callback for multi-message assembly
static AssemblerTimeoutCallback s_timeout_callback = NULL;

// Timeout handler when multi-message assembly takes too long
static void assembly_timeout_handler(void *data) {
  s_state.timeout_timer = NULL;
  APP_LOG(APP_LOG_LEVEL_WARNING, "message_assembler: multi-message assembly timed out");

  // Reset state for next frame
  message_assembler_reset();

  // Invoke callback if registered
  if (s_timeout_callback) {
    s_timeout_callback();
  }
}

void message_assembler_init(GBitmap *target_bitmap) {
  s_state.target_bitmap = target_bitmap;
  s_state.decompressed_offset = 0;
  s_state.expected_chunk = 0;
  s_state.is_assembling = false;
  s_state.timeout_timer = NULL;
  memset(s_state.palette, 0, sizeof(s_state.palette));
}

void message_assembler_deinit(void) {
  if (s_state.timeout_timer) {
    app_timer_cancel(s_state.timeout_timer);
    s_state.timeout_timer = NULL;
  }
  s_state.target_bitmap = NULL;
  s_state.is_assembling = false;
}

void message_assembler_reset(void) {
  if (s_state.timeout_timer) {
    app_timer_cancel(s_state.timeout_timer);
    s_state.timeout_timer = NULL;
  }
  s_state.decompressed_offset = 0;
  s_state.expected_chunk = 0;
  s_state.is_assembling = false;
  memset(s_state.palette, 0, sizeof(s_state.palette));
}

void message_assembler_register_timeout_callback(AssemblerTimeoutCallback callback) {
  s_timeout_callback = callback;
}

// Decompress a chunk directly to the target bitmap at current offset
static bool decompress_chunk_to_bitmap(
  const uint8_t *compressed_data,
  size_t compressed_size
) {
  if (!s_state.target_bitmap) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "message_assembler: target_bitmap is NULL");
    return false;
  }

  uint8_t *bitmap_data = gbitmap_get_data(s_state.target_bitmap);
  if (!bitmap_data) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "message_assembler: failed to get bitmap data");
    return false;
  }

  // Get bitmap dimensions and bytes per row using Pebble SDK functions
  GRect bounds = gbitmap_get_bounds(s_state.target_bitmap);
  uint16_t height = bounds.size.h;
  uint16_t bytes_per_row = gbitmap_get_bytes_per_row(s_state.target_bitmap);
  size_t bitmap_size = bytes_per_row * height;

  if (s_state.decompressed_offset >= bitmap_size) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "message_assembler: offset overflow (offset=%zu, size=%zu)",
            s_state.decompressed_offset, bitmap_size);
    return false;
  }

  // Calculate available space for decompression
  unsigned int available_space = bitmap_size - s_state.decompressed_offset;

  // Decompress at current offset
  unsigned int chunk_size = available_space;
  int result = tinflate_uncompress(
    bitmap_data + s_state.decompressed_offset,
    &chunk_size,
    compressed_data,
    compressed_size
  );

  if (result != TINF_OK) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "message_assembler: decompression failed (result=%d)", result);
    return false;
  }

  s_state.decompressed_offset += chunk_size;
  return true;
}

// Handle single-message 1-bit B&W (no compression)
static bool handle_1bit_single_message(
  const FirstMessageHeader *first_header,
  AssemblerCompleteCallback callback
) {
  if (!s_state.target_bitmap || !callback) {
    return false;
  }

  // For 1-bit, payload is raw uncompressed pixel data as a continuous bitstream
  uint8_t *bitmap_data = gbitmap_get_data(s_state.target_bitmap);
  if (!bitmap_data) {
    return false;
  }

  GRect bounds = gbitmap_get_bounds(s_state.target_bitmap);
  uint16_t width = bounds.size.w;
  uint16_t height = bounds.size.h;
  uint16_t bytes_per_row = gbitmap_get_bytes_per_row(s_state.target_bitmap);

  // Clear bitmap data first
  memset(bitmap_data, 0, bytes_per_row * height);

  size_t total_pixels = width * height;

  // Unpack bits from packed payload and pack into bitmap rows
  uint8_t unpacked_pixels[8];
  size_t pixel_idx = 0;

  for (size_t byte_idx = 0; byte_idx < first_header->payload_size && pixel_idx < total_pixels; byte_idx++) {
    bw_format_unpack_byte(first_header->payload[byte_idx], unpacked_pixels);

    // Process the 8 unpacked pixels
    for (int i = 0; i < 8 && pixel_idx < total_pixels; i++, pixel_idx++) {
      uint8_t pixel = unpacked_pixels[i];

      // Calculate row and column in image
      uint16_t row = pixel_idx / width;
      uint16_t col = pixel_idx % width;

      // Pack into bitmap row (LSB first layout for Pebble 1-bit bitmaps)
      uint16_t bitmap_byte_offset = col / 8;
      uint8_t bitmap_bit_idx = col % 8;
      uint8_t *bitmap_byte = bitmap_data + row * bytes_per_row + bitmap_byte_offset;

      if (pixel) {
        *bitmap_byte |= (1 << bitmap_bit_idx);
      }
    }
  }

  callback(first_header->format, s_state.target_bitmap);
  return true;
}

// Handle single-message 4-bit color (compressed)
static bool handle_4bit_single_message(
  const FirstMessageHeader *first_header,
  AssemblerCompleteCallback callback
) {
  if (!s_state.target_bitmap || !callback) {
    return false;
  }

  // First 16 bytes of payload are palette
  if (first_header->payload_size < 16) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "message_assembler: 4-bit payload too small for palette");
    return false;
  }

  // Copy palette
  memcpy(s_state.palette, first_header->payload, 16);

  // Set palette on bitmap
  GColor *palette_colors = gbitmap_get_palette(s_state.target_bitmap);
  if (palette_colors) {
    for (int i = 0; i < 16; i++) {
      palette_colors[i] = (GColor){.argb = s_state.palette[i]};
    }
  }

  // Decompress pixel data (starts after 16-byte palette)
  const uint8_t *compressed_data = first_header->payload + 16;
  size_t compressed_size = first_header->payload_size - 16;

  s_state.decompressed_offset = 0;
  if (!decompress_chunk_to_bitmap(compressed_data, compressed_size)) {
    return false;
  }

  callback(first_header->format, s_state.target_bitmap);
  return true;
}

// Handle first message of multi-message sequence
static bool handle_multi_message_first(
  const FirstMessageHeader *first_header
) {
  s_state.format = first_header->format;
  s_state.expected_chunk = 0;
  s_state.decompressed_offset = 0;
  s_state.is_assembling = true;

  // Start timeout timer for assembly (10 second timeout)
  #define ASSEMBLY_TIMEOUT_MS 10000
  if (s_state.timeout_timer) {
    app_timer_cancel(s_state.timeout_timer);
  }
  s_state.timeout_timer = app_timer_register(ASSEMBLY_TIMEOUT_MS, assembly_timeout_handler, NULL);

  if (first_header->format == MESSAGE_FORMAT_4BIT_COLOR) {
    // First 16 bytes of payload are palette
    if (first_header->payload_size < 16) {
      APP_LOG(APP_LOG_LEVEL_ERROR, "message_assembler: multi 4-bit payload too small for palette");
      return false;
    }

    // Copy palette
    memcpy(s_state.palette, first_header->payload, 16);

    // Set palette on bitmap
    GColor *palette_colors = gbitmap_get_palette(s_state.target_bitmap);
    if (palette_colors) {
      for (int i = 0; i < 16; i++) {
        palette_colors[i] = (GColor){.argb = s_state.palette[i]};
      }
    }

    // Decompress first chunk (starts after 16-byte palette)
    const uint8_t *compressed_data = first_header->payload + 16;
    size_t compressed_size = first_header->payload_size - 16;

    if (!decompress_chunk_to_bitmap(compressed_data, compressed_size)) {
      return false;
    }

    return true;
  }

  // For 1-bit multi-message (shouldn't happen but handle gracefully)
  APP_LOG(APP_LOG_LEVEL_WARNING, "message_assembler: 1-bit multi-message not typical");
  return true;
}

// Handle continuation message
static bool handle_continuation_message(
  const ContinuationHeader *cont_header,
  AssemblerCompleteCallback callback
) {
  if (!s_state.is_assembling) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "message_assembler: continuation without first message");
    return false;
  }

  // Validate chunk sequence
  if (cont_header->chunk_number != s_state.expected_chunk) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "message_assembler: chunk out of sequence (expected=%u, got=%u)",
            s_state.expected_chunk, cont_header->chunk_number);
    return false;
  }

  // Decompress this chunk
  if (!decompress_chunk_to_bitmap(cont_header->payload, cont_header->payload_size)) {
    return false;
  }

  s_state.expected_chunk++;

  // If this is the last chunk, invoke callback and cleanup
  if (cont_header->is_last_chunk) {
    // Cancel timeout timer since assembly completed
    if (s_state.timeout_timer) {
      app_timer_cancel(s_state.timeout_timer);
      s_state.timeout_timer = NULL;
    }

    callback(s_state.format, s_state.target_bitmap);
    message_assembler_reset();
  }

  return true;
}

bool message_assembler_process(
  const uint8_t *msg_data,
  size_t msg_length,
  AssemblerCompleteCallback callback
) {
  if (!msg_data || msg_length == 0 || !callback) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "message_assembler: invalid arguments");
    return false;
  }

  if (!s_state.target_bitmap) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "message_assembler: target_bitmap not initialized");
    return false;
  }

  // Try to parse as first message
  FirstMessageHeader first_header;
  if (parse_first_message_header(msg_data, msg_length, &first_header)) {
    // Validate format matches allocated buffer
    FrameFormat expected_format = frame_buffer_manager_get_format();
    if ((expected_format == FRAME_FORMAT_1BIT_BW && first_header.format != MESSAGE_FORMAT_1BIT_BW) ||
        (expected_format == FRAME_FORMAT_4BIT_COLOR && first_header.format != MESSAGE_FORMAT_4BIT_COLOR)) {
      APP_LOG(APP_LOG_LEVEL_ERROR, "message_assembler: format mismatch (expected=%d, received=%d)",
              expected_format, first_header.format);
      return false;
    }

    if (first_header.multi_message) {
      // Multi-message sequence - start accumulation
      return handle_multi_message_first(&first_header);
    } else {
      // Single message - process immediately
      if (first_header.format == MESSAGE_FORMAT_1BIT_BW) {
        return handle_1bit_single_message(&first_header, callback);
      } else if (first_header.format == MESSAGE_FORMAT_4BIT_COLOR) {
        return handle_4bit_single_message(&first_header, callback);
      }
    }
    return true;
  }

  // Try to parse as continuation message
  ContinuationHeader cont_header;
  if (parse_continuation_header(msg_data, msg_length, &cont_header)) {
    return handle_continuation_message(&cont_header, callback);
  }

  APP_LOG(APP_LOG_LEVEL_ERROR, "message_assembler: unable to parse as first or continuation");
  return false;
}
