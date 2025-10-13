#ifndef REAPER_ATMOS_H
#define REAPER_ATMOS_H

#include "reaper_plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Mapping between tracks and Atmos objects/beds */
typedef struct reaper_atmos_object_mapping_t {
  int track_index; /* index of the REAPER track */
  int object_id;   /* Dolby Atmos object identifier */
  int is_bed;      /* non-zero if this is a bed channel */
} reaper_atmos_object_mapping_t;

/* Speaker format template description */
typedef struct reaper_atmos_speaker_format {
  const char* name;           /* human readable name */
  int num_channels;           /* number of channels in format */
  const char** channel_names; /* array of channel names */
} reaper_atmos_speaker_format;

/* Host-provided buffer descriptions used during routing */
typedef struct reaper_atmos_buffer_t {
  ReaSample* data; /* pointer to planar channel samples */
  int frames;      /* number of sample frames available */
  int stride;      /* step between frames (1 for contiguous) */
} reaper_atmos_buffer_t;

typedef struct reaper_atmos_bed_channel_t {
  int channel_index;            /* index within the speaker format */
  const char* channel_name;     /* optional channel name */
  reaper_atmos_buffer_t buffer; /* writable buffer for this bed channel */
} reaper_atmos_bed_channel_t;

typedef struct reaper_atmos_object_buffer_t {
  int object_id;                /* Dolby Atmos object identifier */
  reaper_atmos_buffer_t buffer; /* writable buffer for this object */
} reaper_atmos_object_buffer_t;

typedef struct reaper_atmos_render_frame_t {
  double samplerate;                        /* samplerate for the frame */
  int block_length;                         /* expected sample frames per buffer */
  int num_bed_channels;                     /* number of bed channel buffers */
  reaper_atmos_bed_channel_t* bed_channels; /* array of bed channel descriptors */
  int num_objects;                          /* number of object buffers */
  reaper_atmos_object_buffer_t* objects;    /* array of object descriptors */
} reaper_atmos_render_frame_t;

typedef struct reaper_atmos_routing_dest_t {
  int source_channel;    /* input channel index */
  int is_object;         /* non-zero if routed to an object */
  int destination_index; /* bed channel index or object id */
  int object_id;         /* copy of object id when is_object != 0, otherwise -1 */
} reaper_atmos_routing_dest_t;

typedef struct reaper_atmos_routing_state_t {
  double samplerate;                         /* samplerate of the active frame */
  int block_length;                          /* block length in frames */
  reaper_atmos_routing_dest_t* destinations; /* caller-provided buffer for entries */
  int destinations_capacity;                 /* capacity of destinations buffer */
  int destinations_count;                    /* number of entries available */
  int destinations_written;                  /* number of entries written */
} reaper_atmos_routing_state_t;

/* API: obtain information about built in speaker formats */
REAPER_PLUGIN_DLL_EXPORT void REAPER_API_DECL
Atmos_RegisterSpeakerFormat(const reaper_atmos_speaker_format* fmt);
REAPER_PLUGIN_DLL_EXPORT int REAPER_API_DECL Atmos_GetSpeakerFormatCount();
REAPER_PLUGIN_DLL_EXPORT const reaper_atmos_speaker_format* REAPER_API_DECL
Atmos_GetSpeakerFormat(int idx);
REAPER_PLUGIN_DLL_EXPORT bool REAPER_API_DECL Atmos_UnregisterSpeakerFormat(const char* name);

/* API: assign a track to an Atmos object (object_id >=0) or bed (<0) */
REAPER_PLUGIN_DLL_EXPORT void REAPER_API_DECL Atmos_AssignTrackObject(MediaTrack* track,
                                                                      int object_id);
REAPER_PLUGIN_DLL_EXPORT int REAPER_API_DECL Atmos_GetTrackObject(MediaTrack* track);
REAPER_PLUGIN_DLL_EXPORT void REAPER_API_DECL Atmos_UnassignTrackObject(MediaTrack* track);

/* API: export project using ADM or BWF standards */
REAPER_PLUGIN_DLL_EXPORT bool REAPER_API_DECL Atmos_ExportADM(const char* path);
REAPER_PLUGIN_DLL_EXPORT bool REAPER_API_DECL Atmos_ExportBWF(const char* path);

/* API: routing control */
REAPER_PLUGIN_DLL_EXPORT void REAPER_API_DECL Atmos_ClearRouting();
REAPER_PLUGIN_DLL_EXPORT void REAPER_API_DECL Atmos_MapChannelToBed(int channel,
                                                                    int bed_channel_index);
REAPER_PLUGIN_DLL_EXPORT void REAPER_API_DECL Atmos_MapChannelToObject(int channel, int object_id);
REAPER_PLUGIN_DLL_EXPORT bool REAPER_API_DECL
Atmos_BeginRenderFrame(const reaper_atmos_render_frame_t* frame);
REAPER_PLUGIN_DLL_EXPORT void REAPER_API_DECL Atmos_EndRenderFrame();
REAPER_PLUGIN_DLL_EXPORT bool REAPER_API_DECL Atmos_RouteBlock(PCM_source_transfer_t* block);
REAPER_PLUGIN_DLL_EXPORT bool REAPER_API_DECL
Atmos_GetRoutingState(reaper_atmos_routing_state_t* state);
REAPER_PLUGIN_DLL_EXPORT int REAPER_API_DECL Atmos_GetActiveObjectCount();

#ifdef __cplusplus
}
#endif

#endif /* REAPER_ATMOS_H */
