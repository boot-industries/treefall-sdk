# SPDX-License-Identifier: MIT
add_executable(orpheus_perf_render_click
  perf_render_click.cpp
)

target_link_libraries(orpheus_perf_render_click
  PRIVATE
    Orpheus::core
)

target_include_directories(orpheus_perf_render_click
  PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

include(${CMAKE_SOURCE_DIR}/cmake/CompilerWarnings.cmake)
orpheus_enable_warnings(orpheus_perf_render_click)

if(ORP_BUILD_SHARED_CORE)
  add_custom_command(TARGET orpheus_perf_render_click POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:orpheus_session> $<TARGET_FILE_DIR:orpheus_perf_render_click>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:orpheus_clipgrid> $<TARGET_FILE_DIR:orpheus_perf_render_click>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:orpheus_render> $<TARGET_FILE_DIR:orpheus_perf_render_click>
  )
endif()
