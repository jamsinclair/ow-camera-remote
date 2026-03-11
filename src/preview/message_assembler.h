#pragma once

#include "message_header.h"
#include <pebble.h>

// Callback invoked when a complete frame is ready (single or final chunk of multi-message)
// Parameters:
//   format - Message format (1-bit B&W or 4-bit color)
//   bitmap - GBitmap with decompressed data ready to render
//            For 4-bit: palette already set
typedef void (*AssemblerCompleteCallback)(MessageFormat format, GBitmap *bitmap);

// Callback invoked when multi-message assembly times out
typedef void (*AssemblerTimeoutCallback)(void);

// Initialize the assembler with a target bitmap to decompress into
// The bitmap should already be allocated to full size
void message_assembler_init(GBitmap *target_bitmap);

// Register a callback to be invoked if multi-message assembly times out
void message_assembler_register_timeout_callback(AssemblerTimeoutCallback callback);

// Clean up assembler resources
void message_assembler_deinit(void);

// Reset assembler state (e.g., on timeout or error)
void message_assembler_reset(void);

// Process a message (first or continuation)
// Returns true if message was processed successfully
// For single-message or final multi-message chunk: invokes callback
// For intermediate chunks: returns true without invoking callback yet
bool message_assembler_process(
  const uint8_t *msg_data,
  size_t msg_length,
  AssemblerCompleteCallback callback
);
