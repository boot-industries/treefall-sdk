cmake_minimum_required(VERSION 3.22)
project(audio_backend_consumer LANGUAGES CXX)

if(NOT DEFINED ORPHEUS_DISABLED_BACKEND)
  message(FATAL_ERROR "ORPHEUS_DISABLED_BACKEND is required")
endif()

find_package(OrpheusSDK CONFIG REQUIRED)
if(ORPHEUS_DISABLED_BACKEND STREQUAL "CoreAudio" AND
   TARGET Orpheus::orpheus_audio_driver_coreaudio)
  message(FATAL_ERROR "CoreAudio target was exported while disabled")
endif()
if(ORPHEUS_DISABLED_BACKEND STREQUAL "WASAPI" AND
   TARGET Orpheus::orpheus_audio_driver_wasapi)
  message(FATAL_ERROR "WASAPI target was exported while disabled")
endif()

add_executable(audio_backend_consumer main.cpp)
target_compile_features(audio_backend_consumer PRIVATE cxx_std_20)
target_link_libraries(audio_backend_consumer PRIVATE Orpheus::audio_driver_manager)
