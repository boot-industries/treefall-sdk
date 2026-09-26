// SPDX-License-Identifier: MIT
#include "abi/abi_runtime.h"

#include <orpheus/session_graph.h>

#include <mutex>
#include <unordered_set>

namespace orpheus::abi_internal {
namespace {

// Function-local so construction is thread-safe and destruction is ordered at
// exit. Address-keyed: the set is the only authority on whether a handle is
// live, and no handle value is ever dereferenced. Host/control-side only; no
// ABI entry point runs on an audio callback.
std::mutex& RegistryMutex() {
  static std::mutex mutex;
  return mutex;
}

std::unordered_set<orpheus_session_handle>& Registry() {
  static std::unordered_set<orpheus_session_handle> registry;
  return registry;
}

} // namespace

bool RegisterSession(core::SessionGraph* graph, orpheus_session_handle& out_handle) {
  if (graph == nullptr) {
    return false;
  }
  const orpheus_session_handle handle = reinterpret_cast<orpheus_session_handle>(graph);
  const std::lock_guard<std::mutex> lock(RegistryMutex());
  if (!Registry().insert(handle).second) {
    return false;
  }
  out_handle = handle;
  return true;
}

core::SessionGraph* ResolveSession(orpheus_session_handle handle) {
  if (handle == nullptr) {
    return nullptr;
  }
  const std::lock_guard<std::mutex> lock(RegistryMutex());
  if (Registry().find(handle) == Registry().end()) {
    return nullptr;
  }
  return reinterpret_cast<core::SessionGraph*>(handle);
}

core::SessionGraph* UnregisterSession(orpheus_session_handle handle) {
  if (handle == nullptr) {
    return nullptr;
  }
  const std::lock_guard<std::mutex> lock(RegistryMutex());
  if (Registry().erase(handle) == 0U) {
    return nullptr;
  }
  return reinterpret_cast<core::SessionGraph*>(handle);
}

} // namespace orpheus::abi_internal
