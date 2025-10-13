// SPDX-License-Identifier: MIT
#include "session_wrapper.h"

// Include Orpheus SDK headers
#include "core/session/json_io.h"
#include "core/session/session_graph.h"
#include "orpheus/abi.h"
#include "orpheus/errors.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace session_json = orpheus::core::session_json;
using orpheus::core::SessionGraph;

Napi::Object SessionWrapper::Init(Napi::Env env, Napi::Object exports) {
  Napi::Function func = DefineClass(env, "Session", {
    InstanceMethod("loadSession", &SessionWrapper::LoadSession),
    InstanceMethod("getSessionInfo", &SessionWrapper::GetSessionInfo),
    InstanceMethod("renderClick", &SessionWrapper::RenderClick),
    InstanceMethod("getTempo", &SessionWrapper::GetTempo),
    InstanceMethod("setTempo", &SessionWrapper::SetTempo),
    InstanceMethod("subscribe", &SessionWrapper::Subscribe),
    InstanceMethod("unsubscribe", &SessionWrapper::Unsubscribe),
  });

  exports.Set("Session", func);
  return exports;
}

SessionWrapper::SessionWrapper(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<SessionWrapper>(info),
      start_time_(std::chrono::steady_clock::now()) {
  // Constructor - session will be initialized when loadSession is called
}

SessionWrapper::~SessionWrapper() {
  // Cleanup handled by unique_ptr
  // Clear callbacks to release FunctionReferences
  callbacks_.clear();
}

Napi::Value SessionWrapper::LoadSession(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // Validate arguments
  if (info.Length() < 1 || !info[0].IsObject()) {
    Napi::TypeError::New(env, "Expected object with sessionPath")
        .ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::Object payload = info[0].As<Napi::Object>();

  if (!payload.Has("sessionPath") || !payload.Get("sessionPath").IsString()) {
    Napi::TypeError::New(env, "sessionPath must be a string")
        .ThrowAsJavaScriptException();
    return env.Null();
  }

  std::string sessionPath = payload.Get("sessionPath").As<Napi::String>().Utf8Value();
  session_path_ = sessionPath;

  try {
    // Load session from JSON file using Orpheus SDK
    SessionGraph loaded_session = session_json::LoadSessionFromFile(sessionPath);

    // Transfer ownership to our managed pointer
    session_ = std::make_unique<SessionGraph>(std::move(loaded_session));

    // Build success response
    Napi::Object result = Napi::Object::New(env);
    result.Set("success", true);

    Napi::Object resultData = Napi::Object::New(env);
    resultData.Set("sessionPath", sessionPath);
    resultData.Set("sessionName", session_->name());
    resultData.Set("trackCount", static_cast<uint32_t>(session_->tracks().size()));
    resultData.Set("tempo", session_->tempo());

    result.Set("result", resultData);

    // Emit SessionChanged event
    EmitSessionChanged();

    return result;

  } catch (const std::exception& ex) {
    Napi::Object result = Napi::Object::New(env);
    result.Set("success", false);

    Napi::Object error = Napi::Object::New(env);
    error.Set("code", "session.load");
    error.Set("message", "Failed to load session");
    error.Set("details", ex.what());

    result.Set("error", error);
    return result;
  }
}

Napi::Value SessionWrapper::GetSessionInfo(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (!session_) {
    Napi::Error::New(env, "No session loaded")
        .ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::Object info_obj = Napi::Object::New(env);
  info_obj.Set("name", session_->name());
  info_obj.Set("tempo", session_->tempo());
  info_obj.Set("trackCount", session_->tracks().size());

  return info_obj;
}

Napi::Value SessionWrapper::RenderClick(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // Validate arguments
  if (info.Length() < 1 || !info[0].IsObject()) {
    Napi::TypeError::New(env, "Expected object with render parameters")
        .ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::Object params = info[0].As<Napi::Object>();

  // Extract parameters
  if (!params.Has("outputPath") || !params.Get("outputPath").IsString()) {
    Napi::TypeError::New(env, "outputPath is required")
        .ThrowAsJavaScriptException();
    return env.Null();
  }
  std::string outputPath = params.Get("outputPath").As<Napi::String>().Utf8Value();

  uint32_t bars = params.Has("bars") && params.Get("bars").IsNumber()
      ? params.Get("bars").As<Napi::Number>().Uint32Value()
      : 4;

  double bpm = params.Has("bpm") && params.Get("bpm").IsNumber()
      ? params.Get("bpm").As<Napi::Number>().DoubleValue()
      : (session_ ? session_->tempo() : 120.0);

  uint32_t sampleRate = params.Has("sampleRate") && params.Get("sampleRate").IsNumber()
      ? params.Get("sampleRate").As<Napi::Number>().Uint32Value()
      : (session_ ? session_->render_sample_rate() : 48000);

  try {
    // Get render API via ABI
    uint32_t got_major = 0, got_minor = 0;
    const orpheus_render_api_v1* render_api =
        orpheus_render_abi_v1(ORPHEUS_ABI_MAJOR, &got_major, &got_minor);

    if (!render_api) {
      throw std::runtime_error("Failed to negotiate render ABI");
    }

    // Build click spec
    orpheus_render_click_spec spec{};
    spec.tempo_bpm = bpm;
    spec.bars = bars;
    spec.sample_rate = sampleRate;
    spec.channels = 2;
    spec.gain = 0.3;
    spec.click_frequency_hz = 1000.0;
    spec.click_duration_seconds = 0.05;

    // Render click track
    orpheus_status status = render_api->render_click(&spec, outputPath.c_str());

    if (status != ORPHEUS_STATUS_OK) {
      std::stringstream ss;
      ss << "Render failed: " << orpheus_status_to_string(status);
      throw std::runtime_error(ss.str());
    }

    // Build success response
    Napi::Object result = Napi::Object::New(env);
    result.Set("success", true);

    Napi::Object resultData = Napi::Object::New(env);
    resultData.Set("outputPath", outputPath);
    resultData.Set("bars", bars);
    resultData.Set("bpm", bpm);
    resultData.Set("sampleRate", sampleRate);

    result.Set("result", resultData);
    return result;

  } catch (const std::exception& ex) {
    Napi::Object result = Napi::Object::New(env);
    result.Set("success", false);

    Napi::Object error = Napi::Object::New(env);
    error.Set("code", "render.click");
    error.Set("message", "Failed to render click track");
    error.Set("details", ex.what());

    result.Set("error", error);
    return result;
  }
}

Napi::Value SessionWrapper::GetTempo(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (!session_) {
    Napi::Error::New(env, "No session loaded")
        .ThrowAsJavaScriptException();
    return env.Null();
  }

  return Napi::Number::New(env, session_->tempo());
}

Napi::Value SessionWrapper::SetTempo(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (!session_) {
    Napi::Error::New(env, "No session loaded")
        .ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() < 1 || !info[0].IsNumber()) {
    Napi::TypeError::New(env, "Expected number for tempo")
        .ThrowAsJavaScriptException();
    return env.Null();
  }

  double tempo = info[0].As<Napi::Number>().DoubleValue();
  session_->set_tempo(tempo);

  return env.Undefined();
}

Napi::Value SessionWrapper::Subscribe(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1 || !info[0].IsFunction()) {
    Napi::TypeError::New(env, "Expected callback function")
        .ThrowAsJavaScriptException();
    return env.Null();
  }

  // Store the callback with a unique ID
  uint32_t callback_id = next_callback_id_++;
  Napi::Function callback = info[0].As<Napi::Function>();

  callbacks_.push_back(CallbackEntry{
    callback_id,
    Napi::Persistent(callback)
  });

  // Return an unsubscribe function
  Napi::Function unsubscribe_fn = Napi::Function::New(env, [this, callback_id](const Napi::CallbackInfo& unsubscribe_info) {
    // Remove callback by ID
    callbacks_.erase(
      std::remove_if(callbacks_.begin(), callbacks_.end(),
        [callback_id](const CallbackEntry& entry) {
          return entry.id == callback_id;
        }),
      callbacks_.end()
    );
    return unsubscribe_info.Env().Undefined();
  });

  return unsubscribe_fn;
}

Napi::Value SessionWrapper::Unsubscribe(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1 || !info[0].IsNumber()) {
    Napi::TypeError::New(env, "Expected callback ID")
        .ThrowAsJavaScriptException();
    return env.Null();
  }

  uint32_t callback_id = info[0].As<Napi::Number>().Uint32Value();

  // Remove callback by ID
  callbacks_.erase(
    std::remove_if(callbacks_.begin(), callbacks_.end(),
      [callback_id](const CallbackEntry& entry) {
        return entry.id == callback_id;
      }),
    callbacks_.end()
  );

  return env.Undefined();
}

void SessionWrapper::EmitEvent(const Napi::Env& env, const Napi::Object& event) {
  // Call all registered callbacks
  for (auto& entry : callbacks_) {
    try {
      entry.callback.Call({event});
    } catch (const std::exception& ex) {
      // Silently ignore callback errors to prevent one bad callback from affecting others
      std::cerr << "Error in event callback: " << ex.what() << std::endl;
    }
  }
}

void SessionWrapper::EmitSessionChanged() {
  if (callbacks_.empty()) {
    return;  // No callbacks registered
  }

  // Get the environment from the first callback
  Napi::Env env = callbacks_[0].callback.Env();

  // Build SessionChanged event
  Napi::Object event = Napi::Object::New(env);
  event.Set("type", "SessionChanged");

  // Current timestamp in milliseconds
  auto now = std::chrono::system_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
  event.Set("timestamp", static_cast<double>(ms.count()));

  if (!session_path_.empty()) {
    event.Set("sessionPath", session_path_);
  }

  if (session_) {
    event.Set("trackCount", static_cast<uint32_t>(session_->tracks().size()));
  }

  event.Set("sequenceId", sequence_id_++);

  EmitEvent(env, event);
}

void SessionWrapper::EmitHeartbeat() {
  if (callbacks_.empty()) {
    return;  // No callbacks registered
  }

  // Get the environment from the first callback
  Napi::Env env = callbacks_[0].callback.Env();

  // Build Heartbeat event
  Napi::Object event = Napi::Object::New(env);
  event.Set("type", "Heartbeat");

  // Current timestamp in milliseconds
  auto now = std::chrono::system_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
  event.Set("timestamp", static_cast<double>(ms.count()));

  // Uptime in seconds
  auto uptime_duration = std::chrono::steady_clock::now() - start_time_;
  auto uptime_seconds = std::chrono::duration_cast<std::chrono::seconds>(uptime_duration);
  event.Set("uptime", static_cast<double>(uptime_seconds.count()));

  event.Set("sequenceId", sequence_id_++);

  EmitEvent(env, event);
}
