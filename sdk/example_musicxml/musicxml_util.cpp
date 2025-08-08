#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../WDL/wdltypes.h"
#include "../reaper_plugin.h"
#include "../reaper_plugin_functions.h"

// Export selected track MIDI to MusicXML
static bool DoExportTrackMIDIToMusicXML(MediaTrack* tr, const char* fn)
{
    if (!tr || !fn) return false;
    FILE* f = std::fopen(fn, "w");
    if (!f) return false;

    std::fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    std::fprintf(f, "<score-partwise version=\"3.1\">\n");
    std::fprintf(f, "  <part-list><score-part id=\"P1\"><part-name>Track</part-name></score-part></part-list>\n");
    std::fprintf(f, "  <part id=\"P1\">\n");
    std::fprintf(f, "    <measure number=\"1\">\n");

    const int itemCount = CountTrackMediaItems(tr);
    for (int i = 0; i < itemCount; ++i)
    {
        MediaItem* item = GetTrackMediaItem(tr, i);
        const int takeCount = GetMediaItemNumTakes(item);
        for (int t = 0; t < takeCount; ++t)
        {
            MediaItem_Take* take = GetMediaItemTake(item, t);
            if (!take || !TakeIsMIDI(take)) continue;

            int buf_sz = 0;
            MIDI_GetAllEvts(take, NULL, &buf_sz);
            char* buf = (char*)std::malloc(buf_sz);
            if (buf && MIDI_GetAllEvts(take, buf, &buf_sz))
            {
                int pos = 0;
                int ppq = 0;
                while (pos < buf_sz)
                {
                    int offset = *(int*)(buf + pos); pos += 4;
                    unsigned char flags = (unsigned char)buf[pos++]; (void)flags;
                    unsigned char msglen = (unsigned char)buf[pos++];
                    unsigned char* msg = (unsigned char*)buf + pos;
                    pos += msglen;
                    ppq += offset;
                    if (msglen >= 3 && (msg[0] & 0xF0) == 0x90 && msg[2] > 0)
                    {
                        int pitch = msg[1];
                        static const char* steps[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
                        const char* step = steps[pitch % 12];
                        int octave = pitch / 12 - 1;
                        std::fprintf(f, "      <note>\n");
                        std::fprintf(f, "        <pitch><step>%c</step", step[0]);
                        if (step[1] == '#') std::fprintf(f, "<alter>1</alter>");
                        std::fprintf(f, "<octave>%d</octave></pitch>\n", octave);
                        std::fprintf(f, "        <duration>1</duration>\n");
                        std::fprintf(f, "      </note>\n");
                    }
                }
                std::free(buf);
            }
        }
    }

    std::fprintf(f, "    </measure>\n");
    std::fprintf(f, "  </part>\n");
    std::fprintf(f, "</score-partwise>\n");
    std::fclose(f);
    return true;
}

// Import simple MusicXML to selected track
static bool DoImportTrackMIDIFromMusicXML(MediaTrack* tr, const char* fn)
{
    if (!tr || !fn) return false;
    FILE* f = std::fopen(fn, "r");
    if (!f) return false;

    MediaItem* item = AddMediaItemToTrack(tr);
    MediaItem_Take* take = AddTakeToMediaItem(item);
    double ppq = 0.0;
    char line[1024];
    char step = 'C';
    int alter = 0;
    int octave = 4;
    bool inNote = false;
    while (std::fgets(line, sizeof(line), f))
    {
        if (std::strstr(line, "<note")) { inNote = true; step = 'C'; alter = 0; octave = 4; }
        if (inNote)
        {
            char* p;
            if ((p = std::strstr(line, "<step>"))) step = p[6];
            if ((p = std::strstr(line, "<alter>"))) alter = std::atoi(p + 7);
            if ((p = std::strstr(line, "<octave>"))) octave = std::atoi(p + 8);
            if (std::strstr(line, "</note>"))
            {
                int base;
                switch (step)
                {
                    case 'C': base = 0; break; case 'D': base = 2; break;
                    case 'E': base = 4; break; case 'F': base = 5; break;
                    case 'G': base = 7; break; case 'A': base = 9; break;
                    case 'B': base = 11; break; default: base = 0; break;
                }
                base += alter;
                int pitch = (octave + 1) * 12 + base;
                double start = ppq;
                double end = ppq + 960.0;
                MIDI_InsertNote(take, false, false, start, end, 0, pitch, 100, NULL);
                ppq += 960.0;
                inNote = false;
            }
        }
    }
    std::fclose(f);
    return true;
}

extern "C" {
REAPER_PLUGIN_DLL_EXPORT int REAPER_PLUGIN_ENTRYPOINT(REAPER_PLUGIN_HINSTANCE hInstance, reaper_plugin_info_t* rec)
{
    (void)hInstance;
    if (!rec) return 0;
    if (rec->caller_version != REAPER_PLUGIN_VERSION || !rec->GetFunc) return 0;
    if (REAPERAPI_LoadAPI(rec->GetFunc)) return 0;
    rec->Register("API_ExportTrackMIDIToMusicXML", (void*)DoExportTrackMIDIToMusicXML);
    rec->Register("API_ImportTrackMIDIFromMusicXML", (void*)DoImportTrackMIDIFromMusicXML);
    return 1;
}
}

