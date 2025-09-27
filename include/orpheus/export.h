// SPDX-License-Identifier: MIT
#pragma once

#if defined(_WIN32)
  #if defined(ORPHEUS_BUILDING_DLL)
    #define ORPHEUS_API __declspec(dllexport)
  #elif defined(ORPHEUS_USING_DLL)
    #define ORPHEUS_API __declspec(dllimport)
  #else
    #define ORPHEUS_API
  #endif
#else
  #define ORPHEUS_API
#endif
