#pragma once

#if defined(_WIN32)
  #if defined(ORPHEUS_SHARED)
    #if defined(ORPHEUS_BUILDING)
      #define ORPHEUS_API __declspec(dllexport)
    #else
      #define ORPHEUS_API __declspec(dllimport)
    #endif
  #else
    #define ORPHEUS_API
  #endif
#else
  #define ORPHEUS_API
#endif
