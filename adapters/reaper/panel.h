#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace orpheus::reaper {

struct PanelSnapshot {
  bool visible{true};
  std::string session_name{"-"};
  std::size_t track_count{0};
  std::size_t clip_count{0};
  double tempo_bpm{120.0};
  std::uint32_t bars{4};
  std::string last_render_path{"-"};
  std::string status_line{"Ready"};
};

std::string BuildPanelText(const PanelSnapshot &snapshot);
const char *PanelTitle();

}  // namespace orpheus::reaper
