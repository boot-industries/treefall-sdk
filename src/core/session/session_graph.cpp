// SPDX-License-Identifier: MIT
#include "session_graph.h"

#include <algorithm>
#include <cmath>
#include <limits>
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
              if (lhs->start() < rhs->start()) {
                return true;
              }
              if (rhs->start() < lhs->start()) {
                return false;
              }
              return lhs->name() < rhs->name();
            });
}

SessionGraph::SessionGraph() : name_("Session") {}

void SessionGraph::set_name(std::string name) { name_ = std::move(name); }

Track *SessionGraph::add_track(std::string name) {
  auto &slot = tracks_.emplace_back(std::make_unique<Track>(std::move(name)));
  mark_clip_grid_dirty();
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
  mark_clip_grid_dirty();
  return true;
}

void SessionGraph::set_tempo(double bpm) {
  if (bpm <= 0.0) {
    throw std::invalid_argument("Tempo must be positive");
  }
  tempo_bpm_ = bpm;
}

void SessionGraph::set_render_sample_rate(std::uint32_t sample_rate_hz) {
  if (sample_rate_hz == 0u) {
    throw std::invalid_argument("Sample rate must be non-zero");
  }
  render_sample_rate_hz_ = sample_rate_hz;
}

void SessionGraph::set_render_bit_depth(std::uint16_t bit_depth_bits) {
  switch (bit_depth_bits) {
    case 16:
    case 24:
      render_bit_depth_bits_ = bit_depth_bits;
      return;
    default:
      throw std::invalid_argument("Unsupported bit depth");
  }
}

void SessionGraph::set_render_dither(bool enabled) {
  render_dither_enabled_ = enabled;
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
  mark_clip_grid_dirty();
  return target->add_clip(std::move(name), start_beats, length_beats);
}

bool SessionGraph::remove_clip(const Clip *clip) {
  for (const auto &track : tracks_) {
    if (track->remove_clip(clip)) {
      mark_clip_grid_dirty();
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
  mark_clip_grid_dirty();
}

void SessionGraph::set_clip_length(Clip &clip, double length_beats) {
  Clip *target = find_clip(&clip);
  if (target == nullptr) {
    throw std::invalid_argument("Clip does not belong to session");
  }
  target->set_length(length_beats);
  mark_clip_grid_dirty();
}

void SessionGraph::commit_clip_grid() {
  if (!clip_grid_dirty_) {
    return;
  }
  clip_grid_dirty_ = false;
  std::stable_sort(tracks_.begin(), tracks_.end(),
                   [](const std::unique_ptr<Track> &lhs,
                      const std::unique_ptr<Track> &rhs) {
                     return lhs->name() < rhs->name();
                   });

  double min_start = std::numeric_limits<double>::infinity();
  double max_end = std::numeric_limits<double>::lowest();
  bool has_clips = false;

  for (const auto &track : tracks_) {
    track->sort_clips();
    for (const auto &clip : track->clips()) {
      has_clips = true;
      min_start = std::min(min_start, clip->start());
      max_end = std::max(max_end, clip->start() + clip->length());
    }
  }

  if (!has_clips) {
    session_start_beats_ = 0.0;
    session_end_beats_ = 0.0;
  } else {
    session_start_beats_ = (std::isfinite(min_start) ? min_start : 0.0);
    session_end_beats_ = std::max(session_start_beats_, max_end);
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
