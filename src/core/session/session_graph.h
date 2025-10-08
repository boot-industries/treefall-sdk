// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include <unordered_map>

#include "orpheus/export.h"

namespace orpheus::core {

class Clip {
 public:
  ORPHEUS_API Clip(std::string name, double start_beats, double length_beats,
                   std::uint32_t scene_index = 0u);
  ORPHEUS_API ~Clip();

  ORPHEUS_API void set_start(double start_beats);
  ORPHEUS_API void set_length(double length_beats);
  ORPHEUS_API void set_scene_index(std::uint32_t scene_index);

  [[nodiscard]] double start() const { return start_beats_; }
  [[nodiscard]] double length() const { return length_beats_; }
  [[nodiscard]] const std::string &name() const { return name_; }
  [[nodiscard]] std::uint32_t scene_index() const { return scene_index_; }

 private:
  std::string name_;
  double start_beats_;
  double length_beats_;
  std::uint32_t scene_index_;
};

class Track {
 public:
  ORPHEUS_API explicit Track(std::string name);
  ORPHEUS_API ~Track();

  Track(const Track &) = delete;
  Track &operator=(const Track &) = delete;
  ORPHEUS_API Track(Track &&) noexcept = default;
  ORPHEUS_API Track &operator=(Track &&) noexcept = default;

  [[nodiscard]] const std::string &name() const { return name_; }

  [[nodiscard]] ORPHEUS_API Clip *add_clip(std::string name, double start_beats,
                                           double length_beats,
                                           std::uint32_t scene_index = 0u);
  [[nodiscard]] ORPHEUS_API bool remove_clip(const Clip *clip);
  [[nodiscard]] ORPHEUS_API Clip *find_clip(const Clip *clip);

  [[nodiscard]] const std::vector<std::unique_ptr<Clip>> &clips() const {
    return clips_;
  }

  [[nodiscard]] std::vector<std::unique_ptr<Clip>>::const_iterator clips_begin()
      const {
    return clips_.begin();
  }

  [[nodiscard]] std::vector<std::unique_ptr<Clip>>::const_iterator clips_end()
      const {
    return clips_.end();
  }

  ORPHEUS_API void sort_clips();

 private:
  void validate_clip_layout() const;

  std::string name_;
  std::vector<std::unique_ptr<Clip>> clips_;

  friend class SessionGraph;
};

class MarkerSet {
 public:
  struct Marker {
    std::string name;
    double position_beats{0.0};
  };

  ORPHEUS_API explicit MarkerSet(std::string name);
  ORPHEUS_API ~MarkerSet();

  [[nodiscard]] const std::string &name() const { return name_; }

  [[nodiscard]] ORPHEUS_API Marker *add_marker(std::string name,
                                              double position_beats);
  [[nodiscard]] ORPHEUS_API bool remove_marker(const Marker *marker);
  [[nodiscard]] ORPHEUS_API Marker *find_marker(const Marker *marker);

  [[nodiscard]] const std::vector<Marker> &markers() const {
    return markers_;
  }

  [[nodiscard]] std::vector<Marker>::const_iterator markers_begin() const {
    return markers_.begin();
  }

  [[nodiscard]] std::vector<Marker>::const_iterator markers_end() const {
    return markers_.end();
  }

 private:
  std::string name_;
  std::vector<Marker> markers_;
};

class PlaylistLane {
 public:
  ORPHEUS_API explicit PlaylistLane(std::string name, bool is_active = false);
  ORPHEUS_API ~PlaylistLane();

  [[nodiscard]] const std::string &name() const { return name_; }
  [[nodiscard]] bool is_active() const { return is_active_; }

  ORPHEUS_API void set_active(bool active);

 private:
  std::string name_;
  bool is_active_{false};
};

struct ORPHEUS_API TransportState {
  double tempo_bpm{120.0};
  double position_beats{0.0};
  bool is_playing{false};
};

struct ORPHEUS_API QuantizationWindow {
  double grid_beats{1.0};
  double tolerance_beats{0.0};
};

struct ORPHEUS_API CommittedClip {
  const Clip *clip{nullptr};
  std::uint32_t scene_index{0u};
  double arranged_start_beats{0.0};
  double arranged_length_beats{0.0};
};

class SessionGraph {
 public:
  ORPHEUS_API SessionGraph();
  ORPHEUS_API ~SessionGraph();

  SessionGraph(const SessionGraph &) = delete;
  SessionGraph &operator=(const SessionGraph &) = delete;
  ORPHEUS_API SessionGraph(SessionGraph &&) noexcept = default;
  ORPHEUS_API SessionGraph &operator=(SessionGraph &&) noexcept = default;

  ORPHEUS_API void set_name(std::string name);
  [[nodiscard]] const std::string &name() const { return name_; }

  [[nodiscard]] ORPHEUS_API Track *add_track(std::string name);
  [[nodiscard]] ORPHEUS_API bool remove_track(const Track *track);

  [[nodiscard]] ORPHEUS_API MarkerSet *add_marker_set(std::string name);
  [[nodiscard]] ORPHEUS_API PlaylistLane *add_playlist_lane(
      std::string name, bool is_active = false);

  ORPHEUS_API void set_tempo(double bpm);
  [[nodiscard]] double tempo() const { return tempo_bpm_; }

  ORPHEUS_API void set_render_sample_rate(std::uint32_t sample_rate_hz);
  ORPHEUS_API void set_render_bit_depth(std::uint16_t bit_depth_bits);
  ORPHEUS_API void set_render_dither(bool enabled);

  [[nodiscard]] std::uint32_t render_sample_rate() const {
    return render_sample_rate_hz_;
  }
  [[nodiscard]] std::uint16_t render_bit_depth() const {
    return render_bit_depth_bits_;
  }
  [[nodiscard]] bool render_dither() const { return render_dither_enabled_; }

  [[nodiscard]] ORPHEUS_API TransportState transport_state() const;

  ORPHEUS_API void set_session_range(double start_beats, double end_beats);
  [[nodiscard]] double session_start_beats() const {
    return session_start_beats_;
  }
  [[nodiscard]] double session_end_beats() const {
    return session_end_beats_;
  }

  [[nodiscard]] ORPHEUS_API Clip *add_clip(Track &track, std::string name,
                                          double start_beats,
                                          double length_beats,
                                          std::uint32_t scene_index = 0u);
  [[nodiscard]] ORPHEUS_API bool remove_clip(const Clip *clip);
  ORPHEUS_API void set_clip_start(Clip &clip, double start_beats);
  ORPHEUS_API void set_clip_length(Clip &clip, double length_beats);
  ORPHEUS_API void set_clip_scene(Clip &clip, std::uint32_t scene_index);
  ORPHEUS_API void commit_clip_grid();

  ORPHEUS_API void trigger_scene(std::uint32_t scene_index,
                                 double position_beats,
                                 const QuantizationWindow &quantization);
  ORPHEUS_API void end_scene(std::uint32_t scene_index, double position_beats,
                             const QuantizationWindow &quantization);
  ORPHEUS_API void commit_arrangement(
      double fallback_scene_length_beats = 0.0);

  [[nodiscard]] const std::vector<CommittedClip> &committed_clips() const {
    return committed_clips_;
  }

  [[nodiscard]] const std::vector<std::unique_ptr<Track>> &tracks() const {
    return tracks_;
  }

  [[nodiscard]] std::vector<std::unique_ptr<Track>>::const_iterator tracks_begin()
      const {
    return tracks_.begin();
  }

  [[nodiscard]] std::vector<std::unique_ptr<Track>>::const_iterator tracks_end()
      const {
    return tracks_.end();
  }

  [[nodiscard]] const std::vector<std::unique_ptr<MarkerSet>> &marker_sets()
      const {
    return marker_sets_;
  }

  [[nodiscard]] std::vector<std::unique_ptr<MarkerSet>>::const_iterator
  marker_sets_begin() const {
    return marker_sets_.begin();
  }

  [[nodiscard]] std::vector<std::unique_ptr<MarkerSet>>::const_iterator
  marker_sets_end() const {
    return marker_sets_.end();
  }

  [[nodiscard]] const std::vector<std::unique_ptr<PlaylistLane>> &
  playlist_lanes() const {
    return playlist_lanes_;
  }

  [[nodiscard]] std::vector<std::unique_ptr<PlaylistLane>>::const_iterator
  playlist_lanes_begin() const {
    return playlist_lanes_.begin();
  }

  [[nodiscard]] std::vector<std::unique_ptr<PlaylistLane>>::const_iterator
  playlist_lanes_end() const {
    return playlist_lanes_.end();
  }

 private:
  Track *find_track(const Track *track);
  Track *find_clip_track(const Clip *clip);
  Clip *find_clip(const Clip *clip);
  void mark_clip_grid_dirty() { clip_grid_dirty_ = true; }
  void update_session_range_from_commits();
  static double QuantizePosition(double position_beats,
                                 const QuantizationWindow &quantization,
                                 double minimum_beats);

  struct SceneTimelineEntry {
    std::uint32_t scene_index{0u};
    double trigger_position_beats{0.0};
    QuantizationWindow trigger_quantization{};
    double quantized_start_beats{0.0};
    bool has_end{false};
    double end_position_beats{0.0};
    QuantizationWindow end_quantization{};
    double quantized_end_beats{0.0};
  };

  struct ActiveScene {
    std::size_t timeline_index{0};
  };

  double tempo_bpm_{120.0};
  double transport_position_beats_{0.0};
  bool transport_is_playing_{false};
  std::string name_;
  double session_start_beats_{0.0};
  double session_end_beats_{0.0};
  std::vector<std::unique_ptr<Track>> tracks_;
  bool clip_grid_dirty_{false};
  std::uint32_t render_sample_rate_hz_{48000u};
  std::uint16_t render_bit_depth_bits_{24u};
  bool render_dither_enabled_{true};
  std::vector<SceneTimelineEntry> scene_timeline_;
  std::unordered_map<std::uint32_t, ActiveScene> active_scenes_;
  std::vector<CommittedClip> committed_clips_;
  std::vector<std::unique_ptr<MarkerSet>> marker_sets_;
  std::vector<std::unique_ptr<PlaylistLane>> playlist_lanes_;
};

}  // namespace orpheus::core

static_assert(!std::is_copy_constructible_v<orpheus::core::Track>);
static_assert(!std::is_copy_assignable_v<orpheus::core::Track>);
static_assert(!std::is_copy_constructible_v<orpheus::core::SessionGraph>);
static_assert(!std::is_copy_assignable_v<orpheus::core::SessionGraph>);

static_assert(std::is_same_v<decltype(std::declval<const orpheus::core::Track &>().clips()),
                             const std::vector<std::unique_ptr<orpheus::core::Clip>> &>);

static_assert(
    std::is_same_v<decltype(std::declval<const orpheus::core::SessionGraph &>().tracks()),
                   const std::vector<std::unique_ptr<orpheus::core::Track>> &>);

static_assert(std::is_same_v<decltype(std::declval<const orpheus::core::MarkerSet &>().markers()),
                             const std::vector<orpheus::core::MarkerSet::Marker> &>);

static_assert(std::is_same_v<decltype(std::declval<const orpheus::core::SessionGraph &>().marker_sets()),
                   const std::vector<std::unique_ptr<orpheus::core::MarkerSet>> &>);

static_assert(std::is_same_v<decltype(std::declval<const orpheus::core::SessionGraph &>().playlist_lanes()),
                   const std::vector<std::unique_ptr<orpheus::core::PlaylistLane>> &>);
