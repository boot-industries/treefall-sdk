// SPDX-License-Identifier: MIT
#pragma once

#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "orpheus/export.h"

namespace orpheus::core {

class ORPHEUS_API Clip {
 public:
  Clip(std::string name, double start_beats, double length_beats);

  void set_start(double start_beats);
  void set_length(double length_beats);

  [[nodiscard]] double start() const { return start_beats_; }
  [[nodiscard]] double length() const { return length_beats_; }
  [[nodiscard]] const std::string &name() const { return name_; }

 private:
  std::string name_;
  double start_beats_;
  double length_beats_;
};

class ORPHEUS_API Track {
 public:
  explicit Track(std::string name);

  Track(const Track &) = delete;
  Track &operator=(const Track &) = delete;
  Track(Track &&) noexcept = default;
  Track &operator=(Track &&) noexcept = default;

  [[nodiscard]] const std::string &name() const { return name_; }

  Clip *add_clip(std::string name, double start_beats, double length_beats);
  bool remove_clip(const Clip *clip);
  Clip *find_clip(const Clip *clip);

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

  void sort_clips();

 private:
  std::string name_;
  std::vector<std::unique_ptr<Clip>> clips_;
};

struct ORPHEUS_API TransportState {
  double tempo_bpm{120.0};
  double position_beats{0.0};
  bool is_playing{false};
};

class ORPHEUS_API SessionGraph {
 public:
  SessionGraph();

  SessionGraph(const SessionGraph &) = delete;
  SessionGraph &operator=(const SessionGraph &) = delete;
  SessionGraph(SessionGraph &&) noexcept = default;
  SessionGraph &operator=(SessionGraph &&) noexcept = default;

  void set_name(std::string name);
  [[nodiscard]] const std::string &name() const { return name_; }

  Track *add_track(std::string name);
  bool remove_track(const Track *track);

  void set_tempo(double bpm);
  [[nodiscard]] double tempo() const { return tempo_bpm_; }

  [[nodiscard]] TransportState transport_state() const;

  void set_session_range(double start_beats, double end_beats);
  [[nodiscard]] double session_start_beats() const {
    return session_start_beats_;
  }
  [[nodiscard]] double session_end_beats() const {
    return session_end_beats_;
  }

  Clip *add_clip(Track &track, std::string name, double start_beats,
                double length_beats);
  bool remove_clip(const Clip *clip);
  void set_clip_start(Clip &clip, double start_beats);
  void set_clip_length(Clip &clip, double length_beats);
  void commit_clip_grid();

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

 private:
  Track *find_track(const Track *track);
  Clip *find_clip(const Clip *clip);
  void mark_clip_grid_dirty() { clip_grid_dirty_ = true; }

  double tempo_bpm_{120.0};
  double transport_position_beats_{0.0};
  bool transport_is_playing_{false};
  std::string name_;
  double session_start_beats_{0.0};
  double session_end_beats_{0.0};
  std::vector<std::unique_ptr<Track>> tracks_;
  bool clip_grid_dirty_{false};
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
