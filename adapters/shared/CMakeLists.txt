# SPDX-License-Identifier: MIT
# Header-only utilities shared by the host adapters (minhost, reaper).

add_library(orpheus_adapters_common INTERFACE)
add_library(Orpheus::adapters_common ALIAS orpheus_adapters_common)

target_include_directories(orpheus_adapters_common
  INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(orpheus_adapters_common
  INTERFACE
    Orpheus::core
)
