// SPDX-License-Identifier: MIT
#pragma once

#include "orpheus/errors.h"
#include "treefall/export.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef orpheus_status treefall_status;
typedef orpheus_log_level treefall_log_level;
typedef orpheus_log_callback treefall_log_callback;
typedef orpheus_telemetry_callback treefall_telemetry_callback;

#define TREEFALL_STATUS_OK ORPHEUS_STATUS_OK
#define TREEFALL_STATUS_INVALID_ARGUMENT ORPHEUS_STATUS_INVALID_ARGUMENT
#define TREEFALL_STATUS_NOT_FOUND ORPHEUS_STATUS_NOT_FOUND
#define TREEFALL_STATUS_OUT_OF_MEMORY ORPHEUS_STATUS_OUT_OF_MEMORY
#define TREEFALL_STATUS_INTERNAL_ERROR ORPHEUS_STATUS_INTERNAL_ERROR
#define TREEFALL_STATUS_NOT_IMPLEMENTED ORPHEUS_STATUS_NOT_IMPLEMENTED
#define TREEFALL_STATUS_IO_ERROR ORPHEUS_STATUS_IO_ERROR

#define TREEFALL_LOG_LEVEL_DEBUG ORPHEUS_LOG_LEVEL_DEBUG
#define TREEFALL_LOG_LEVEL_INFO ORPHEUS_LOG_LEVEL_INFO
#define TREEFALL_LOG_LEVEL_WARN ORPHEUS_LOG_LEVEL_WARN
#define TREEFALL_LOG_LEVEL_ERROR ORPHEUS_LOG_LEVEL_ERROR
TREEFALL_API const char* treefall_status_to_string(treefall_status status);
TREEFALL_API void treefall_set_logger(treefall_log_callback callback, void* user_data);
TREEFALL_API void treefall_set_telemetry_callback(treefall_telemetry_callback callback,
                                                  void* user_data);

#ifdef __cplusplus
}
#endif
