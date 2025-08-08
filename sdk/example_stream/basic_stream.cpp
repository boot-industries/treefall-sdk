/*
  basic_stream
  Simple example demonstrating the reaper_stream API.
*/

#ifdef _WIN32
#include <windows.h>
#else
#include "../../WDL/swell/swell.h"
#endif

#include "../reaper_api_loader.hpp"

#define REAPERAPI_IMPLEMENT
#define REAPERAPI_MINIMAL
#define REAPER_PLUGIN_FUNCTIONS_IMPL_LOADFUNC
#include "../reaper_plugin.h"
#include "reaper_plugin_functions.h"
#include "../reaper_stream/reaper_stream.h"

REAPER_PLUGIN_HINSTANCE g_hInst;

extern "C" {

REAPER_PLUGIN_DLL_EXPORT int REAPER_PLUGIN_ENTRYPOINT(REAPER_PLUGIN_HINSTANCE instance, reaper_plugin_info_t *rec)
{
  g_hInst = instance;
  if (rec)
  {
    if (REAPERAPI_LoadAPI(rec->GetFunc)) return 0;

    int handle = stream_open("ws://localhost:9000");
    if (handle)
    {
      const int ns = 512;
      ReaSample buf[ns] = {0.0};
      PCM_source_transfer_t block = {};
      block.samplerate = 44100.0;
      block.nch = 1;
      block.length = ns;
      block.samples = buf;
      stream_send(handle, &block);
    }
    return 1;
  }
  return 0;
}

}
