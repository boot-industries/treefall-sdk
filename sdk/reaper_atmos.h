#ifndef REAPER_ATMOS_H
#define REAPER_ATMOS_H

#include "reaper_plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Mapping between tracks and Atmos objects/beds */
typedef struct reaper_atmos_object_mapping_t {
  int track_index;  /* index of the REAPER track */
  int object_id;    /* Dolby Atmos object identifier */
  int is_bed;       /* non-zero if this is a bed channel */
} reaper_atmos_object_mapping_t;

/* Speaker format template description */
typedef struct reaper_atmos_speaker_format {
  const char *name;            /* human readable name */
  int num_channels;            /* number of channels in format */
  const char **channel_names;  /* array of channel names */
} reaper_atmos_speaker_format;

/* API: obtain information about built in speaker formats */
REAPER_PLUGIN_DLL_EXPORT void REAPER_API_DECL Atmos_RegisterSpeakerFormat(const reaper_atmos_speaker_format *fmt);
REAPER_PLUGIN_DLL_EXPORT int REAPER_API_DECL Atmos_GetSpeakerFormatCount();
REAPER_PLUGIN_DLL_EXPORT const reaper_atmos_speaker_format *REAPER_API_DECL Atmos_GetSpeakerFormat(int idx);

/* API: assign a track to an Atmos object (object_id >=0) or bed (<0) */
REAPER_PLUGIN_DLL_EXPORT void REAPER_API_DECL Atmos_AssignTrackObject(MediaTrack *track, int object_id);
REAPER_PLUGIN_DLL_EXPORT int REAPER_API_DECL Atmos_GetTrackObject(MediaTrack *track);

/* API: export project using ADM or BWF standards */
REAPER_PLUGIN_DLL_EXPORT bool REAPER_API_DECL Atmos_ExportADM(const char *path);
REAPER_PLUGIN_DLL_EXPORT bool REAPER_API_DECL Atmos_ExportBWF(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* REAPER_ATMOS_H */
