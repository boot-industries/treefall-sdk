// SPDX-License-Identifier: MIT
#pragma once

#include "orpheus/abi.h"

#include "session/session_graph.h"

#include <filesystem>
#include <ios>
#include <new>
#include <stdexcept>

namespace orpheus::abi_internal {

class IoException : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

inline constexpr orpheus_abi_version kCurrentAbi{1, 0};

inline orpheus::core::SessionGraph *ToSession(orpheus_session_handle handle) {
  return reinterpret_cast<orpheus::core::SessionGraph *>(handle);
}

inline orpheus::core::Track *ToTrack(orpheus_track_handle handle) {
  return reinterpret_cast<orpheus::core::Track *>(handle);
}

inline orpheus::core::Clip *ToClip(orpheus_clip_handle handle) {
  return reinterpret_cast<orpheus::core::Clip *>(handle);
}

template <typename Fn>
orpheus_status GuardAbiCall(Fn &&fn) {
  try {
    return fn();
  } catch (const std::invalid_argument &) {
    return ORPHEUS_STATUS_INVALID_ARGUMENT;
  } catch (const IoException &) {
    return ORPHEUS_STATUS_IO_ERROR;
  } catch (const std::filesystem::filesystem_error &) {
    return ORPHEUS_STATUS_IO_ERROR;
  } catch (const std::ios_base::failure &) {
    return ORPHEUS_STATUS_IO_ERROR;
  } catch (const std::bad_alloc &) {
    return ORPHEUS_STATUS_OUT_OF_MEMORY;
  } catch (...) {
    return ORPHEUS_STATUS_INTERNAL_ERROR;
  }
}

}  // namespace orpheus::abi_internal
