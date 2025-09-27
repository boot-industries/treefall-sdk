// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace orpheus::reaper {

struct PanelMarker {
  std::string name;
  double position_beats{0.0};
};

struct MarkerSetSnapshot {
  std::string name;
  std::vector<PanelMarker> markers;
};

struct PlaylistLaneSnapshot {
  std::string name;
  bool is_active{false};
};

struct PanelSnapshot {
  bool visible{true};
  std::string session_name{"-"};
  std::size_t track_count{0};
  std::size_t clip_count{0};
  double tempo_bpm{120.0};
  std::uint32_t bars{4};
  std::string last_render_path{"-"};
  std::string status_line{"Ready"};
  std::vector<MarkerSetSnapshot> marker_sets;
  std::vector<PlaylistLaneSnapshot> playlist_lanes;
};

std::string BuildPanelText(const PanelSnapshot &snapshot);
const char *PanelTitle();

}  // namespace orpheus::reaper
