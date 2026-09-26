// SPDX-License-Identifier: MIT
#pragma once

#include "orpheus/abi.h"

// A dedicated export macro: the three ABI libraries force-define
// ORPHEUS_BUILDING_DLL for themselves, so ORPHEUS_API would make each of them
// expect their own definition of a symbol the runtime library owns.
#if defined(_WIN32) && defined(ORPHEUS_ABI_RUNTIME_BUILDING_DLL)
#define ORPHEUS_ABI_RUNTIME_API __declspec(dllexport)
#elif defined(_WIN32)
#define ORPHEUS_ABI_RUNTIME_API __declspec(dllimport)
#else
#define ORPHEUS_ABI_RUNTIME_API
#endif

namespace orpheus::core {
class SessionGraph;
} // namespace orpheus::core

namespace orpheus::abi_internal {

/// Register a session graph and publish its handle. Returns false and leaves
/// `out_handle` unchanged when registration fails (for example on allocation
/// failure); the caller keeps ownership of the graph and must destroy it.
ORPHEUS_ABI_RUNTIME_API bool RegisterSession(core::SessionGraph* graph,
                                             orpheus_session_handle& out_handle);

/// Resolve a handle to its live graph, or nullptr when the handle is null or
/// is not currently registered. Never dereferences the handle.
ORPHEUS_ABI_RUNTIME_API core::SessionGraph* ResolveSession(orpheus_session_handle handle);

/// Atomically remove a registered handle and return its graph, or nullptr when
/// the handle is null, unknown, or already destroyed. The caller owns the
/// returned graph and deletes it; `delete nullptr` is a well-defined no-op.
ORPHEUS_ABI_RUNTIME_API core::SessionGraph* UnregisterSession(orpheus_session_handle handle);

} // namespace orpheus::abi_internal
