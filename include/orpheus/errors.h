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

typedef void (*orpheus_log_callback)(orpheus_log_level level,
                                      const char *message,
                                      void *user_data);

ORPHEUS_API const char *orpheus_status_to_string(orpheus_status status);
ORPHEUS_API void orpheus_set_logger(orpheus_log_callback callback,
                                    void *user_data);

typedef void (*orpheus_telemetry_callback)(const char *event_name,
                                            const char *json_payload,
                                            void *user_data);

ORPHEUS_API void orpheus_set_telemetry_callback(
    orpheus_telemetry_callback callback, void *user_data);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include <string_view>

namespace orpheus {

ORPHEUS_API void Log(orpheus_log_level level, std::string_view message);
ORPHEUS_API void EmitTelemetry(std::string_view event_name,
                               std::string_view json_payload);

}  // namespace orpheus

#endif  // __cplusplus
