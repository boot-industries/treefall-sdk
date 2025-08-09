#include "../reaper_plugin.h"
#include "reaper_plugin_functions.h"

class SimpleArp : public MIDI_effect
{
public:
  SimpleArp() {}
  MIDI_effect* Duplicate() override { return new SimpleArp(); }
  bool IsAvailable() override { return true; }
  void SetAvailable(bool) override {}
  const char* GetType() override { return "SimpleArp"; }
  int PropertiesWindow(HWND) override { return 0; }
  void Process(MIDI_effect_transfer_t* block) override
  {
    if (!block || !block->input_events || !block->output_events) return;
    int pos = 0;
    MIDI_event_t* evt;
    while ((evt = block->input_events->EnumItems(&pos)))
    {
      // pass through original event
      block->output_events->AddItem(evt);
      if (evt->is_note_on())
      {
        MIDI_event_t e = *evt;
        e.midi_message[1] = evt->midi_message[1] + 4; // major third
        e.frame_offset += 240; // simple offset
        block->output_events->AddItem(&e);
        e.midi_message[1] = evt->midi_message[1] + 7; // perfect fifth
        e.frame_offset += 240;
        block->output_events->AddItem(&e);
      }
    }
  }
  void SaveState(ProjectStateContext*) override {}
  int LoadState(const char*, ProjectStateContext*) override { return 0; }
  int Extended(int, void*, void*, void*) override { return 0; }
};

static const char* ArpName() { return "Example Arpeggiator"; }
static MIDI_effect* CreateArp() { return new SimpleArp(); }

static midieffect_register_t reg = { ArpName, CreateArp };

extern "C" {
REAPER_PLUGIN_DLL_EXPORT int REAPER_PLUGIN_ENTRYPOINT(REAPER_PLUGIN_HINSTANCE hInstance, reaper_plugin_info_t* rec)
{
  if (rec)
  {
    if (rec->caller_version != REAPER_PLUGIN_VERSION || !rec->Register) return 0;
    plugin_register = rec->Register;
    plugin_register("midieffect", &reg);
    return 1;
  }
  plugin_register("-midieffect", &reg);
  return 0;
}
}
