# SPDX-License-Identifier: MIT
if(ORP_WITH_TESTS)
  add_subdirectory(conformance)
endif()

add_subdirectory(perf)
add_subdirectory(cli)

if(WIN32 AND ORPHEUS_ENABLE_WASAPI)
  add_executable(orpheus_wasapi_hardware_acceptance
    wasapi_hardware_acceptance.cpp)
  target_compile_features(orpheus_wasapi_hardware_acceptance PRIVATE cxx_std_20)
  target_link_libraries(orpheus_wasapi_hardware_acceptance
    PRIVATE orpheus_audio_driver_manager)
  orpheus_enable_warnings(orpheus_wasapi_hardware_acceptance)
endif()

if(APPLE AND ORPHEUS_ENABLE_COREAUDIO)
  add_executable(orpheus_coreaudio_hardware_acceptance
    coreaudio_hardware_acceptance.cpp)
  target_compile_features(orpheus_coreaudio_hardware_acceptance PRIVATE cxx_std_20)
  target_link_libraries(orpheus_coreaudio_hardware_acceptance
    PRIVATE orpheus_audio_driver_coreaudio)
  orpheus_enable_warnings(orpheus_coreaudio_hardware_acceptance)
endif()
