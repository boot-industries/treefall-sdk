// SPDX-License-Identifier: MIT
/**
 * Orpheus Native Driver - N-API Bindings
 *
 * Direct Node.js ↔ C++ integration for Orpheus SDK
 * Provides zero-copy, low-latency access to Orpheus core features
 */

#include "session_wrapper.h"
#include <napi.h>

/**
 * Module initialization
 *
 * Exports:
 * - Session: SessionGraph wrapper class
 */
Napi::Object Init(Napi::Env env, Napi::Object exports) {
  // Register SessionWrapper class
  SessionWrapper::Init(env, exports);

  // Add module metadata
  exports.Set("version", Napi::String::New(env, "0.1.0-alpha.0"));
  exports.Set("driver", Napi::String::New(env, "native"));

  return exports;
}

// Register the module
NODE_API_MODULE(orpheus_native, Init)
