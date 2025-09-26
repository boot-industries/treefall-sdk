#pragma once

#include "orpheus/export.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct orpheus_abi_version {
  uint32_t major;
  uint32_t minor;
} orpheus_abi_version;

typedef enum orpheus_status {
  ORPHEUS_STATUS_OK = 0,
  ORPHEUS_STATUS_INVALID_ARGUMENT = 1,
  ORPHEUS_STATUS_NOT_FOUND = 2,
  ORPHEUS_STATUS_OUT_OF_MEMORY = 3,
  ORPHEUS_STATUS_INTERNAL_ERROR = 4,
  ORPHEUS_STATUS_NOT_IMPLEMENTED = 5,
  ORPHEUS_STATUS_IO_ERROR = 6
} orpheus_status;

typedef struct orpheus_transport_state {
  double tempo_bpm;
  double position_beats;
  int32_t is_playing;
} orpheus_transport_state;

struct orpheus_session_handle_t;
struct orpheus_track_handle_t;
struct orpheus_clip_handle_t;

typedef struct orpheus_session_handle_t *orpheus_session_handle;
typedef struct orpheus_track_handle_t *orpheus_track_handle;
typedef struct orpheus_clip_handle_t *orpheus_clip_handle;

typedef struct orpheus_track_desc {
  const char *name;
} orpheus_track_desc;

typedef struct orpheus_clip_desc {
  const char *name;
  double start_beats;
  double length_beats;
} orpheus_clip_desc;

typedef struct orpheus_render_click_spec {
  double tempo_bpm;
  uint32_t bars;
  uint32_t sample_rate;
  uint32_t channels;
  double gain;
  double click_frequency_hz;
  double click_duration_seconds;
} orpheus_render_click_spec;

typedef struct orpheus_session_v1 {
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
} orpheus_session_v1;

typedef struct orpheus_clipgrid_v1 {
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
  orpheus_status (*commit)(orpheus_session_handle session);
} orpheus_clipgrid_v1;

typedef struct orpheus_render_v1 {
  orpheus_status (*render_click)(const orpheus_render_click_spec *spec,
                                 const char *out_path);
  orpheus_status (*render_tracks)(orpheus_session_handle session,
                                  const char *out_path);
} orpheus_render_v1;

typedef struct orpheus_abi_negotiator {
  orpheus_abi_version (*negotiate)(orpheus_abi_version requested);
} orpheus_abi_negotiator;

ORPHEUS_API const orpheus_session_v1 *orpheus_session_abi_v1(void);
ORPHEUS_API const orpheus_clipgrid_v1 *orpheus_clipgrid_abi_v1(void);
ORPHEUS_API const orpheus_render_v1 *orpheus_render_abi_v1(void);
ORPHEUS_API const orpheus_abi_negotiator *orpheus_negotiate_abi(void);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include <string>

namespace orpheus {

using AbiVersion = orpheus_abi_version;

inline constexpr AbiVersion kCurrentAbi{1, 0};

inline std::string ToString(const AbiVersion &version) {
  return std::to_string(version.major) + "." + std::to_string(version.minor);
}

inline AbiVersion NegotiateAbi(const AbiVersion &requested) {
  const auto *negotiator = orpheus_negotiate_abi();
  if (negotiator == nullptr || negotiator->negotiate == nullptr) {
    return kCurrentAbi;
  }
  return negotiator->negotiate(requested);
}

}  // namespace orpheus

#endif  // __cplusplus
