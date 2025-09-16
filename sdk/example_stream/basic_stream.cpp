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

#include <chrono>
#include <thread>

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

    ShowConsoleMsg("basic_stream: opening ws://127.0.0.1:9000...\n");
    int handle = stream_open("ws://127.0.0.1:9000");
    if (handle)
    {
      const int ns = 512;
      ReaSample buf[ns] = {0.0};
      PCM_source_transfer_t block = {};
      block.samplerate = 44100.0;
      block.nch = 1;
      block.length = ns;
      block.samples = buf;
      // Fill the buffer with a short ramp for demonstration purposes.
      for (int i = 0; i < ns; ++i)
      {
        buf[i] = (ReaSample)i / (ReaSample)ns;
      }

      if (!stream_send(handle, &block))
      {
        ShowConsoleMsg("basic_stream: failed to send audio block.\n");
      }
      else
      {
        ShowConsoleMsg("basic_stream: block sent, waiting for echo...\n");
        for (int i = 0; i < ns; ++i)
        {
          buf[i] = 0.0;
        }

        PCM_source_transfer_t recv = {};
        recv.samplerate = block.samplerate;
        recv.nch = block.nch;
        recv.length = block.length;
        recv.samples = buf;

        int received = 0;
        for (int attempt = 0; attempt < 50 && received == 0; ++attempt)
        {
          std::this_thread::sleep_for(std::chrono::milliseconds(40));
          received = stream_receive(handle, &recv);
        }

        if (received > 0)
        {
          ShowConsoleMsg("basic_stream: received echoed audio block.\n");
        }
        else
        {
          ShowConsoleMsg("basic_stream: no audio received before timeout.\n");
        }
      }
    }
    else
    {
      ShowConsoleMsg("basic_stream: failed to open stream.\n");
    }
    return 1;
  }
  return 0;
}

}
