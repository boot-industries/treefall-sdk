add_executable(orpheus_minhost
  click_renderer.cpp
  main.cpp
)

target_link_libraries(orpheus_minhost
  PRIVATE
    orpheus_core
)

target_include_directories(orpheus_minhost
  PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

include(${CMAKE_SOURCE_DIR}/cmake/CompilerWarnings.cmake)
orpheus_enable_warnings(orpheus_minhost)
