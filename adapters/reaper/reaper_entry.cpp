// SPDX-License-Identifier: MIT
#include "panel.h"

#include "json_io.h"
#include "orpheus/abi.h"
#include "orpheus/adapters/reaper/entry.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <string>

namespace session_json = orpheus::core::session_json;
using orpheus::core::SessionGraph;
using orpheus::reaper::MarkerSetSnapshot;
using orpheus::reaper::PanelMarker;
using orpheus::reaper::PlaylistLaneSnapshot;

namespace {

std::mutex gStateMutex;
orpheus::reaper::PanelSnapshot gSnapshot;
std::string gPanelText;
constexpr std::uint32_t kBeatsPerBar = 4;

const orpheus_session_api_v1 *SessionAbi() {
  uint32_t major = 0;
  uint32_t minor = 0;
  const auto *api =
      orpheus_session_abi_v1(ORPHEUS_ABI_V1_MAJOR, &major, &minor);
  if (api == nullptr || major != ORPHEUS_ABI_V1_MAJOR ||
      minor != ORPHEUS_ABI_V1_MINOR) {
    return nullptr;
  }
  return api;
}

const orpheus_clipgrid_api_v1 *ClipgridAbi() {
  uint32_t major = 0;
  uint32_t minor = 0;
  const auto *api =
      orpheus_clipgrid_abi_v1(ORPHEUS_ABI_V1_MAJOR, &major, &minor);
  if (api == nullptr || major != ORPHEUS_ABI_V1_MAJOR ||
      minor != ORPHEUS_ABI_V1_MINOR) {
    return nullptr;
  }
  return api;
}

const orpheus_render_api_v1 *RenderAbi() {
  uint32_t major = 0;
  uint32_t minor = 0;
  const auto *api =
      orpheus_render_abi_v1(ORPHEUS_ABI_V1_MAJOR, &major, &minor);
  if (api == nullptr || major != ORPHEUS_ABI_V1_MAJOR ||
      minor != ORPHEUS_ABI_V1_MINOR) {
    return nullptr;
  }
  return api;
}

std::string StatusToString(orpheus_status status) {
  switch (status) {
    case ORPHEUS_STATUS_OK:
      return "ok";
    case ORPHEUS_STATUS_INVALID_ARGUMENT:
      return "invalid argument";
    case ORPHEUS_STATUS_NOT_FOUND:
      return "not found";
    case ORPHEUS_STATUS_OUT_OF_MEMORY:
      return "out of memory";
    case ORPHEUS_STATUS_INTERNAL_ERROR:
      return "internal error";
    case ORPHEUS_STATUS_NOT_IMPLEMENTED:
      return "not implemented";
    case ORPHEUS_STATUS_IO_ERROR:
      return "io error";
  }
  return "unknown";
}

void RefreshPanelLocked() { gPanelText = orpheus::reaper::BuildPanelText(gSnapshot); }

struct SessionGuard {
  const orpheus_session_api_v1 *abi{};
  orpheus_session_handle handle{};
  ~SessionGuard() {
    if (abi && handle) {
      abi->destroy(handle);
    }
  }
};

orpheus_status PopulateSession(const SessionGraph &graph,
                               orpheus_session_handle handle,
                               const orpheus_session_api_v1 *session_abi,
                               const orpheus_clipgrid_api_v1 *clipgrid_abi,
                               std::size_t &clip_count) {
  clip_count = 0;
  for (const auto &track_ptr : graph.tracks()) {
    orpheus_track_handle track_handle{};
    const orpheus_track_desc desc{track_ptr->name().c_str()};
    auto status = session_abi->add_track(handle, &desc, &track_handle);
    if (status != ORPHEUS_STATUS_OK) {
      return status;
    }
    for (const auto &clip_ptr : track_ptr->clips()) {
      const orpheus_clip_desc clip_desc{clip_ptr->name().c_str(),
                                        clip_ptr->start(),
                                        clip_ptr->length(),
                                        0u};
      orpheus_clip_handle clip_handle{};
      status = clipgrid_abi->add_clip(handle, track_handle, &clip_desc,
                                      &clip_handle);
      if (status != ORPHEUS_STATUS_OK) {
        return status;
      }
      ++clip_count;
    }
  }
  return clipgrid_abi->commit(handle);
}

bool ImportSessionLocked(const std::string &path, std::string &error) {
  const auto *session_abi = SessionAbi();
  const auto *clipgrid_abi = ClipgridAbi();
  if (!session_abi || !clipgrid_abi) {
    error = "ABI tables unavailable";
    return false;
  }

  SessionGraph graph;
  try {
    graph = session_json::LoadSessionFromFile(path);
  } catch (const std::exception &ex) {
    error = ex.what();
    return false;
  }

  orpheus_session_handle handle{};
  if (session_abi->create(&handle) != ORPHEUS_STATUS_OK) {
    error = "Failed to create session";
    return false;
  }
  SessionGuard guard{session_abi, handle};

  auto status = session_abi->set_tempo(handle, graph.tempo());
  if (status != ORPHEUS_STATUS_OK) {
    error = "Tempo apply failed: " + StatusToString(status);
    return false;
  }

  std::size_t clip_count = 0;
  status = PopulateSession(graph, handle, session_abi, clipgrid_abi, clip_count);
  if (status != ORPHEUS_STATUS_OK) {
    error = "Session import failed: " + StatusToString(status);
    return false;
  }

  gSnapshot.session_name = graph.name();
  gSnapshot.track_count = graph.tracks().size();
  gSnapshot.clip_count = clip_count;
  gSnapshot.tempo_bpm = graph.tempo();
  gSnapshot.marker_sets.clear();
  gSnapshot.marker_sets.reserve(graph.marker_sets().size());
  for (const auto &marker_set_ptr : graph.marker_sets()) {
    MarkerSetSnapshot snapshot_set;
    snapshot_set.name = marker_set_ptr->name();
    snapshot_set.markers.reserve(marker_set_ptr->markers().size());
    for (const auto &marker : marker_set_ptr->markers()) {
      PanelMarker snapshot_marker;
      snapshot_marker.name = marker.name;
      snapshot_marker.position_beats = marker.position_beats;
      snapshot_set.markers.push_back(std::move(snapshot_marker));
    }
    gSnapshot.marker_sets.push_back(std::move(snapshot_set));
  }
  gSnapshot.playlist_lanes.clear();
  gSnapshot.playlist_lanes.reserve(graph.playlist_lanes().size());
  for (const auto &lane_ptr : graph.playlist_lanes()) {
    PlaylistLaneSnapshot lane_snapshot;
    lane_snapshot.name = lane_ptr->name();
    lane_snapshot.is_active = lane_ptr->is_active();
    gSnapshot.playlist_lanes.push_back(std::move(lane_snapshot));
  }
  const double total_beats =
      std::max(0.0, graph.session_end_beats() - graph.session_start_beats());
  const double bars_exact = total_beats / static_cast<double>(kBeatsPerBar);
  gSnapshot.bars = bars_exact <= 0.0
                       ? std::max<std::uint32_t>(1, gSnapshot.bars)
                       : static_cast<std::uint32_t>(
                             std::max(1.0, std::ceil(bars_exact)));
  gSnapshot.status_line.clear();
  return true;
}

bool RenderClickLocked(const std::string &path, std::string &error) {
  const auto *render_abi = RenderAbi();
  if (!render_abi) {
    error = "Render ABI unavailable";
    return false;
  }
  orpheus_render_click_spec spec{};
  spec.tempo_bpm = gSnapshot.tempo_bpm;
  spec.bars = gSnapshot.bars;
  spec.sample_rate = 44100;
  spec.channels = 2;
  spec.gain = 0.3;
  spec.click_frequency_hz = 1000.0;
  spec.click_duration_seconds = 0.05;
  const auto status = render_abi->render_click(&spec, path.c_str());
  if (status != ORPHEUS_STATUS_OK) {
    error = "Render failed: " + StatusToString(status);
    return false;
  }
  gSnapshot.last_render_path = path;
  gSnapshot.status_line.clear();
  return true;
}

void EnsurePanelInitialized() {
  if (gPanelText.empty()) {
    RefreshPanelLocked();
  }
}

}  // namespace

extern "C" REAPER_ORPHEUS_API const char *ReaperExtensionName() {
  return orpheus::reaper::PanelTitle();
}

extern "C" REAPER_ORPHEUS_API const char *ReaperExtensionVersion() {
  static std::string version = "ABI " + orpheus::ToString(orpheus::kSessionAbi);
  return version.c_str();
}

extern "C" REAPER_ORPHEUS_API const char *ReaperExtensionPanelText() {
  std::lock_guard lock(gStateMutex);
  EnsurePanelInitialized();
  return gPanelText.c_str();
}

extern "C" REAPER_ORPHEUS_API int OrpheusTogglePanel() {
  std::lock_guard lock(gStateMutex);
  gSnapshot.visible = !gSnapshot.visible;
  RefreshPanelLocked();
  return 1;
}

extern "C" REAPER_ORPHEUS_API int OrpheusImportSession(const char *json_path) {
  if (json_path == nullptr) {
    return 0;
  }
  std::lock_guard lock(gStateMutex);
  std::string error;
  const bool ok = ImportSessionLocked(json_path, error);
  if (!ok) {
    gSnapshot.status_line = error;
  }
  RefreshPanelLocked();
  return ok ? 1 : 0;
}

extern "C" REAPER_ORPHEUS_API int OrpheusRenderClickToFile(const char *output_path) {
  if (output_path == nullptr) {
    return 0;
  }
  std::lock_guard lock(gStateMutex);
  std::string error;
  const bool ok = RenderClickLocked(output_path, error);
  if (!ok) {
    gSnapshot.status_line = error;
  }
  RefreshPanelLocked();
  return ok ? 1 : 0;
}
