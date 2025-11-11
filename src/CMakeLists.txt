# SPDX-License-Identifier: MIT
set(ORPHEUS_LIBRARY_TYPE STATIC)
if(ORP_BUILD_SHARED_CORE)
  set(ORPHEUS_LIBRARY_TYPE SHARED)
endif()

add_library(orpheus_core_runtime OBJECT
  core/common/errors.cpp
  core/common/json_parser.cpp
  core/common/performance_monitor.cpp
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
  target_sources(${target} PRIVATE $<TARGET_OBJECTS:orpheus_core_runtime>)
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

include(${CMAKE_SOURCE_DIR}/cmake/CompilerWarnings.cmake)
orpheus_enable_warnings(orpheus_core_runtime)
foreach(target ${ORPHEUS_CORE_TARGETS})
  orpheus_enable_warnings(${target})
endforeach()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  foreach(target ${ORPHEUS_CORE_TARGETS})
    target_link_options(${target} PUBLIC "$<$<CONFIG:Debug>:-fsanitize=address>")
  endforeach()
endif()

if(ORP_ENABLE_UBSAN AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  foreach(target ${ORPHEUS_CORE_TARGETS})
    target_link_options(${target} PUBLIC "$<$<CONFIG:Debug>:-fsanitize=undefined>")
    if(ORP_EXTRA_UBSAN_LIB)
      target_link_libraries(${target} PUBLIC ${ORP_EXTRA_UBSAN_LIB})
    endif()
  endforeach()
endif()

if(WIN32 AND ORP_BUILD_SHARED_CORE)
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

if(WIN32 AND ORP_BUILD_SHARED_CORE)
  target_compile_definitions(orpheus_core INTERFACE ORPHEUS_USING_DLL)
endif()

add_library(Orpheus::core ALIAS orpheus_core)
add_library(Orpheus::orpheus_core ALIAS orpheus_core)
