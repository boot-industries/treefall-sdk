// SPDX-License-Identifier: MIT
#include "orpheus/abi.h"

#include "abi/abi_internal.h"

#include <algorithm>
#include <string>

using orpheus::abi_internal::GuardAbiCall;
using orpheus::abi_internal::ToSession;
using orpheus::abi_internal::ToTrack;

namespace {

orpheus_status SessionCreate(orpheus_session_handle *out_session) {
  if (out_session == nullptr) {
    return ORPHEUS_STATUS_INVALID_ARGUMENT;
  }
  return GuardAbiCall([&]() -> orpheus_status {
    auto *session = new orpheus::core::SessionGraph();
    *out_session = reinterpret_cast<orpheus_session_handle>(session);
    return ORPHEUS_STATUS_OK;
  });
}

void SessionDestroy(orpheus_session_handle session) {
  auto *ptr = ToSession(session);
  delete ptr;
}

orpheus_status SessionAddTrack(orpheus_session_handle session,
                               const orpheus_track_desc *desc,
                               orpheus_track_handle *out_track) {
  if (session == nullptr || out_track == nullptr || desc == nullptr) {
    return ORPHEUS_STATUS_INVALID_ARGUMENT;
  }
  return GuardAbiCall([&]() -> orpheus_status {
    auto *session_ptr = ToSession(session);
    const std::string name = desc->name != nullptr ? desc->name : "";
    orpheus::core::Track *track = session_ptr->add_track(name);
    *out_track = reinterpret_cast<orpheus_track_handle>(track);
    return ORPHEUS_STATUS_OK;
  });
}

orpheus_status SessionRemoveTrack(orpheus_session_handle session,
                                  orpheus_track_handle track) {
  if (session == nullptr || track == nullptr) {
    return ORPHEUS_STATUS_INVALID_ARGUMENT;
  }
  return GuardAbiCall([&]() -> orpheus_status {
    auto *session_ptr = ToSession(session);
    auto *track_ptr = ToTrack(track);
    if (!session_ptr->remove_track(track_ptr)) {
      return ORPHEUS_STATUS_NOT_FOUND;
    }
    return ORPHEUS_STATUS_OK;
  });
}

orpheus_status SessionSetTempo(orpheus_session_handle session, double bpm) {
  if (session == nullptr) {
    return ORPHEUS_STATUS_INVALID_ARGUMENT;
  }
  return GuardAbiCall([&]() -> orpheus_status {
    ToSession(session)->set_tempo(bpm);
    return ORPHEUS_STATUS_OK;
  });
}

orpheus_status SessionGetTransportState(orpheus_session_handle session,
                                        orpheus_transport_state *out_state) {
  if (session == nullptr || out_state == nullptr) {
    return ORPHEUS_STATUS_INVALID_ARGUMENT;
  }
  auto *session_ptr = ToSession(session);
  const auto state = session_ptr->transport_state();
  out_state->tempo_bpm = state.tempo_bpm;
  out_state->position_beats = state.position_beats;
  out_state->is_playing = state.is_playing ? 1 : 0;
  return ORPHEUS_STATUS_OK;
}

const orpheus_session_v1 kSessionV1{
    &SessionCreate,        &SessionDestroy,     &SessionAddTrack,
    &SessionRemoveTrack,   &SessionSetTempo,    &SessionGetTransportState};

orpheus_abi_version NegotiateAbi(orpheus_abi_version requested) {
  using orpheus::abi_internal::kCurrentAbi;
  if (requested.major != kCurrentAbi.major) {
    return kCurrentAbi;
  }
  orpheus_abi_version negotiated = kCurrentAbi;
  negotiated.minor = std::min(requested.minor, kCurrentAbi.minor);
  return negotiated;
}

const orpheus_abi_negotiator kNegotiator{&NegotiateAbi};

}  // namespace

extern "C" {

const orpheus_session_v1 *orpheus_session_abi_v1() { return &kSessionV1; }

const orpheus_abi_negotiator *orpheus_negotiate_abi() { return &kNegotiator; }

}  // extern "C"
