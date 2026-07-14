// SPDX-License-Identifier: MIT
#include "orpheus/session_graph.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace orpheus::core {

namespace {
constexpr double kMinimumLengthBeats = 1e-6;
constexpr double kClipOrderingTolerance = 1e-9;

TimeRange RangeFromBeats(double start_beats, double length_beats, double tempo_bpm,
                         std::uint32_t sample_rate) {
  const TimePoint start = TimePoint::fromBeats(start_beats, tempo_bpm, sample_rate);
  const TimePoint end = TimePoint::fromBeats(start_beats + length_beats, tempo_bpm, sample_rate);
  return TimeRange::fromStartEnd(start, end);
}

double BeatsFromTime(TimePoint point, double tempo_bpm, std::uint32_t sample_rate) {
  return point.toBeats(tempo_bpm, sample_rate);
}
} // namespace

Clip::Clip(ClipId id, std::string name, double start_beats, double length_beats,
           std::uint32_t scene_index)
    : id_(id), name_(std::move(name)), start_beats_(start_beats),
      length_beats_(std::max(length_beats, kMinimumLengthBeats)), scene_index_(scene_index) {}

Clip::~Clip() = default;

void Clip::set_start(double start_beats) {
  start_beats_ = start_beats;
}

void Clip::set_length(double length_beats) {
  length_beats_ = std::max(length_beats, kMinimumLengthBeats);
}

void Clip::set_scene_index(std::uint32_t scene_index) {
  scene_index_ = scene_index;
}

Track::Track(TrackId id, std::string name) : id_(id), name_(std::move(name)) {}

Track::~Track() = default;

Clip* Track::add_clip(ClipId id, std::string name, double start_beats, double length_beats,
                      std::uint32_t scene_index) {
  auto clip =
      std::unique_ptr<Clip>(new Clip(id, std::move(name), start_beats, length_beats, scene_index));
  Clip* raw = clip.get();
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

bool Track::remove_clip(const Clip* clip) {
  const auto it =
      std::find_if(clips_.begin(), clips_.end(),
                   [&](const std::unique_ptr<Clip>& candidate) { return candidate.get() == clip; });
  if (it == clips_.end()) {
    return false;
  }
  clips_.erase(it);
  return true;
}

Clip* Track::find_clip(const Clip* clip) {
  const auto it =
      std::find_if(clips_.begin(), clips_.end(),
                   [&](const std::unique_ptr<Clip>& candidate) { return candidate.get() == clip; });
  if (it == clips_.end()) {
    return nullptr;
  }
  return it->get();
}

void Track::sort_clips() {
  std::sort(clips_.begin(), clips_.end(),
            [](const std::unique_ptr<Clip>& lhs, const std::unique_ptr<Clip>& rhs) {
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
    const Clip& previous = *clips_[index - 1];
    const Clip& current = *clips_[index];
    if (current.start() + kClipOrderingTolerance < previous.start()) {
      throw std::invalid_argument("Clips on track \"" + name_ + "\" must be sorted by start time");
    }
    const double previous_end = previous.start() + previous.length();
    if (previous_end > current.start() + kClipOrderingTolerance) {
      throw std::invalid_argument("Clips on track \"" + name_ + "\" must not overlap");
    }
  }
}

MarkerSet::MarkerSet(std::string name) : name_(std::move(name)) {}

MarkerSet::~MarkerSet() = default;

MarkerSet::Marker* MarkerSet::add_marker(std::string name, double position_beats) {
  auto& marker = markers_.emplace_back();
  marker.name = std::move(name);
  marker.position_beats = position_beats;
  return &marker;
}

bool MarkerSet::remove_marker(const Marker* marker) {
  const auto it = std::find_if(markers_.begin(), markers_.end(),
                               [&](const Marker& candidate) { return &candidate == marker; });
  if (it == markers_.end()) {
    return false;
  }
  markers_.erase(it);
  return true;
}

MarkerSet::Marker* MarkerSet::find_marker(const Marker* marker) {
  const auto it = std::find_if(markers_.begin(), markers_.end(),
                               [&](const Marker& candidate) { return &candidate == marker; });
  if (it == markers_.end()) {
    return nullptr;
  }
  return &(*it);
}

PlaylistLane::PlaylistLane(std::string name, bool is_active)
    : name_(std::move(name)), is_active_(is_active) {}

PlaylistLane::~PlaylistLane() = default;

void PlaylistLane::set_active(bool active) {
  is_active_ = active;
}

SessionGraph::SessionGraph() : SessionGraph(SessionId::fromRaw(1u)) {}

SessionGraph::SessionGraph(SessionId session_id) : session_id_(session_id), name_("Session") {
  if (!session_id.isValid()) {
    throw std::invalid_argument("Session ID must be valid");
  }
}

SessionGraph::~SessionGraph() = default;

SessionGraph::Transaction::Transaction(SessionGraph& graph, SessionGraphSnapshot before)
    : graph_(&graph), before_(std::move(before)), before_change_(graph.last_change_),
      base_revision_(graph.revision_) {}

SessionGraph::Transaction::Transaction(Transaction&& other) noexcept
    : graph_(std::exchange(other.graph_, nullptr)), before_(std::move(other.before_)),
      before_change_(other.before_change_), base_revision_(other.base_revision_) {}

SessionGraph::Transaction& SessionGraph::Transaction::operator=(Transaction&& other) noexcept {
  if (this != &other) {
    rollback();
    graph_ = std::exchange(other.graph_, nullptr);
    before_ = std::move(other.before_);
    before_change_ = other.before_change_;
    base_revision_ = other.base_revision_;
  }
  return *this;
}

SessionGraph::Transaction::~Transaction() {
  rollback();
}

SessionGraphChangeSet SessionGraph::Transaction::commit() {
  if (graph_ == nullptr || !graph_->transaction_active_) {
    throw std::logic_error("Session transaction is not active");
  }

  SessionGraphChangeSet result{base_revision_, base_revision_, graph_->pending_changes_};
  if (!result.empty()) {
    result.revision = ++graph_->revision_;
    graph_->last_change_ = result;
  }
  graph_->pending_changes_ = SessionChangeFlags::None;
  graph_->transaction_active_ = false;
  graph_ = nullptr;
  return result;
}

void SessionGraph::Transaction::rollback() noexcept {
  if (graph_ == nullptr) {
    return;
  }
  if (graph_->transaction_active_) {
    graph_->restore_unchecked(before_);
    graph_->revision_ = base_revision_;
    graph_->last_change_ = before_change_;
    graph_->pending_changes_ = SessionChangeFlags::None;
    graph_->transaction_active_ = false;
  }
  graph_ = nullptr;
}

SessionGraph::Transaction SessionGraph::begin_transaction() {
  if (transaction_active_) {
    throw std::logic_error("Nested session transactions are not supported");
  }
  auto before = snapshot();
  transaction_active_ = true;
  pending_changes_ = SessionChangeFlags::None;
  return Transaction(*this, std::move(before));
}

void SessionGraph::record_change(SessionChangeFlags changes) {
  if (changes == SessionChangeFlags::None) {
    return;
  }
  if (transaction_active_) {
    pending_changes_ |= changes;
    return;
  }
  const std::uint64_t base = revision_;
  last_change_ = SessionGraphChangeSet{base, ++revision_, changes};
}

SessionGraphSnapshot SessionGraph::snapshot() const {
  SessionGraphSnapshot result;
  result.session_id = session_id_;
  result.revision = revision_;
  result.name = name_;
  result.tempo_bpm = tempo_bpm_;
  result.render_sample_rate_hz = render_sample_rate_hz_;
  result.render_bit_depth_bits = render_bit_depth_bits_;
  result.render_dither_enabled = render_dither_enabled_;
  result.session_range =
      RangeFromBeats(session_start_beats_, session_end_beats_ - session_start_beats_, tempo_bpm_,
                     render_sample_rate_hz_);

  result.tracks.reserve(tracks_.size());
  for (const auto& track : tracks_) {
    SessionTrackSnapshot track_snapshot;
    track_snapshot.id = track->id();
    track_snapshot.name = track->name();
    track_snapshot.clips.reserve(track->clips().size());
    for (const auto& clip : track->clips()) {
      track_snapshot.clips.push_back(SessionClipSnapshot{
          clip->id(), track->id(), clip->name(),
          RangeFromBeats(clip->start(), clip->length(), tempo_bpm_, render_sample_rate_hz_),
          clip->scene_index()});
    }
    result.tracks.push_back(std::move(track_snapshot));
  }

  result.clip_assignments = clip_assignment_ids();
  return result;
}

void SessionGraph::restore_unchecked(const SessionGraphSnapshot& state) {
  if (state.schema_version != SessionGraphSnapshot::kSchemaVersion) {
    throw std::invalid_argument("Unsupported session graph snapshot schema");
  }
  if (!state.session_id.isValid() || state.session_id != session_id_) {
    throw std::invalid_argument("Session graph snapshot belongs to a different session");
  }
  if (!(state.tempo_bpm > 0.0) || !std::isfinite(state.tempo_bpm)) {
    throw std::invalid_argument("Snapshot tempo must be finite and positive");
  }
  if (state.render_sample_rate_hz == 0u) {
    throw std::invalid_argument("Snapshot sample rate must be non-zero");
  }
  if (state.render_bit_depth_bits != 16u && state.render_bit_depth_bits != 24u &&
      state.render_bit_depth_bits != 32u) {
    throw std::invalid_argument("Snapshot bit depth is unsupported");
  }

  std::vector<std::unique_ptr<Track>> rebuilt_tracks;
  rebuilt_tracks.reserve(state.tracks.size());
  IdAllocator<TrackId> rebuilt_track_ids;
  IdAllocator<ClipId> rebuilt_clip_ids;
  std::unordered_set<std::uint64_t> seen_track_ids;
  std::unordered_set<std::uint64_t> seen_clip_ids;

  for (const auto& track_state : state.tracks) {
    if (!track_state.id.isValid() || !seen_track_ids.insert(track_state.id.raw()).second) {
      throw std::invalid_argument("Snapshot contains an invalid or duplicate track ID");
    }
    auto track = std::unique_ptr<Track>(new Track(track_state.id, track_state.name));
    rebuilt_track_ids.reserveThrough(track_state.id);
    for (const auto& clip_state : track_state.clips) {
      if (!clip_state.id.isValid() || clip_state.track_id != track_state.id ||
          !seen_clip_ids.insert(clip_state.id.raw()).second || clip_state.range.empty()) {
        throw std::invalid_argument("Snapshot contains an invalid clip");
      }
      const double start_beats =
          BeatsFromTime(clip_state.range.start(), state.tempo_bpm, state.render_sample_rate_hz);
      const double end_beats =
          BeatsFromTime(clip_state.range.end(), state.tempo_bpm, state.render_sample_rate_hz);
      static_cast<void>(track->add_clip(clip_state.id, clip_state.name, start_beats,
                                        end_beats - start_beats, clip_state.scene_index));
      rebuilt_clip_ids.reserveThrough(clip_state.id);
    }
    rebuilt_tracks.push_back(std::move(track));
  }

  std::vector<std::uint64_t> rebuilt_assignments;
  rebuilt_assignments.reserve(state.clip_assignments.size());
  for (const ClipId assignment : state.clip_assignments) {
    if (!assignment.isValid()) {
      throw std::invalid_argument("Snapshot contains an invalid clip assignment");
    }
    rebuilt_assignments.push_back(assignment.raw());
  }

  const double session_start =
      BeatsFromTime(state.session_range.start(), state.tempo_bpm, state.render_sample_rate_hz);
  const double session_end =
      BeatsFromTime(state.session_range.end(), state.tempo_bpm, state.render_sample_rate_hz);

  track_ids_ = rebuilt_track_ids;
  clip_ids_ = rebuilt_clip_ids;
  name_ = state.name;
  tempo_bpm_ = state.tempo_bpm;
  render_sample_rate_hz_ = state.render_sample_rate_hz;
  render_bit_depth_bits_ = state.render_bit_depth_bits;
  render_dither_enabled_ = state.render_dither_enabled;
  session_start_beats_ = session_start;
  session_end_beats_ = session_end;
  tracks_ = std::move(rebuilt_tracks);
  clip_assignments_ = std::move(rebuilt_assignments);
  clip_grid_dirty_ = false;
  scene_timeline_.clear();
  active_scenes_.clear();
  committed_clips_.clear();
}

void SessionGraph::restore(const SessionGraphSnapshot& state) {
  restore_unchecked(state);
  record_change(SessionChangeFlags::Metadata | SessionChangeFlags::Structure |
                SessionChangeFlags::Timing | SessionChangeFlags::ClipAssignments |
                SessionChangeFlags::RenderSettings);
}

TrackId SessionGraph::create_track(std::string name) {
  return add_track(std::move(name))->id();
}

bool SessionGraph::remove_track(TrackId track_id) {
  Track* track = find_track(track_id);
  return track != nullptr && remove_track(track);
}

ClipId SessionGraph::create_clip(TrackId track_id, std::string name, TimeRange range,
                                 std::uint32_t scene_index) {
  if (range.empty()) {
    throw std::invalid_argument("Clip range must not be empty");
  }
  Track* track = find_track(track_id);
  if (track == nullptr) {
    throw std::invalid_argument("Track ID does not belong to session");
  }
  const double start_beats = BeatsFromTime(range.start(), tempo_bpm_, render_sample_rate_hz_);
  const double end_beats = BeatsFromTime(range.end(), tempo_bpm_, render_sample_rate_hz_);
  return add_clip(*track, std::move(name), start_beats, end_beats - start_beats, scene_index)->id();
}

bool SessionGraph::remove_clip(ClipId clip_id) {
  Clip* clip = find_clip(clip_id);
  return clip != nullptr && remove_clip(clip);
}

void SessionGraph::set_clip_range(ClipId clip_id, TimeRange range) {
  if (range.empty()) {
    throw std::invalid_argument("Clip range must not be empty");
  }
  Clip* target = find_clip(clip_id);
  Track* owner = find_clip_track(target);
  if (target == nullptr || owner == nullptr) {
    throw std::invalid_argument("Clip ID does not belong to session");
  }
  const double previous_start = target->start();
  const double previous_length = target->length();
  const double start_beats = BeatsFromTime(range.start(), tempo_bpm_, render_sample_rate_hz_);
  const double end_beats = BeatsFromTime(range.end(), tempo_bpm_, render_sample_rate_hz_);
  target->set_start(start_beats);
  target->set_length(end_beats - start_beats);
  owner->sort_clips();
  try {
    owner->validate_clip_layout();
  } catch (...) {
    target->set_start(previous_start);
    target->set_length(previous_length);
    owner->sort_clips();
    throw;
  }
  mark_clip_grid_dirty();
  record_change(SessionChangeFlags::Timing);
}

void SessionGraph::set_clip_scene(ClipId clip_id, std::uint32_t scene_index) {
  Clip* clip = find_clip(clip_id);
  if (clip == nullptr) {
    throw std::invalid_argument("Clip ID does not belong to session");
  }
  set_clip_scene(*clip, scene_index);
}

void SessionGraph::set_name(std::string name) {
  name_ = std::move(name);
  record_change(SessionChangeFlags::Metadata);
}

void SessionGraph::set_clip_assignments(std::vector<std::uint64_t> assignments) {
  clip_assignments_ = std::move(assignments);
  record_change(SessionChangeFlags::ClipAssignments);
}

std::vector<ClipId> SessionGraph::clip_assignment_ids() const {
  std::vector<ClipId> result;
  result.reserve(clip_assignments_.size());
  for (const std::uint64_t raw : clip_assignments_) {
    result.push_back(ClipId::fromRaw(raw));
  }
  return result;
}

void SessionGraph::set_clip_assignment_ids(std::vector<ClipId> assignments) {
  std::vector<std::uint64_t> raw;
  raw.reserve(assignments.size());
  for (const ClipId assignment : assignments) {
    if (!assignment.isValid()) {
      throw std::invalid_argument("Clip assignment ID must be valid");
    }
    raw.push_back(assignment.raw());
  }
  set_clip_assignments(std::move(raw));
}

Track* SessionGraph::add_track(std::string name) {
  const TrackId id = track_ids_.allocate();
  auto& slot = tracks_.emplace_back(std::unique_ptr<Track>(new Track(id, std::move(name))));
  mark_clip_grid_dirty();
  record_change(SessionChangeFlags::Structure);
  return slot.get();
}

bool SessionGraph::remove_track(const Track* track) {
  const auto it =
      std::find_if(tracks_.begin(), tracks_.end(), [&](const std::unique_ptr<Track>& candidate) {
        return candidate.get() == track;
      });
  if (it == tracks_.end()) {
    return false;
  }
  tracks_.erase(it);
  mark_clip_grid_dirty();
  record_change(SessionChangeFlags::Structure);
  return true;
}

MarkerSet* SessionGraph::add_marker_set(std::string name) {
  auto& slot = marker_sets_.emplace_back(std::make_unique<MarkerSet>(std::move(name)));
  return slot.get();
}

PlaylistLane* SessionGraph::add_playlist_lane(std::string name, bool is_active) {
  auto& slot =
      playlist_lanes_.emplace_back(std::make_unique<PlaylistLane>(std::move(name), is_active));
  return slot.get();
}

void SessionGraph::set_tempo(double bpm) {
  if (bpm <= 0.0 || !std::isfinite(bpm)) {
    throw std::invalid_argument("Tempo must be finite and positive");
  }
  tempo_bpm_ = bpm;
  record_change(SessionChangeFlags::Timing);
}

void SessionGraph::set_render_sample_rate(std::uint32_t sample_rate_hz) {
  if (sample_rate_hz == 0u) {
    throw std::invalid_argument("Sample rate must be non-zero");
  }
  render_sample_rate_hz_ = sample_rate_hz;
  record_change(SessionChangeFlags::RenderSettings);
}

void SessionGraph::set_render_bit_depth(std::uint16_t bit_depth_bits) {
  switch (bit_depth_bits) {
  case 16:
  case 24:
  case 32:
    render_bit_depth_bits_ = bit_depth_bits;
    record_change(SessionChangeFlags::RenderSettings);
    return;
  default:
    throw std::invalid_argument("Unsupported bit depth");
  }
}

void SessionGraph::set_render_dither(bool enabled) {
  render_dither_enabled_ = enabled;
  record_change(SessionChangeFlags::RenderSettings);
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
  record_change(SessionChangeFlags::Timing);
}

Clip* SessionGraph::add_clip(Track& track, std::string name, double start_beats,
                             double length_beats, std::uint32_t scene_index) {
  Track* target = find_track(&track);
  if (target == nullptr) {
    throw std::invalid_argument("Track does not belong to session");
  }
  Clip* clip = target->add_clip(clip_ids_.allocate(), std::move(name), start_beats, length_beats,
                                scene_index);
  mark_clip_grid_dirty();
  record_change(SessionChangeFlags::Structure);
  return clip;
}

bool SessionGraph::remove_clip(const Clip* clip) {
  for (const auto& track : tracks_) {
    if (track->remove_clip(clip)) {
      mark_clip_grid_dirty();
      record_change(SessionChangeFlags::Structure);
      return true;
    }
  }
  return false;
}

void SessionGraph::set_clip_start(Clip& clip, double start_beats) {
  Track* owner = find_clip_track(&clip);
  if (owner == nullptr) {
    throw std::invalid_argument("Clip does not belong to session");
  }
  Clip* target = owner->find_clip(&clip);
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
  record_change(SessionChangeFlags::Timing);
}

void SessionGraph::set_clip_length(Clip& clip, double length_beats) {
  Track* owner = find_clip_track(&clip);
  if (owner == nullptr) {
    throw std::invalid_argument("Clip does not belong to session");
  }
  Clip* target = owner->find_clip(&clip);
  const double previous_length = target->length();
  target->set_length(length_beats);
  try {
    owner->validate_clip_layout();
  } catch (...) {
    target->set_length(previous_length);
    throw;
  }
  mark_clip_grid_dirty();
  record_change(SessionChangeFlags::Timing);
}

void SessionGraph::set_clip_scene(Clip& clip, std::uint32_t scene_index) {
  Clip* target = find_clip(&clip);
  if (target == nullptr) {
    throw std::invalid_argument("Clip does not belong to session");
  }
  target->set_scene_index(scene_index);
  mark_clip_grid_dirty();
  record_change(SessionChangeFlags::Metadata);
}

void SessionGraph::commit_clip_grid() {
  if (!clip_grid_dirty_) {
    return;
  }
  clip_grid_dirty_ = false;
  std::stable_sort(tracks_.begin(), tracks_.end(),
                   [](const std::unique_ptr<Track>& lhs, const std::unique_ptr<Track>& rhs) {
                     return lhs->name() < rhs->name();
                   });

  double min_start = std::numeric_limits<double>::infinity();
  double max_end = std::numeric_limits<double>::lowest();
  bool has_clips = false;

  for (const auto& track : tracks_) {
    track->sort_clips();
    track->validate_clip_layout();
    for (const auto& clip : track->clips()) {
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

double SessionGraph::QuantizePosition(double position_beats, const QuantizationWindow& quantization,
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

void SessionGraph::trigger_scene(std::uint32_t scene_index, double position_beats,
                                 const QuantizationWindow& quantization) {
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
                             const QuantizationWindow& quantization) {
  const auto active = active_scenes_.find(scene_index);
  if (active == active_scenes_.end()) {
    throw std::invalid_argument("Scene has not been triggered");
  }
  SceneTimelineEntry& entry = scene_timeline_[active->second.timeline_index];
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
  for (const auto& clip : committed_clips_) {
    min_start = std::min(min_start, clip.arranged_start_beats);
    max_end = std::max(max_end, clip.arranged_start_beats + clip.arranged_length_beats);
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

  for (auto& entry : resolved_timeline) {
    if (!entry.has_end) {
      entry.quantized_end_beats =
          entry.quantized_start_beats + std::max(fallback_scene_length_beats, 0.0);
      entry.has_end = entry.quantized_end_beats > entry.quantized_start_beats;
    }
  }

  std::stable_sort(resolved_timeline.begin(), resolved_timeline.end(),
                   [](const SceneTimelineEntry& lhs, const SceneTimelineEntry& rhs) {
                     if (lhs.quantized_start_beats < rhs.quantized_start_beats) {
                       return true;
                     }
                     if (rhs.quantized_start_beats < lhs.quantized_start_beats) {
                       return false;
                     }
                     return lhs.scene_index < rhs.scene_index;
                   });

  for (const SceneTimelineEntry& entry : resolved_timeline) {
    const double scene_end =
        entry.has_end ? entry.quantized_end_beats
                      : entry.quantized_start_beats + std::max(fallback_scene_length_beats, 0.0);
    for (const auto& track : tracks_) {
      for (const auto& clip : track->clips()) {
        if (clip->scene_index() != entry.scene_index) {
          continue;
        }
        CommittedClip committed;
        committed.clip = clip.get();
        committed.scene_index = entry.scene_index;
        committed.arranged_start_beats = entry.quantized_start_beats;
        const double max_length = scene_end - entry.quantized_start_beats;
        committed.arranged_length_beats =
            std::max(kMinimumLengthBeats, std::min(clip->length(), std::max(max_length, 0.0)));
        committed_clips_.push_back(committed);
      }
    }
  }

  update_session_range_from_commits();
  scene_timeline_.clear();
  active_scenes_.clear();
}

Track* SessionGraph::find_track(const Track* track) {
  const auto it =
      std::find_if(tracks_.begin(), tracks_.end(), [&](const std::unique_ptr<Track>& candidate) {
        return candidate.get() == track;
      });
  if (it == tracks_.end()) {
    return nullptr;
  }
  return it->get();
}

Track* SessionGraph::find_track(TrackId track_id) {
  const auto it =
      std::find_if(tracks_.begin(), tracks_.end(), [&](const std::unique_ptr<Track>& candidate) {
        return candidate->id() == track_id;
      });
  return it == tracks_.end() ? nullptr : it->get();
}

Track* SessionGraph::find_clip_track(const Clip* clip) {
  for (const auto& track : tracks_) {
    if (track->find_clip(clip) != nullptr) {
      return track.get();
    }
  }
  return nullptr;
}

Clip* SessionGraph::find_clip(const Clip* clip) {
  Track* owner = find_clip_track(clip);
  if (owner == nullptr) {
    return nullptr;
  }
  return owner->find_clip(clip);
}

Clip* SessionGraph::find_clip(ClipId clip_id) {
  for (const auto& track : tracks_) {
    const auto it = std::find_if(
        track->clips().begin(), track->clips().end(),
        [&](const std::unique_ptr<Clip>& candidate) { return candidate->id() == clip_id; });
    if (it != track->clips().end()) {
      return it->get();
    }
  }
  return nullptr;
}

} // namespace orpheus::core
