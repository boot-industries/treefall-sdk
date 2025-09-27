// SPDX-License-Identifier: MIT
#pragma once

#include "orpheus/abi_version.h"
#include "orpheus/errors.h"
#include "orpheus/export.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct orpheus_abi_version {
  uint32_t major;
  uint32_t minor;
} orpheus_abi_version;

typedef struct orpheus_transport_state {
  double tempo_bpm;
  double position_beats;
  int32_t is_playing;
} orpheus_transport_state;

struct orpheus_session_handle_t;
struct orpheus_track_handle_t;
struct orpheus_clip_handle_t;
struct orpheus_scene_handle_t;

typedef struct orpheus_session_handle_t *orpheus_session_handle;
typedef struct orpheus_track_handle_t *orpheus_track_handle;
typedef struct orpheus_clip_handle_t *orpheus_clip_handle;
typedef struct orpheus_scene_handle_t *orpheus_scene_handle;

typedef struct orpheus_track_desc {
  const char *name;
} orpheus_track_desc;

typedef struct orpheus_clip_desc {
  const char *name;
  double start_beats;
  double length_beats;
  uint32_t scene_index;
} orpheus_clip_desc;

typedef struct orpheus_quantization_window {
  double grid_beats;
  double tolerance_beats;
} orpheus_quantization_window;

typedef struct orpheus_scene_trigger_desc {
  uint32_t scene_index;
  double position_beats;
  orpheus_quantization_window quant;
} orpheus_scene_trigger_desc;

typedef struct orpheus_scene_end_desc {
  uint32_t scene_index;
  double position_beats;
  orpheus_quantization_window quant;
} orpheus_scene_end_desc;

typedef struct orpheus_arrangement_commit_desc {
  double fallback_scene_length_beats;
} orpheus_arrangement_commit_desc;

typedef struct orpheus_render_click_spec {
  double tempo_bpm;
  uint32_t bars;
  uint32_t sample_rate;
  uint32_t channels;
  double gain;
  double click_frequency_hz;
  double click_duration_seconds;
} orpheus_render_click_spec;

typedef struct orpheus_session_api_v1 {
  uint64_t caps;
  orpheus_status (*create)(orpheus_session_handle *out_session);
  void (*destroy)(orpheus_session_handle session);
  orpheus_status (*add_track)(orpheus_session_handle session,
                              const orpheus_track_desc *desc,
                              orpheus_track_handle *out_track);
  orpheus_status (*remove_track)(orpheus_session_handle session,
                                 orpheus_track_handle track);
  orpheus_status (*set_tempo)(orpheus_session_handle session, double bpm);
  orpheus_status (*get_transport_state)(orpheus_session_handle session,
                                        orpheus_transport_state *out_state);
} orpheus_session_api_v1;

typedef struct orpheus_clipgrid_api_v1 {
  uint64_t caps;
  orpheus_status (*add_clip)(orpheus_session_handle session,
                             orpheus_track_handle track,
                             const orpheus_clip_desc *desc,
                             orpheus_clip_handle *out_clip);
  orpheus_status (*remove_clip)(orpheus_session_handle session,
                                orpheus_clip_handle clip);
  orpheus_status (*set_clip_start)(orpheus_session_handle session,
                                   orpheus_clip_handle clip,
                                   double start_beats);
  orpheus_status (*set_clip_length)(orpheus_session_handle session,
                                    orpheus_clip_handle clip,
                                    double length_beats);
  orpheus_status (*set_clip_scene)(orpheus_session_handle session,
                                   orpheus_clip_handle clip,
                                   uint32_t scene_index);
  orpheus_status (*commit)(orpheus_session_handle session);
  orpheus_status (*trigger_scene)(orpheus_session_handle session,
                                  const orpheus_scene_trigger_desc *desc);
  orpheus_status (*end_scene)(orpheus_session_handle session,
                              const orpheus_scene_end_desc *desc);
  orpheus_status (*commit_arrangement)(
      orpheus_session_handle session,
      const orpheus_arrangement_commit_desc *desc);
} orpheus_clipgrid_api_v1;

typedef struct orpheus_render_api_v1 {
  uint64_t caps;
  orpheus_status (*render_click)(const orpheus_render_click_spec *spec,
                                 const char *out_path);
  orpheus_status (*render_tracks)(orpheus_session_handle session,
                                  const char *out_path);
} orpheus_render_api_v1;

ORPHEUS_API const orpheus_session_api_v1 *orpheus_session_abi_v1(
    uint32_t want_major, uint32_t *got_major, uint32_t *got_minor);
ORPHEUS_API const orpheus_clipgrid_api_v1 *orpheus_clipgrid_abi_v1(
    uint32_t want_major, uint32_t *got_major, uint32_t *got_minor);
ORPHEUS_API const orpheus_render_api_v1 *orpheus_render_abi_v1(
    uint32_t want_major, uint32_t *got_major, uint32_t *got_minor);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include <string>

namespace orpheus {

using AbiVersion = orpheus_abi_version;

inline constexpr AbiVersion kSessionAbi{ORPHEUS_ABI_V1_MAJOR,
                                        ORPHEUS_ABI_V1_MINOR};
inline constexpr AbiVersion kClipgridAbi{ORPHEUS_ABI_V1_MAJOR,
                                         ORPHEUS_ABI_V1_MINOR};
inline constexpr AbiVersion kRenderAbi{ORPHEUS_ABI_V1_MAJOR,
                                       ORPHEUS_ABI_V1_MINOR};

inline std::string ToString(const AbiVersion &version) {
  return std::to_string(version.major) + "." + std::to_string(version.minor);
}

}  // namespace orpheus

#endif  // __cplusplus
