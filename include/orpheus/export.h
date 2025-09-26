#pragma once

#if defined(_WIN32)
  #if defined(ORPHEUS_BUILD_SHARED)
    #define ORPHEUS_API __declspec(dllexport)
  #else
    #define ORPHEUS_API __declspec(dllimport)
  #endif
#else
  #define ORPHEUS_API __attribute__((visibility("default")))
#endif
