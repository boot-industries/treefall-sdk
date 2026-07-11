// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

#include "orpheus/abi.h"

namespace orpheus::adapters {

// Negotiate a single versioned ABI table.
//
// Each orpheus_*_abi_v1() getter follows the same contract: it takes the
// requested major version and out-params for the actual major/minor, and
// returns the API table (or nullptr). Adapters then must reject the table
// unless the major/minor exactly match what the SDK headers were built
// against. This helper captures that null + version check once so it is not
// copy-pasted per ABI table across the minhost and reaper adapters.
//
// `getter` is any of orpheus_session_abi_v1 / orpheus_clipgrid_abi_v1 /
// orpheus_render_abi_v1. `out_major`/`out_minor` receive the negotiated
// version even when the table is rejected (useful for diagnostics).
template <typename Getter>
auto NegotiateAbi(Getter getter, std::uint32_t& out_major, std::uint32_t& out_minor)
    -> decltype(getter(ORPHEUS_ABI_MAJOR, &out_major, &out_minor)) {
  out_major = 0;
  out_minor = 0;
  const auto* api = getter(ORPHEUS_ABI_MAJOR, &out_major, &out_minor);
  if (api == nullptr || out_major != ORPHEUS_ABI_MAJOR || out_minor != ORPHEUS_ABI_MINOR) {
    return nullptr;
  }
  return api;
}

// Convenience overload for callers that do not need the negotiated version.
template <typename Getter> auto NegotiateAbi(Getter getter) {
  std::uint32_t major = 0;
  std::uint32_t minor = 0;
  return NegotiateAbi(getter, major, minor);
}

} // namespace orpheus::adapters
