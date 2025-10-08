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
constexpr double kClipOrderingTolerance = 1e-9;
}

Clip::Clip(std::string name, double start_beats, double length_beats,
           std::uint32_t scene_index)
    : name_(std::move(name)),
      start_beats_(start_beats),
      length_beats_(std::max(length_beats, kMinimumLengthBeats)),
      scene_index_(scene_index) {}

Clip::~Clip() = default;

void Clip::set_start(double start_beats) { start_beats_ = start_beats; }

void Clip::set_length(double length_beats) {
  length_beats_ = std::max(length_beats, kMinimumLengthBeats);
}

void Clip::set_scene_index(std::uint32_t scene_index) {
  scene_index_ = scene_index;
}

Track::Track(std::string name) : name_(std::move(name)) {}

Track::~Track() = default;

Clip *Track::add_clip(std::string name, double start_beats, double length_beats,
                      std::uint32_t scene_index) {
  auto clip = std::make_unique<Clip>(std::move(name), start_beats, length_beats,
                                     scene_index);
  Clip *raw = clip.get();
  clips_.push_back(std::move(clip));
  sort_clips();
  try {
    validate_clip_layout();
  } catch (...) {
    const bool removed = remove_clip(raw);
    (void)removed;
    throw;
  }
  return raw;
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

void Track::validate_clip_layout() const {
  if (clips_.empty()) {
    return;
  }
  for (std::size_t index = 1; index < clips_.size(); ++index) {
    const Clip &previous = *clips_[index - 1];
    const Clip &current = *clips_[index];
    if (current.start() + kClipOrderingTolerance < previous.start()) {
      throw std::invalid_argument("Clips on track \"" + name_ +
                                  "\" must be sorted by start time");
    }
    const double previous_end = previous.start() + previous.length();
    if (previous_end > current.start() + kClipOrderingTolerance) {
      throw std::invalid_argument("Clips on track \"" + name_ +
                                  "\" must not overlap");
    }
  }
}

MarkerSet::MarkerSet(std::string name) : name_(std::move(name)) {}

MarkerSet::~MarkerSet() = default;

MarkerSet::Marker *MarkerSet::add_marker(std::string name,
                                         double position_beats) {
  auto &marker = markers_.emplace_back();
  marker.name = std::move(name);
  marker.position_beats = position_beats;
  return &marker;
}

bool MarkerSet::remove_marker(const Marker *marker) {
  const auto it = std::find_if(markers_.begin(), markers_.end(),
                               [&](const Marker &candidate) {
                                 return &candidate == marker;
                               });
  if (it == markers_.end()) {
    return false;
  }
  markers_.erase(it);
  return true;
}

MarkerSet::Marker *MarkerSet::find_marker(const Marker *marker) {
  const auto it = std::find_if(markers_.begin(), markers_.end(),
                               [&](const Marker &candidate) {
                                 return &candidate == marker;
                               });
  if (it == markers_.end()) {
    return nullptr;
  }
  return &(*it);
}

PlaylistLane::PlaylistLane(std::string name, bool is_active)
    : name_(std::move(name)), is_active_(is_active) {}

PlaylistLane::~PlaylistLane() = default;

void PlaylistLane::set_active(bool active) { is_active_ = active; }

SessionGraph::SessionGraph() : name_("Session") {}

SessionGraph::~SessionGraph() = default;

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

MarkerSet *SessionGraph::add_marker_set(std::string name) {
  auto &slot = marker_sets_.emplace_back(
      std::make_unique<MarkerSet>(std::move(name)));
  return slot.get();
}

PlaylistLane *SessionGraph::add_playlist_lane(std::string name,
                                              bool is_active) {
  auto &slot = playlist_lanes_.emplace_back(
      std::make_unique<PlaylistLane>(std::move(name), is_active));
  return slot.get();
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
    case 32:
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
                             double length_beats, std::uint32_t scene_index) {
  Track *target = find_track(&track);
  if (target == nullptr) {
    throw std::invalid_argument("Track does not belong to session");
  }
  mark_clip_grid_dirty();
  return target->add_clip(std::move(name), start_beats, length_beats,
                          scene_index);
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
  Track *owner = find_clip_track(&clip);
  if (owner == nullptr) {
    throw std::invalid_argument("Clip does not belong to session");
  }
  Clip *target = owner->find_clip(&clip);
  const double previous_start = target->start();
  target->set_start(start_beats);
  owner->sort_clips();
  try {
    owner->validate_clip_layout();
  } catch (...) {
    target->set_start(previous_start);
    owner->sort_clips();
    throw;
  }
  mark_clip_grid_dirty();
}

void SessionGraph::set_clip_length(Clip &clip, double length_beats) {
  Track *owner = find_clip_track(&clip);
  if (owner == nullptr) {
    throw std::invalid_argument("Clip does not belong to session");
  }
  Clip *target = owner->find_clip(&clip);
  const double previous_length = target->length();
  target->set_length(length_beats);
  try {
    owner->validate_clip_layout();
  } catch (...) {
    target->set_length(previous_length);
    throw;
  }
  mark_clip_grid_dirty();
}

void SessionGraph::set_clip_scene(Clip &clip, std::uint32_t scene_index) {
  Clip *target = find_clip(&clip);
  if (target == nullptr) {
    throw std::invalid_argument("Clip does not belong to session");
  }
  target->set_scene_index(scene_index);
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
    track->validate_clip_layout();
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

double SessionGraph::QuantizePosition(double position_beats,
                                      const QuantizationWindow &quantization,
                                      double minimum_beats) {
  if (quantization.grid_beats <= 0.0) {
    throw std::invalid_argument("Quantization grid must be positive");
  }
  const double grid = quantization.grid_beats;
  const double normalized = position_beats / grid;
  const double nearest = std::round(normalized) * grid;
  const double diff = std::abs(nearest - position_beats);
  if (diff <= quantization.tolerance_beats) {
    return std::max(nearest, minimum_beats);
  }
  if (nearest < position_beats) {
    return std::max(nearest + grid, minimum_beats);
  }
  return std::max(nearest, minimum_beats);
}

void SessionGraph::trigger_scene(std::uint32_t scene_index,
                                 double position_beats,
                                 const QuantizationWindow &quantization) {
  const double quantized_start =
      QuantizePosition(position_beats, quantization, /*minimum_beats=*/0.0);

  SceneTimelineEntry entry;
  entry.scene_index = scene_index;
  entry.trigger_position_beats = position_beats;
  entry.trigger_quantization = quantization;
  entry.quantized_start_beats = quantized_start;
  const std::size_t index = scene_timeline_.size();
  scene_timeline_.push_back(entry);
  active_scenes_[scene_index] = ActiveScene{index};
}

void SessionGraph::end_scene(std::uint32_t scene_index, double position_beats,
                             const QuantizationWindow &quantization) {
  const auto active = active_scenes_.find(scene_index);
  if (active == active_scenes_.end()) {
    throw std::invalid_argument("Scene has not been triggered");
  }
  SceneTimelineEntry &entry = scene_timeline_[active->second.timeline_index];
  const double quantized_end =
      QuantizePosition(position_beats, quantization, entry.quantized_start_beats);
  entry.has_end = true;
  entry.end_position_beats = position_beats;
  entry.end_quantization = quantization;
  entry.quantized_end_beats = std::max(quantized_end, entry.quantized_start_beats);
  active_scenes_.erase(active);
}

void SessionGraph::update_session_range_from_commits() {
  if (committed_clips_.empty()) {
    session_start_beats_ = 0.0;
    session_end_beats_ = 0.0;
    return;
  }

  double min_start = std::numeric_limits<double>::infinity();
  double max_end = std::numeric_limits<double>::lowest();
  for (const auto &clip : committed_clips_) {
    min_start = std::min(min_start, clip.arranged_start_beats);
    max_end =
        std::max(max_end, clip.arranged_start_beats + clip.arranged_length_beats);
  }

  if (!std::isfinite(min_start) || !std::isfinite(max_end)) {
    session_start_beats_ = 0.0;
    session_end_beats_ = 0.0;
    return;
  }

  session_start_beats_ = min_start;
  session_end_beats_ = std::max(session_start_beats_, max_end);
}

void SessionGraph::commit_arrangement(double fallback_scene_length_beats) {
  committed_clips_.clear();

  std::vector<SceneTimelineEntry> resolved_timeline = scene_timeline_;

  for (auto &entry : resolved_timeline) {
    if (!entry.has_end) {
      entry.quantized_end_beats =
          entry.quantized_start_beats + std::max(fallback_scene_length_beats, 0.0);
      entry.has_end = entry.quantized_end_beats > entry.quantized_start_beats;
    }
  }

  std::stable_sort(resolved_timeline.begin(), resolved_timeline.end(),
                   [](const SceneTimelineEntry &lhs,
                      const SceneTimelineEntry &rhs) {
                     if (lhs.quantized_start_beats < rhs.quantized_start_beats) {
                       return true;
                     }
                     if (rhs.quantized_start_beats < lhs.quantized_start_beats) {
                       return false;
                     }
                     return lhs.scene_index < rhs.scene_index;
                   });

  for (const SceneTimelineEntry &entry : resolved_timeline) {
    const double scene_end = entry.has_end
                                 ? entry.quantized_end_beats
                                 : entry.quantized_start_beats +
                                       std::max(fallback_scene_length_beats, 0.0);
    for (const auto &track : tracks_) {
      for (const auto &clip : track->clips()) {
        if (clip->scene_index() != entry.scene_index) {
          continue;
        }
        CommittedClip committed;
        committed.clip = clip.get();
        committed.scene_index = entry.scene_index;
        committed.arranged_start_beats = entry.quantized_start_beats;
        const double max_length = scene_end - entry.quantized_start_beats;
        committed.arranged_length_beats =
            std::max(kMinimumLengthBeats,
                     std::min(clip->length(), std::max(max_length, 0.0)));
        committed_clips_.push_back(committed);
      }
    }
  }

  update_session_range_from_commits();
  scene_timeline_.clear();
  active_scenes_.clear();
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

Track *SessionGraph::find_clip_track(const Clip *clip) {
  for (const auto &track : tracks_) {
    if (track->find_clip(clip) != nullptr) {
      return track.get();
    }
  }
  return nullptr;
}

Clip *SessionGraph::find_clip(const Clip *clip) {
  Track *owner = find_clip_track(clip);
  if (owner == nullptr) {
    return nullptr;
  }
  return owner->find_clip(clip);
}

}  // namespace orpheus::core
