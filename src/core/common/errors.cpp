// SPDX-License-Identifier: MIT
#include "orpheus/errors.h"

#include <atomic>
#include <string>
#include <string_view>

namespace {

template <typename Callback>
struct CallbackState {
  std::atomic<Callback> callback{nullptr};
  std::atomic<void *> user_data{nullptr};
};

CallbackState<orpheus_log_callback> &LoggerState() {
  static CallbackState<orpheus_log_callback> state;
  return state;
}

CallbackState<orpheus_telemetry_callback> &TelemetryState() {
  static CallbackState<orpheus_telemetry_callback> state;
  return state;
}

}  // namespace

extern "C" {

const char *orpheus_status_to_string(orpheus_status status) {
  switch (status) {
    case ORPHEUS_STATUS_OK:
      return "ok";
    case ORPHEUS_STATUS_INVALID_ARGUMENT:
      return "invalid argument";
    case ORPHEUS_STATUS_NOT_FOUND:
      return "not found";
    case ORPHEUS_STATUS_OUT_OF_MEMORY:
      return "out of memory";
    case ORPHEUS_STATUS_INTERNAL_ERROR:
      return "internal error";
    case ORPHEUS_STATUS_NOT_IMPLEMENTED:
      return "not implemented";
    case ORPHEUS_STATUS_IO_ERROR:
      return "io error";
  }
  return "unknown status";
}

void orpheus_set_logger(orpheus_log_callback callback, void *user_data) {
  auto &state = LoggerState();
  state.user_data.store(callback ? user_data : nullptr,
                        std::memory_order_relaxed);
  state.callback.store(callback, std::memory_order_release);
}

void orpheus_set_telemetry_callback(orpheus_telemetry_callback callback,
                                    void *user_data) {
  auto &state = TelemetryState();
  state.user_data.store(callback ? user_data : nullptr,
                        std::memory_order_relaxed);
  state.callback.store(callback, std::memory_order_release);
}

}  // extern "C"

namespace orpheus {

void Log(orpheus_log_level level, std::string_view message) {
  auto &state = LoggerState();
  const auto callback = state.callback.load(std::memory_order_acquire);
  if (callback == nullptr) {
    return;
  }
  const auto user_data = state.user_data.load(std::memory_order_acquire);
  std::string buffer(message);
  callback(level, buffer.c_str(), user_data);
}

void EmitTelemetry(std::string_view event_name,
                   std::string_view json_payload) {
  auto &state = TelemetryState();
  const auto callback = state.callback.load(std::memory_order_acquire);
  if (callback == nullptr) {
    return;
  }
  const auto user_data = state.user_data.load(std::memory_order_acquire);
  const std::string event_copy(event_name);
  const std::string payload_copy(json_payload);
  callback(event_copy.c_str(), payload_copy.c_str(), user_data);
}

}  // namespace orpheus
