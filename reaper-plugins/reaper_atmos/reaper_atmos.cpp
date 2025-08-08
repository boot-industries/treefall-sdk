#include "../../sdk/reaper_atmos.h"
#include <map>
#include <string>
#include <vector>
#include <cstring>

/*------------------------------------------------------------
  Simple Dolby Atmos routing module for the REAPER SDK.
  This is a minimal example that demonstrates how track
  channels can be mapped to bed or object outputs and how
  PCM_source_transfer_t blocks can be processed.
------------------------------------------------------------*/

struct AtmosChannelDest {
  bool is_object; // true=object, false=bed
  int index;      // bed/object index
};

class AtmosRouter {
public:
  void setChannels(int nch) { m_map.assign(nch, AtmosChannelDest{false,0}); }
  void mapChannelToBed(int ch, int bedIndex) {
    if (ch < (int)m_map.size()) m_map[ch] = {false, bedIndex};
  }
  void mapChannelToObject(int ch, int objectIndex) {
    if (ch < (int)m_map.size()) m_map[ch] = {true, objectIndex};
  }
  // Process a PCM block by dispatching channel data into bed/object buffers
  void processBlock(PCM_source_transfer_t *block) {
    const int nch = block->nch;
    const int len = block->length;
    if ((int)m_map.size() < nch) m_map.resize(nch);
    // allocate buffers
    if (m_beds.empty()) m_beds.resize(16); // support up to 16 beds in this example
    if (m_objects.empty()) m_objects.resize(128);
    for (int ch = 0; ch < nch; ++ch) {
      ReaSample *src = block->samples + ch * len;
      const AtmosChannelDest &d = m_map[ch];
      if (d.is_object) {
        m_objects[d.index].assign(src, src + len);
      } else {
        m_beds[d.index].assign(src, src + len);
      }
    }
  }
  const std::vector<ReaSample> &getBed(int idx) const { return m_beds[idx]; }
  const std::vector<ReaSample> &getObject(int idx) const { return m_objects[idx]; }
private:
  std::vector<AtmosChannelDest> m_map;
  std::vector< std::vector<ReaSample> > m_beds;
  std::vector< std::vector<ReaSample> > m_objects;
};

static AtmosRouter g_router; // global router instance

// ------------------------------------------------------------------
// Speaker format templates
// ------------------------------------------------------------------
struct BuiltinFormat {
  const char *name;
  const char *channels[16];
};

static BuiltinFormat g_builtin_formats[] = {
  {"5.1.4", {"L","R","C","LFE","Ls","Rs","Ltf","Rtf","Ltr","Rtr",0}},
  {"7.1.2", {"L","R","C","LFE","Lss","Rss","Lrs","Rrs","Ltf","Rtf",0}},
};
static std::vector<reaper_atmos_speaker_format> g_formats;

static void init_formats()
{
  if (!g_formats.empty()) return;
  for (const auto &f : g_builtin_formats)
  {
    reaper_atmos_speaker_format fmt{};
    fmt.name = f.name;
    int cnt = 0; while (f.channels[cnt]) cnt++;
    fmt.num_channels = cnt;
    fmt.channel_names = f.channels;
    g_formats.push_back(fmt);
  }
}

void REAPER_API_DECL Atmos_RegisterSpeakerFormat(const reaper_atmos_speaker_format *fmt)
{
  init_formats();
  if (fmt) g_formats.push_back(*fmt);
}

int REAPER_API_DECL Atmos_GetSpeakerFormatCount()
{
  init_formats();
  return (int)g_formats.size();
}

const reaper_atmos_speaker_format *REAPER_API_DECL Atmos_GetSpeakerFormat(int idx)
{
  init_formats();
  if (idx < 0 || idx >= (int)g_formats.size()) return nullptr;
  return &g_formats[idx];
}

// ------------------------------------------------------------------
// Object routing API
// ------------------------------------------------------------------
static std::map<MediaTrack*, int> g_trackToObject;

void REAPER_API_DECL Atmos_AssignTrackObject(MediaTrack *track, int object_id)
{
  if (!track) return;
  g_trackToObject[track] = object_id;
}

int REAPER_API_DECL Atmos_GetTrackObject(MediaTrack *track)
{
  if (!track) return -1;
  auto it = g_trackToObject.find(track);
  if (it == g_trackToObject.end()) return -1;
  return it->second;
}

// ------------------------------------------------------------------
// Export stubs
// ------------------------------------------------------------------
static bool write_text_file(const char *path, const char *text)
{
  FILE *fp = fopen(path, "wb");
  if (!fp) return false;
  size_t r = fwrite(text, 1, strlen(text), fp);
  fclose(fp);
  return r == strlen(text);
}

bool REAPER_API_DECL Atmos_ExportADM(const char *path)
{
  return write_text_file(path, "ADM export placeholder\n");
}

bool REAPER_API_DECL Atmos_ExportBWF(const char *path)
{
  return write_text_file(path, "BWF export placeholder\n");
}

// ------------------------------------------------------------------
// Plug-in entry point: expose our API to extensions
// ------------------------------------------------------------------
extern "C" REAPER_PLUGIN_DLL_EXPORT int REAPER_PLUGIN_ENTRYPOINT(REAPER_PLUGIN_HINSTANCE hInstance, reaper_plugin_info_t *rec)
{
  if (!rec || !rec->Register) return 0;
  init_formats();
  rec->Register("API_Atmos_AssignTrackObject", (void*)Atmos_AssignTrackObject);
  rec->Register("API_Atmos_GetTrackObject", (void*)Atmos_GetTrackObject);
  rec->Register("API_Atmos_GetSpeakerFormat", (void*)Atmos_GetSpeakerFormat);
  rec->Register("API_Atmos_GetSpeakerFormatCount", (void*)Atmos_GetSpeakerFormatCount);
  rec->Register("API_Atmos_ExportADM", (void*)Atmos_ExportADM);
  rec->Register("API_Atmos_ExportBWF", (void*)Atmos_ExportBWF);
  return 1;
}
