add_executable(orpheus_minhost
  main.cpp
)

target_link_libraries(orpheus_minhost
  PRIVATE
    Orpheus::core
)

target_include_directories(orpheus_minhost
  PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/core/session
)

include(${CMAKE_SOURCE_DIR}/cmake/CompilerWarnings.cmake)
orpheus_enable_warnings(orpheus_minhost)
