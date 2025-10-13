// SPDX-License-Identifier: MIT
#include "panel.h"

#include "orpheus/abi.h"

#include <iomanip>
#include <sstream>

namespace orpheus::reaper {

std::string BuildPanelText(const PanelSnapshot& snapshot) {
  std::ostringstream stream;
  stream << "Orpheus Adapter\n";
  stream << "ABI Version: " << ToString(kSessionAbi) << "\n";
  stream << "Panel: " << (snapshot.visible ? "Visible" : "Hidden") << "\n";
  stream << "Session: " << snapshot.session_name << " (Tracks: " << snapshot.track_count
         << " Clips: " << snapshot.clip_count << ")\n";
  stream << "Bars: " << snapshot.bars << "\n";
  stream << std::fixed << std::setprecision(2);
  stream << "Tempo: " << snapshot.tempo_bpm << " BPM\n";
  stream << "Marker Sets: " << snapshot.marker_sets.size() << "\n";
  for (const auto& set : snapshot.marker_sets) {
    stream << "  - " << set.name << " (" << set.markers.size() << ")";
    if (!set.markers.empty()) {
      stream << ": ";
      for (std::size_t index = 0; index < set.markers.size(); ++index) {
        const auto& marker = set.markers[index];
        if (index != 0) {
          stream << ", ";
        }
        stream << marker.name << "@" << marker.position_beats;
      }
    }
    stream << "\n";
  }
  stream << "Playlist Lanes: " << snapshot.playlist_lanes.size() << "\n";
  for (const auto& lane : snapshot.playlist_lanes) {
    stream << "  - " << lane.name;
    if (lane.is_active) {
      stream << " [active]";
    }
    stream << "\n";
  }
  const std::string& render_path =
      snapshot.last_render_path.empty() ? std::string("-") : snapshot.last_render_path;
  stream << "Last Render: " << render_path << "\n";
  const std::string& status =
      snapshot.status_line.empty() ? std::string("Ready") : snapshot.status_line;
  stream << "Status: " << status;
  return stream.str();
}

const char* PanelTitle() {
  return "Orpheus";
}

} // namespace orpheus::reaper
