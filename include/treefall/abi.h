// SPDX-License-Identifier: MIT
#pragma once

#include "orpheus/abi.h"
#include "treefall/abi_version.h"
#include "treefall/errors.h"
#include "treefall/export.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef orpheus_abi_version treefall_abi_version;
typedef orpheus_transport_state treefall_transport_state;
typedef orpheus_track_desc treefall_track_desc;
typedef orpheus_clip_desc treefall_clip_desc;
typedef orpheus_quantization_window treefall_quantization_window;
typedef orpheus_scene_trigger_desc treefall_scene_trigger_desc;
typedef orpheus_scene_end_desc treefall_scene_end_desc;
typedef orpheus_arrangement_commit_desc treefall_arrangement_commit_desc;
typedef orpheus_render_click_spec treefall_render_click_spec;
typedef orpheus_session_api_v1 treefall_session_api_v1;
typedef orpheus_clipgrid_api_v1 treefall_clipgrid_api_v1;
typedef orpheus_render_api_v1 treefall_render_api_v1;

typedef struct orpheus_session_handle_t treefall_session_handle_t;
typedef struct orpheus_track_handle_t treefall_track_handle_t;
typedef struct orpheus_clip_handle_t treefall_clip_handle_t;
typedef struct orpheus_scene_handle_t treefall_scene_handle_t;

typedef orpheus_session_handle treefall_session_handle;
typedef orpheus_track_handle treefall_track_handle;
typedef orpheus_clip_handle treefall_clip_handle;
typedef orpheus_scene_handle treefall_scene_handle;

TREEFALL_API const treefall_session_api_v1*
treefall_session_abi_v1(uint32_t want_major, uint32_t* got_major, uint32_t* got_minor);
TREEFALL_API const treefall_clipgrid_api_v1*
treefall_clipgrid_abi_v1(uint32_t want_major, uint32_t* got_major, uint32_t* got_minor);
TREEFALL_API const treefall_render_api_v1*
treefall_render_abi_v1(uint32_t want_major, uint32_t* got_major, uint32_t* got_minor);

#ifdef __cplusplus
}
#endif
