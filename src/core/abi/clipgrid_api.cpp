// SPDX-License-Identifier: MIT
#include "orpheus/abi.h"

#include "abi/abi_internal.h"

#include <string>

using orpheus::abi_internal::GuardAbiCall;
using orpheus::abi_internal::ToClip;
using orpheus::abi_internal::ToSession;
using orpheus::abi_internal::ToTrack;

namespace {

orpheus_status ClipgridAddClip(orpheus_session_handle session,
                               orpheus_track_handle track,
                               const orpheus_clip_desc *desc,
                               orpheus_clip_handle *out_clip) {
  if (session == nullptr || track == nullptr || desc == nullptr ||
      out_clip == nullptr) {
    return ORPHEUS_STATUS_INVALID_ARGUMENT;
  }
  return GuardAbiCall([&]() -> orpheus_status {
    auto *session_ptr = ToSession(session);
    auto *track_ptr = ToTrack(track);
    const std::string name = desc->name != nullptr ? desc->name : "";
    orpheus::core::Clip *clip = session_ptr->add_clip(*track_ptr, name,
                                                      desc->start_beats,
                                                      desc->length_beats);
    *out_clip = reinterpret_cast<orpheus_clip_handle>(clip);
    return ORPHEUS_STATUS_OK;
  });
}

orpheus_status ClipgridRemoveClip(orpheus_session_handle session,
                                  orpheus_clip_handle clip) {
  if (session == nullptr || clip == nullptr) {
    return ORPHEUS_STATUS_INVALID_ARGUMENT;
  }
  return GuardAbiCall([&]() -> orpheus_status {
    auto *session_ptr = ToSession(session);
    auto *clip_ptr = ToClip(clip);
    if (!session_ptr->remove_clip(clip_ptr)) {
      return ORPHEUS_STATUS_NOT_FOUND;
    }
    return ORPHEUS_STATUS_OK;
  });
}

orpheus_status ClipgridSetClipStart(orpheus_session_handle session,
                                    orpheus_clip_handle clip,
                                    double start_beats) {
  if (session == nullptr || clip == nullptr) {
    return ORPHEUS_STATUS_INVALID_ARGUMENT;
  }
  return GuardAbiCall([&]() -> orpheus_status {
    auto *session_ptr = ToSession(session);
    auto *clip_ptr = ToClip(clip);
    session_ptr->set_clip_start(*clip_ptr, start_beats);
    return ORPHEUS_STATUS_OK;
  });
}

orpheus_status ClipgridSetClipLength(orpheus_session_handle session,
                                     orpheus_clip_handle clip,
                                     double length_beats) {
  if (session == nullptr || clip == nullptr) {
    return ORPHEUS_STATUS_INVALID_ARGUMENT;
  }
  return GuardAbiCall([&]() -> orpheus_status {
    auto *session_ptr = ToSession(session);
    auto *clip_ptr = ToClip(clip);
    session_ptr->set_clip_length(*clip_ptr, length_beats);
    return ORPHEUS_STATUS_OK;
  });
}

orpheus_status ClipgridCommit(orpheus_session_handle session) {
  if (session == nullptr) {
    return ORPHEUS_STATUS_INVALID_ARGUMENT;
  }
  return GuardAbiCall([&]() -> orpheus_status {
    ToSession(session)->commit_clip_grid();
    return ORPHEUS_STATUS_OK;
  });
}

const orpheus_clipgrid_v1 kClipgridV1{&ClipgridAddClip,    &ClipgridRemoveClip,
                                      &ClipgridSetClipStart,
                                      &ClipgridSetClipLength,
                                      &ClipgridCommit};

}  // namespace

extern "C" {

const orpheus_clipgrid_v1 *orpheus_clipgrid_abi_v1() { return &kClipgridV1; }

}  // extern "C"
