// SPDX-License-Identifier: MIT
#pragma once

#include <chrono>
#include <memory>
#include <napi.h>
#include <vector>

// Forward declare Orpheus types to avoid including full headers
namespace orpheus::core {
class SessionGraph;
}

/**
 * N-API wrapper for Orpheus SessionGraph
 *
 * Provides Node.js bindings for session management, including:
 * - Loading session JSON
 * - Rendering audio
 * - Querying session state
 * - Event callbacks
 */
class SessionWrapper : public Napi::ObjectWrap<SessionWrapper> {
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  SessionWrapper(const Napi::CallbackInfo& info);
  ~SessionWrapper();

private:
  // Session management
  Napi::Value LoadSession(const Napi::CallbackInfo& info);
  Napi::Value GetSessionInfo(const Napi::CallbackInfo& info);

  // Rendering
  Napi::Value RenderClick(const Napi::CallbackInfo& info);

  // Transport state
  Napi::Value GetTempo(const Napi::CallbackInfo& info);
  Napi::Value SetTempo(const Napi::CallbackInfo& info);

  // Event callbacks
  Napi::Value Subscribe(const Napi::CallbackInfo& info);
  Napi::Value Unsubscribe(const Napi::CallbackInfo& info);

  // Internal event emission
  void EmitEvent(const Napi::Env& env, const Napi::Object& event);
  void EmitSessionChanged();
  void EmitHeartbeat();

  std::unique_ptr<orpheus::core::SessionGraph> session_;
  std::string session_path_;

  // Event callbacks
  struct CallbackEntry {
    uint32_t id;
    Napi::FunctionReference callback;
  };
  std::vector<CallbackEntry> callbacks_;
  uint32_t next_callback_id_ = 0;
  uint32_t sequence_id_ = 0;
  std::chrono::steady_clock::time_point start_time_;
};
