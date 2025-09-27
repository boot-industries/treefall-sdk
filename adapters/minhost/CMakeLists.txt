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

if(ORP_BUILD_SHARED_CORE)
  add_custom_command(TARGET orpheus_minhost POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:orpheus_session> $<TARGET_FILE_DIR:orpheus_minhost>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:orpheus_clipgrid> $<TARGET_FILE_DIR:orpheus_minhost>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:orpheus_render> $<TARGET_FILE_DIR:orpheus_minhost>
  )
endif()
