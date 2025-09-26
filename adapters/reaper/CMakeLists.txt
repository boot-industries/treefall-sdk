add_library(reaper_orpheus SHARED
  panel.cpp
  reaper_entry.cpp
)

target_link_libraries(reaper_orpheus
  PRIVATE
    orpheus_core
)

target_compile_definitions(reaper_orpheus PRIVATE ORPHEUS_BUILD_SHARED)

target_include_directories(reaper_orpheus
  PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

include(${CMAKE_SOURCE_DIR}/cmake/CompilerWarnings.cmake)
orpheus_enable_warnings(reaper_orpheus)
