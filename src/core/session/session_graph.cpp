#include "session_graph.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace orpheus::core {

namespace {
constexpr double kMinimumLengthBeats = 1e-6;
}

Clip::Clip(std::string name, double start_beats, double length_beats)
    : name_(std::move(name)),
      start_beats_(start_beats),
      length_beats_(std::max(length_beats, kMinimumLengthBeats)) {}

void Clip::set_start(double start_beats) { start_beats_ = start_beats; }

void Clip::set_length(double length_beats) {
  length_beats_ = std::max(length_beats, kMinimumLengthBeats);
}

Track::Track(std::string name) : name_(std::move(name)) {}

Clip *Track::add_clip(std::string name, double start_beats, double length_beats) {
  auto &slot = clips_.emplace_back(
      std::make_unique<Clip>(std::move(name), start_beats, length_beats));
  return slot.get();
}

bool Track::remove_clip(const Clip *clip) {
  const auto it = std::find_if(clips_.begin(), clips_.end(),
                               [&](const std::unique_ptr<Clip> &candidate) {
                                 return candidate.get() == clip;
                               });
  if (it == clips_.end()) {
    return false;
  }
  clips_.erase(it);
  return true;
}

Clip *Track::find_clip(const Clip *clip) {
  const auto it = std::find_if(clips_.begin(), clips_.end(),
                               [&](const std::unique_ptr<Clip> &candidate) {
                                 return candidate.get() == clip;
                               });
  if (it == clips_.end()) {
    return nullptr;
  }
  return it->get();
}

void Track::sort_clips() {
  std::sort(clips_.begin(), clips_.end(),
            [](const std::unique_ptr<Clip> &lhs, const std::unique_ptr<Clip> &rhs) {
              return lhs->start() < rhs->start();
            });
}

SessionGraph::SessionGraph() : name_("Session") {}

void SessionGraph::set_name(std::string name) { name_ = std::move(name); }

Track *SessionGraph::add_track(std::string name) {
  auto &slot = tracks_.emplace_back(std::make_unique<Track>(std::move(name)));
  return slot.get();
}

bool SessionGraph::remove_track(const Track *track) {
  const auto it = std::find_if(tracks_.begin(), tracks_.end(),
                               [&](const std::unique_ptr<Track> &candidate) {
                                 return candidate.get() == track;
                               });
  if (it == tracks_.end()) {
    return false;
  }
  tracks_.erase(it);
  return true;
}

void SessionGraph::set_tempo(double bpm) {
  if (bpm <= 0.0) {
    throw std::invalid_argument("Tempo must be positive");
  }
  tempo_bpm_ = bpm;
}

TransportState SessionGraph::transport_state() const {
  TransportState state;
  state.tempo_bpm = tempo_bpm_;
  state.position_beats = transport_position_beats_;
  state.is_playing = transport_is_playing_;
  return state;
}

void SessionGraph::set_session_range(double start_beats, double end_beats) {
  if (end_beats < start_beats) {
    throw std::invalid_argument("Session end must not precede start");
  }
  session_start_beats_ = start_beats;
  session_end_beats_ = end_beats;
}

Clip *SessionGraph::add_clip(Track &track, std::string name, double start_beats,
                             double length_beats) {
  Track *target = find_track(&track);
  if (target == nullptr) {
    throw std::invalid_argument("Track does not belong to session");
  }
  return target->add_clip(std::move(name), start_beats, length_beats);
}

bool SessionGraph::remove_clip(const Clip *clip) {
  for (const auto &track : tracks_) {
    if (track->remove_clip(clip)) {
      return true;
    }
  }
  return false;
}

void SessionGraph::set_clip_start(Clip &clip, double start_beats) {
  Clip *target = find_clip(&clip);
  if (target == nullptr) {
    throw std::invalid_argument("Clip does not belong to session");
  }
  target->set_start(start_beats);
}

void SessionGraph::set_clip_length(Clip &clip, double length_beats) {
  Clip *target = find_clip(&clip);
  if (target == nullptr) {
    throw std::invalid_argument("Clip does not belong to session");
  }
  target->set_length(length_beats);
}

void SessionGraph::commit_clip_grid() {
  for (const auto &track : tracks_) {
    track->sort_clips();
  }
}

Track *SessionGraph::find_track(const Track *track) {
  const auto it = std::find_if(tracks_.begin(), tracks_.end(),
                               [&](const std::unique_ptr<Track> &candidate) {
                                 return candidate.get() == track;
                               });
  if (it == tracks_.end()) {
    return nullptr;
  }
  return it->get();
}

Clip *SessionGraph::find_clip(const Clip *clip) {
  for (const auto &track : tracks_) {
    Clip *candidate = track->find_clip(clip);
    if (candidate != nullptr) {
      return candidate;
    }
  }
  return nullptr;
}

}  // namespace orpheus::core
