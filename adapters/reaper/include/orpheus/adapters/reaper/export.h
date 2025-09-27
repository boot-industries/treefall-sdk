// SPDX-License-Identifier: MIT
#pragma once

#if defined(_WIN32)
  #if defined(REAPER_ORPHEUS_BUILDING)
    #define REAPER_ORPHEUS_API __declspec(dllexport)
  #else
    #define REAPER_ORPHEUS_API __declspec(dllimport)
  #endif
#else
  #define REAPER_ORPHEUS_API
#endif
