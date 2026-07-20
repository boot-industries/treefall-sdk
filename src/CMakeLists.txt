# SPDX-License-Identifier: MIT
set(ORPHEUS_LIBRARY_TYPE STATIC)
if(ORP_BUILD_SHARED_CORE)
  set(ORPHEUS_LIBRARY_TYPE SHARED)
endif()

add_library(orpheus_diagnostics_objects OBJECT
  core/common/performance_monitor.cpp
  core/common/realtime_diagnostics.cpp
  core/common/realtime_telemetry.cpp
)

target_compile_features(orpheus_diagnostics_objects PUBLIC cxx_std_20)
target_include_directories(orpheus_diagnostics_objects
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/core
)
set_target_properties(orpheus_diagnostics_objects PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(orpheus_channel_format_objects OBJECT
  core/channel_format.cpp
)

target_compile_features(orpheus_channel_format_objects PUBLIC cxx_std_20)
target_include_directories(orpheus_channel_format_objects
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/core
)
set_target_properties(orpheus_channel_format_objects PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(orpheus_core_runtime OBJECT
  core/common/errors.cpp
  core/common/json_parser.cpp
  core/adm/entity_graph.cpp
  core/render/pcm.cpp
  orpheus/render_tracks.cpp
  core/session/session_graph.cpp
  core/session/json_io.cpp
  core/session/scene_manager.cpp
  core/otio/reconform_plan.cpp
  dsp/oscillator.cpp
)

target_compile_features(orpheus_core_runtime PUBLIC cxx_std_20)
target_include_directories(orpheus_core_runtime
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/core
)
set_target_properties(orpheus_core_runtime PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(orpheus_session ${ORPHEUS_LIBRARY_TYPE}
  core/abi/session_api.cpp
)
add_library(orpheus_clipgrid ${ORPHEUS_LIBRARY_TYPE}
  core/abi/clipgrid_api.cpp
)
add_library(orpheus_render ${ORPHEUS_LIBRARY_TYPE}
  core/abi/render_api.cpp
)

set(ORPHEUS_CORE_TARGETS orpheus_session orpheus_clipgrid orpheus_render)

foreach(target ${ORPHEUS_CORE_TARGETS})
  target_sources(${target} PRIVATE
    $<TARGET_OBJECTS:orpheus_core_runtime>
    $<TARGET_OBJECTS:orpheus_diagnostics_objects>
    $<TARGET_OBJECTS:orpheus_channel_format_objects>
  )
  target_compile_features(${target} PUBLIC cxx_std_20)
  target_include_directories(${target}
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
      $<INSTALL_INTERFACE:include>
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/core
  )
  set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE ON)
endforeach()

# performance_monitor.cpp (in orpheus_diagnostics_objects) holds a public
# core::SessionGraph pointer, so it emits references to SessionGraph's member
# destructors (Clip/Track/MarkerSet/PlaylistLane). Bundle
# orpheus_core_runtime's objects so orpheus_diagnostics is genuinely self-contained
# — the standalone link check (verify_diagnostics_standalone) requires it, and
# MSVC's strict archive resolution fails to link without it (LNK2019).
add_library(orpheus_diagnostics STATIC
  core/common/diagnostics_archive_anchor.cpp
  $<TARGET_OBJECTS:orpheus_diagnostics_objects>
  $<TARGET_OBJECTS:orpheus_core_runtime>
  $<TARGET_OBJECTS:orpheus_channel_format_objects>
)
target_compile_features(orpheus_diagnostics PUBLIC cxx_std_20)
target_include_directories(orpheus_diagnostics
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:include>
)
set_target_properties(orpheus_diagnostics PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(Orpheus::diagnostics ALIAS orpheus_diagnostics)
add_library(Orpheus::orpheus_diagnostics ALIAS orpheus_diagnostics)

include(${ORPHEUS_ROOT_SOURCE_DIR}/cmake/CompilerWarnings.cmake)
orpheus_enable_warnings(orpheus_diagnostics_objects)
orpheus_enable_warnings(orpheus_channel_format_objects)
orpheus_enable_warnings(orpheus_core_runtime)
foreach(target ${ORPHEUS_CORE_TARGETS})
  orpheus_enable_warnings(${target})
endforeach()
orpheus_enable_warnings(orpheus_diagnostics)

orpheus_enable_sanitizers(orpheus_diagnostics_objects)
orpheus_enable_sanitizers(orpheus_channel_format_objects)
orpheus_enable_sanitizers(orpheus_core_runtime)
foreach(target ${ORPHEUS_CORE_TARGETS})
  orpheus_enable_sanitizers(${target})
endforeach()
orpheus_enable_sanitizers(orpheus_diagnostics)

if(WIN32 AND ORP_BUILD_SHARED_CORE)
  target_compile_definitions(orpheus_diagnostics_objects PRIVATE ORPHEUS_BUILDING_DLL)
  target_compile_definitions(orpheus_channel_format_objects PRIVATE ORPHEUS_BUILDING_DLL)
  target_compile_definitions(orpheus_core_runtime PRIVATE ORPHEUS_BUILDING_DLL)
  foreach(target ${ORPHEUS_CORE_TARGETS})
    target_compile_definitions(${target} PRIVATE ORPHEUS_BUILDING_DLL)
    set_target_properties(${target} PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS OFF)
  endforeach()
endif()

# M2: Real-time infrastructure modules
if(ORPHEUS_ENABLE_REALTIME)
  add_subdirectory(core/transport)
  list(APPEND ORPHEUS_CORE_TARGETS orpheus_transport)

  add_subdirectory(core/routing)
  list(APPEND ORPHEUS_CORE_TARGETS orpheus_routing)

  add_subdirectory(core/audio_io)
  if(TARGET orpheus_audio_io)
    list(APPEND ORPHEUS_CORE_TARGETS orpheus_audio_io)
  endif()

  # Platform audio drivers (CoreAudio, WASAPI, ASIO)
  # Note: Dummy driver is built with orpheus_audio_io
  add_subdirectory(platform/audio_drivers)
  if(TARGET orpheus_audio_driver_coreaudio)
    list(APPEND ORPHEUS_CORE_TARGETS orpheus_audio_driver_coreaudio)
  endif()
  if(TARGET orpheus_audio_driver_wasapi)
    list(APPEND ORPHEUS_CORE_TARGETS orpheus_audio_driver_wasapi)
  endif()
  if(TARGET orpheus_audio_driver_asio)
    list(APPEND ORPHEUS_CORE_TARGETS orpheus_audio_driver_asio)
  endif()
endif()

add_library(orpheus_core INTERFACE)
target_link_libraries(orpheus_core INTERFACE ${ORPHEUS_CORE_TARGETS})
orpheus_enable_sanitizers(orpheus_core)

if(WIN32 AND ORP_BUILD_SHARED_CORE)
  target_compile_definitions(orpheus_core INTERFACE ORPHEUS_USING_DLL)
endif()

add_library(Orpheus::core ALIAS orpheus_core)
add_library(Orpheus::orpheus_core ALIAS orpheus_core)
