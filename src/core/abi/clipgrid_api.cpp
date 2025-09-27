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

const orpheus_clipgrid_api_v1 kClipgridApiV1{
    ORPHEUS_CLIPGRID_CAP_V1_CORE, &ClipgridAddClip, &ClipgridRemoveClip,
    &ClipgridSetClipStart,        &ClipgridSetClipLength,
    &ClipgridCommit};

}  // namespace

extern "C" {

const orpheus_clipgrid_api_v1 *orpheus_clipgrid_abi_v1(uint32_t want_major,
                                                       uint32_t *got_major,
                                                       uint32_t *got_minor) {
  (void)want_major;
  if (got_major != nullptr) {
    *got_major = ORPHEUS_ABI_V1_MAJOR;
  }
  if (got_minor != nullptr) {
    *got_minor = ORPHEUS_ABI_V1_MINOR;
  }
  return &kClipgridApiV1;
}

}  // extern "C"
