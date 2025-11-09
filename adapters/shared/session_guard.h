// SPDX-License-Identifier: MIT
#pragma once

#include "orpheus/abi.h"

namespace orpheus::adapters {

/// RAII guard for automatic session cleanup
/// Ensures orpheus_session_handle is destroyed when guard goes out of scope
struct SessionGuard {
  const orpheus_session_api_v1* api = nullptr;
  orpheus_session_handle handle = nullptr;

  /// Default constructor
  SessionGuard() = default;

  /// Construct with API and handle
  SessionGuard(const orpheus_session_api_v1* api_, orpheus_session_handle handle_)
      : api(api_), handle(handle_) {}

  /// Destructor: automatically destroys session if valid
  ~SessionGuard() {
    if (api && handle) {
      api->destroy(handle);
    }
  }

  /// Non-copyable (unique ownership)
  SessionGuard(const SessionGuard&) = delete;
  SessionGuard& operator=(const SessionGuard&) = delete;

  /// Movable (transfer ownership)
  SessionGuard(SessionGuard&& other) noexcept : api(other.api), handle(other.handle) {
    other.api = nullptr;
    other.handle = nullptr;
  }

  SessionGuard& operator=(SessionGuard&& other) noexcept {
    if (this != &other) {
      // Destroy current handle if valid
      if (api && handle) {
        api->destroy(handle);
      }

      // Transfer ownership from other
      api = other.api;
      handle = other.handle;
      other.api = nullptr;
      other.handle = nullptr;
    }
    return *this;
  }
};

} // namespace orpheus::adapters
