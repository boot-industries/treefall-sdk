# SPDX-License-Identifier: MIT
add_library(reaper_orpheus SHARED
  panel.cpp
  reaper_entry.cpp
)

target_link_libraries(reaper_orpheus
  PRIVATE
    Orpheus::core
)

target_compile_definitions(reaper_orpheus
  PRIVATE
    REAPER_ORPHEUS_BUILDING
)

target_include_directories(reaper_orpheus
  PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/core/session
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

include(${CMAKE_SOURCE_DIR}/cmake/CompilerWarnings.cmake)
orpheus_enable_warnings(reaper_orpheus)
