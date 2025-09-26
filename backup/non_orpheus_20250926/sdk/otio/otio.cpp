#ifdef _WIN32
#include <windows.h>
#else
#include "../../WDL/swell/swell.h"
#endif

#include "../../WDL/wdltypes.h"
#include "../reaper_plugin.h"

#define REAPERAPI_IMPLEMENT
#define REAPERAPI_MINIMAL
#define REAPERAPI_WANT_AddMediaItemToTrack
#define REAPERAPI_WANT_AddProjectMarker
#define REAPERAPI_WANT_CountProjectMarkers
#define REAPERAPI_WANT_CountTempoTimeSigMarkers
#define REAPERAPI_WANT_CountTrackMediaItems
#define REAPERAPI_WANT_CountTracks
#define REAPERAPI_WANT_EnumProjectMarkers2
#define REAPERAPI_WANT_EnumProjects
#define REAPERAPI_WANT_GetMediaItemInfo_Value
#define REAPERAPI_WANT_GetTempoTimeSigMarker
#define REAPERAPI_WANT_GetTrack
#define REAPERAPI_WANT_GetTrackMediaItem
#define REAPERAPI_WANT_InsertTrackAtIndex
#define REAPERAPI_WANT_SetMediaItemInfo_Value
#define REAPERAPI_WANT_SetTempoTimeSigMarker
#include "../reaper_plugin_functions.h"

#ifdef HAVE_OPENTIMELINEIO
#include <opentimelineio/timeline.h>
#include <opentimelineio/track.h>
#include <opentimelineio/clip.h>
#include <opentimelineio/marker.h>
#include <opentimelineio/serialization.h>
using namespace opentimelineio::OPENTIMELINEIO_VERSION;
#endif

// Export REAPER project to an OTIO file.
bool Project_ExportOTIO(ReaProject* proj, const char* fn)
{
#ifdef HAVE_OPENTIMELINEIO
  Timeline timeline("REAPER Export");
  // collect tracks and items
  int trackCount = CountTracks(proj);
  for (int ti = 0; ti < trackCount; ++ti)
  {
    MediaTrack* tr = GetTrack(proj, ti);
    Track* otioTrack = new Track();
    int itemCount = CountTrackMediaItems(tr);
    for (int ii = 0; ii < itemCount; ++ii)
    {
      MediaItem* item = GetTrackMediaItem(tr, ii);
      double pos = GetMediaItemInfo_Value(item, "D_POSITION");
      double len = GetMediaItemInfo_Value(item, "D_LENGTH");
      Clip* clip = new Clip();
      clip->set_start_time(RationalTime(pos, 48000.0));
      clip->set_duration(RationalTime(len, 48000.0));
      otioTrack->append_child(clip);
    }
    timeline.tracks()->append_child(otioTrack);
  }
  // markers
  int numMarkers = 0, numRegions = 0;
  int total = CountProjectMarkers(proj, &numMarkers, &numRegions);
  for (int i = 0; i < total; ++i)
  {
    bool isrgn; double pos, end; const char* name; int idx;
    EnumProjectMarkers2(proj, i, &isrgn, &pos, &end, &name, &idx);
    Marker* m = new Marker(name ? name : "");
    m->set_time(RationalTime(pos, 48000.0));
    timeline.global_markers().push_back(m);
  }
  // tempo markers
  int tempoCount = CountTempoTimeSigMarkers(proj);
  for (int i = 0; i < tempoCount; ++i)
  {
    double timepos, beatpos, bpm; int num, denom; bool lineartempo;
    GetTempoTimeSigMarker(proj, i, &timepos, NULL, &beatpos, &bpm, &num, &denom, &lineartempo);
    Any dict;
    dict["position"] = timepos;
    dict["bpm"] = bpm;
    timeline.metadata()["tempo"][i] = dict;
  }
  ErrorStatus err;
  serialize_json_to_file(&timeline, fn, &err);
  return !err;
#else
  (void)proj; (void)fn;
  return false;
#endif
}

// Import OTIO file into a REAPER project.
ReaProject* Project_ImportOTIO(const char* fn)
{
#ifdef HAVE_OPENTIMELINEIO
  ErrorStatus err;
  auto timeline = Timeline::from_json_file(fn, &err);
  if (err || !timeline) return NULL;
  ReaProject* proj = EnumProjects(-1, NULL, 0);
  int trackIndex = 0;
  for (auto& child : timeline->tracks()->children())
  {
    Track* otioTrack = dynamic_cast<Track*>(child);
    if (!otioTrack) continue;
    InsertTrackAtIndex(trackIndex, true);
    MediaTrack* tr = GetTrack(proj, trackIndex++);
    int itemIndex = 0;
    for (auto& clipChild : otioTrack->children())
    {
      Clip* clip = dynamic_cast<Clip*>(clipChild);
      if (!clip) continue;
      MediaItem* item = AddMediaItemToTrack(tr);
      SetMediaItemInfo_Value(item, "D_POSITION", clip->start_time().value());
      SetMediaItemInfo_Value(item, "D_LENGTH", clip->duration().value());
    }
  }
  for (auto& m : timeline->global_markers())
  {
    AddProjectMarker(proj, false, m->time().value(), 0.0, m->name().c_str(), -1);
  }
  auto tempoMeta = timeline->metadata()["tempo"];
  for (auto& it : tempoMeta)
  {
    double pos = it.second["position"].to_double();
    double bpm = it.second["bpm"].to_double();
    SetTempoTimeSigMarker(proj, -1, pos, 0, 0.0, bpm, 0, 0, false);
  }
  return proj;
#else
  (void)fn;
  return NULL;
#endif
}
