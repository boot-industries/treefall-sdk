// SPDX-License-Identifier: MIT
#pragma once

#include "orpheus/export.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum orpheus_status {
  ORPHEUS_STATUS_OK = 0,
  ORPHEUS_STATUS_INVALID_ARGUMENT = 1,
  ORPHEUS_STATUS_NOT_FOUND = 2,
  ORPHEUS_STATUS_OUT_OF_MEMORY = 3,
  ORPHEUS_STATUS_INTERNAL_ERROR = 4,
  ORPHEUS_STATUS_NOT_IMPLEMENTED = 5,
  ORPHEUS_STATUS_IO_ERROR = 6
} orpheus_status;

typedef enum orpheus_log_level {
  ORPHEUS_LOG_LEVEL_DEBUG = 0,
  ORPHEUS_LOG_LEVEL_INFO = 1,
  ORPHEUS_LOG_LEVEL_WARN = 2,
  ORPHEUS_LOG_LEVEL_ERROR = 3
} orpheus_log_level;

typedef void (*orpheus_log_callback)(orpheus_log_level level, const char* message, void* user_data);

ORPHEUS_API const char* orpheus_status_to_string(orpheus_status status);
/**
 * Configure the logger in the ABI shared library through which this setter is
 * invoked, not process-wide. Session, clipgrid and render DSOs keep independent
 * registrations. The Orpheus and Treefall spellings share one registration
 * within the same DSO; either spelling replaces it, and a null callback clears
 * it. Static builds keep callback state per final link unit.
 */
ORPHEUS_API void orpheus_set_logger(orpheus_log_callback callback, void* user_data);

typedef void (*orpheus_telemetry_callback)(const char* event_name, const char* json_payload,
                                           void* user_data);

/**
 * Configure telemetry with the same DSO-local scope and same-DSO alias-sharing
 * rules as orpheus_set_logger. This callback registration is independent of
 * the shared session-handle registry.
 */
ORPHEUS_API void orpheus_set_telemetry_callback(orpheus_telemetry_callback callback,
                                                void* user_data);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include <stdexcept>
#include <string>
#include <string_view>

namespace orpheus {

enum class SessionGraphError : uint8_t {
  OK = 0,
  InvalidHandle = 1,
  InvalidParameter = 2,
  NotReady = 3,
  NotSupported = 4,
  NotInitialized = 5,
  InvalidClipTrimPoints = 18,
  InvalidFadeDuration = 19,
  ClipNotRegistered = 20,
  InternalError = 255
};

template <typename T> struct Result {
  T value;
  SessionGraphError error = SessionGraphError::OK;
  std::string errorMessage;

  bool isOk() const {
    return error == SessionGraphError::OK;
  }

  T& operator*() {
    if (!isOk()) {
      throw std::runtime_error("Result error: " + errorMessage);
    }
    return value;
  }

  const T& operator*() const {
    if (!isOk()) {
      throw std::runtime_error("Result error: " + errorMessage);
    }
    return value;
  }
};

ORPHEUS_API void Log(orpheus_log_level level, std::string_view message);
ORPHEUS_API void EmitTelemetry(std::string_view event_name, std::string_view json_payload);

} // namespace orpheus

#endif // __cplusplus
