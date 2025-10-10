// SPDX-License-Identifier: MIT
#include "orpheus/abi.h"

#include "abi/abi_internal.h"

#include <string>

using orpheus::abi_internal::GuardAbiCall;
using orpheus::abi_internal::ToClip;
using orpheus::abi_internal::ToSession;
using orpheus::abi_internal::ToTrack;

namespace {

orpheus_status ClipgridAddClip(orpheus_session_handle session, orpheus_track_handle track,
                               const orpheus_clip_desc* desc, orpheus_clip_handle* out_clip) {
  if (session == nullptr || track == nullptr || desc == nullptr || out_clip == nullptr) {
    return ORPHEUS_STATUS_INVALID_ARGUMENT;
  }
  return GuardAbiCall([&]() -> orpheus_status {
    auto* session_ptr = ToSession(session);
    auto* track_ptr = ToTrack(track);
    const std::string name = desc->name != nullptr ? desc->name : "";
    orpheus::core::Clip* clip = session_ptr->add_clip(*track_ptr, name, desc->start_beats,
                                                      desc->length_beats, desc->scene_index);
    *out_clip = reinterpret_cast<orpheus_clip_handle>(clip);
    return ORPHEUS_STATUS_OK;
  });
}

orpheus_status ClipgridRemoveClip(orpheus_session_handle session, orpheus_clip_handle clip) {
  if (session == nullptr || clip == nullptr) {
    return ORPHEUS_STATUS_INVALID_ARGUMENT;
  }
  return GuardAbiCall([&]() -> orpheus_status {
    auto* session_ptr = ToSession(session);
    auto* clip_ptr = ToClip(clip);
    if (!session_ptr->remove_clip(clip_ptr)) {
      return ORPHEUS_STATUS_NOT_FOUND;
    }
    return ORPHEUS_STATUS_OK;
  });
}

orpheus_status ClipgridSetClipStart(orpheus_session_handle session, orpheus_clip_handle clip,
                                    double start_beats) {
  if (session == nullptr || clip == nullptr) {
    return ORPHEUS_STATUS_INVALID_ARGUMENT;
  }
  return GuardAbiCall([&]() -> orpheus_status {
    auto* session_ptr = ToSession(session);
    auto* clip_ptr = ToClip(clip);
    session_ptr->set_clip_start(*clip_ptr, start_beats);
    return ORPHEUS_STATUS_OK;
  });
}

orpheus_status ClipgridSetClipLength(orpheus_session_handle session, orpheus_clip_handle clip,
                                     double length_beats) {
  if (session == nullptr || clip == nullptr) {
    return ORPHEUS_STATUS_INVALID_ARGUMENT;
  }
  return GuardAbiCall([&]() -> orpheus_status {
    auto* session_ptr = ToSession(session);
    auto* clip_ptr = ToClip(clip);
    session_ptr->set_clip_length(*clip_ptr, length_beats);
    return ORPHEUS_STATUS_OK;
  });
}

orpheus_status ClipgridSetClipScene(orpheus_session_handle session, orpheus_clip_handle clip,
                                    uint32_t scene_index) {
  if (session == nullptr || clip == nullptr) {
    return ORPHEUS_STATUS_INVALID_ARGUMENT;
  }
  return GuardAbiCall([&]() -> orpheus_status {
    auto* session_ptr = ToSession(session);
    auto* clip_ptr = ToClip(clip);
    session_ptr->set_clip_scene(*clip_ptr, scene_index);
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

orpheus_status ClipgridTriggerScene(orpheus_session_handle session,
                                    const orpheus_scene_trigger_desc* desc) {
  if (session == nullptr || desc == nullptr) {
    return ORPHEUS_STATUS_INVALID_ARGUMENT;
  }
  return GuardAbiCall([&]() -> orpheus_status {
    auto* session_ptr = ToSession(session);
    orpheus::core::QuantizationWindow window;
    window.grid_beats = desc->quant.grid_beats;
    window.tolerance_beats = desc->quant.tolerance_beats;
    session_ptr->trigger_scene(desc->scene_index, desc->position_beats, window);
    return ORPHEUS_STATUS_OK;
  });
}

orpheus_status ClipgridEndScene(orpheus_session_handle session,
                                const orpheus_scene_end_desc* desc) {
  if (session == nullptr || desc == nullptr) {
    return ORPHEUS_STATUS_INVALID_ARGUMENT;
  }
  return GuardAbiCall([&]() -> orpheus_status {
    auto* session_ptr = ToSession(session);
    orpheus::core::QuantizationWindow window;
    window.grid_beats = desc->quant.grid_beats;
    window.tolerance_beats = desc->quant.tolerance_beats;
    session_ptr->end_scene(desc->scene_index, desc->position_beats, window);
    return ORPHEUS_STATUS_OK;
  });
}

orpheus_status ClipgridCommitArrangement(orpheus_session_handle session,
                                         const orpheus_arrangement_commit_desc* desc) {
  if (session == nullptr) {
    return ORPHEUS_STATUS_INVALID_ARGUMENT;
  }
  return GuardAbiCall([&]() -> orpheus_status {
    const double fallback = desc != nullptr ? desc->fallback_scene_length_beats : 0.0;
    ToSession(session)->commit_arrangement(fallback);
    return ORPHEUS_STATUS_OK;
  });
}

const orpheus_clipgrid_api_v1 kClipgridApiV1{ORPHEUS_CLIPGRID_CAP_V1_CORE |
                                                 ORPHEUS_CLIPGRID_CAP_V1_SCENES,
                                             &ClipgridAddClip,
                                             &ClipgridRemoveClip,
                                             &ClipgridSetClipStart,
                                             &ClipgridSetClipLength,
                                             &ClipgridSetClipScene,
                                             &ClipgridCommit,
                                             &ClipgridTriggerScene,
                                             &ClipgridEndScene,
                                             &ClipgridCommitArrangement};

} // namespace

extern "C" ORPHEUS_API const orpheus_clipgrid_api_v1*
orpheus_clipgrid_abi_v1(uint32_t want_major, uint32_t* got_major, uint32_t* got_minor) {
  if (got_major != nullptr) {
    *got_major = ORPHEUS_ABI_MAJOR;
  }
  if (got_minor != nullptr) {
    *got_minor = ORPHEUS_ABI_MINOR;
  }
  if (want_major != ORPHEUS_ABI_MAJOR) {
    return nullptr;
  }
  return &kClipgridApiV1;
}
