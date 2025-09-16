#include "atmos_engine.h"

static AtmosEngine g_engine;

static void register_api(reaper_plugin_info_t *rec, const char *name, void *fn)
{
  if (rec && rec->Register && name && fn)
    rec->Register(name, fn);
}

void REAPER_API_DECL Atmos_RegisterSpeakerFormat(const reaper_atmos_speaker_format *fmt)
{
  g_engine.registerSpeakerFormat(fmt);
}

bool REAPER_API_DECL Atmos_UnregisterSpeakerFormat(const char *name)
{
  return g_engine.unregisterSpeakerFormat(name);
}

int REAPER_API_DECL Atmos_GetSpeakerFormatCount()
{
  return g_engine.getSpeakerFormatCount();
}

const reaper_atmos_speaker_format *REAPER_API_DECL Atmos_GetSpeakerFormat(int idx)
{
  return g_engine.getSpeakerFormat(idx);
}

void REAPER_API_DECL Atmos_AssignTrackObject(MediaTrack *track, int object_id)
{
  g_engine.assignTrackObject(track, object_id);
}

int REAPER_API_DECL Atmos_GetTrackObject(MediaTrack *track)
{
  return g_engine.getTrackObject(track);
}

void REAPER_API_DECL Atmos_UnassignTrackObject(MediaTrack *track)
{
  g_engine.unassignTrackObject(track);
}

void REAPER_API_DECL Atmos_ClearRouting()
{
  g_engine.clearRouting();
}

void REAPER_API_DECL Atmos_MapChannelToBed(int channel, int bed_channel_index)
{
  g_engine.mapChannelToBed(channel, bed_channel_index);
}

void REAPER_API_DECL Atmos_MapChannelToObject(int channel, int object_id)
{
  g_engine.mapChannelToObject(channel, object_id);
}

bool REAPER_API_DECL Atmos_BeginRenderFrame(const reaper_atmos_render_frame_t *frame)
{
  if (!frame)
    return false;
  return g_engine.beginFrame(*frame, nullptr);
}

void REAPER_API_DECL Atmos_EndRenderFrame()
{
  g_engine.endFrame();
}

bool REAPER_API_DECL Atmos_RouteBlock(PCM_source_transfer_t *block)
{
  if (!block)
    return false;
  return g_engine.processBlock(*block, nullptr);
}

bool REAPER_API_DECL Atmos_GetRoutingState(reaper_atmos_routing_state_t *state)
{
  return g_engine.getRoutingState(state);
}

int REAPER_API_DECL Atmos_GetActiveObjectCount()
{
  return g_engine.getActiveObjectCount();
}

bool REAPER_API_DECL Atmos_ExportADM(const char *path)
{
  if (!path)
    return false;
  return g_engine.exportADM(path);
}

bool REAPER_API_DECL Atmos_ExportBWF(const char *path)
{
  if (!path)
    return false;
  return g_engine.exportBWF(path);
}

extern "C" REAPER_PLUGIN_DLL_EXPORT int REAPER_PLUGIN_ENTRYPOINT(REAPER_PLUGIN_HINSTANCE, reaper_plugin_info_t *rec)
{
  if (!rec || !rec->Register)
    return 0;

  register_api(rec, "API_Atmos_AssignTrackObject", (void *)Atmos_AssignTrackObject);
  register_api(rec, "API_Atmos_GetTrackObject", (void *)Atmos_GetTrackObject);
  register_api(rec, "API_Atmos_UnassignTrackObject", (void *)Atmos_UnassignTrackObject);
  register_api(rec, "API_Atmos_RegisterSpeakerFormat", (void *)Atmos_RegisterSpeakerFormat);
  register_api(rec, "API_Atmos_UnregisterSpeakerFormat", (void *)Atmos_UnregisterSpeakerFormat);
  register_api(rec, "API_Atmos_GetSpeakerFormat", (void *)Atmos_GetSpeakerFormat);
  register_api(rec, "API_Atmos_GetSpeakerFormatCount", (void *)Atmos_GetSpeakerFormatCount);
  register_api(rec, "API_Atmos_ClearRouting", (void *)Atmos_ClearRouting);
  register_api(rec, "API_Atmos_MapChannelToBed", (void *)Atmos_MapChannelToBed);
  register_api(rec, "API_Atmos_MapChannelToObject", (void *)Atmos_MapChannelToObject);
  register_api(rec, "API_Atmos_BeginRenderFrame", (void *)Atmos_BeginRenderFrame);
  register_api(rec, "API_Atmos_EndRenderFrame", (void *)Atmos_EndRenderFrame);
  register_api(rec, "API_Atmos_RouteBlock", (void *)Atmos_RouteBlock);
  register_api(rec, "API_Atmos_GetRoutingState", (void *)Atmos_GetRoutingState);
  register_api(rec, "API_Atmos_GetActiveObjectCount", (void *)Atmos_GetActiveObjectCount);
  register_api(rec, "API_Atmos_ExportADM", (void *)Atmos_ExportADM);
  register_api(rec, "API_Atmos_ExportBWF", (void *)Atmos_ExportBWF);

  return 1;
}

