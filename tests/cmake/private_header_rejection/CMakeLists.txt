# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.22)
project(orpheus_private_session_header_rejection LANGUAGES CXX)

find_package(OrpheusSDK CONFIG REQUIRED)
enable_testing()

# Build and run this equivalent first; the negative must not pass because the
# installed core, public header or its runtime dependencies are broken.
add_executable(public_session_io_consumer ../find_package/session_io.cpp)
target_compile_features(public_session_io_consumer PRIVATE cxx_std_20)
target_link_libraries(public_session_io_consumer PRIVATE Orpheus::core)
if(WIN32)
  foreach(runtime_target
      Orpheus::orpheus_abi_runtime Orpheus::orpheus_session
      Orpheus::orpheus_clipgrid Orpheus::orpheus_render)
    if(TARGET ${runtime_target})
      add_custom_command(TARGET public_session_io_consumer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:public_session_io_consumer>
                $<TARGET_FILE:${runtime_target}>
                $<TARGET_FILE_DIR:public_session_io_consumer>
        COMMAND_EXPAND_LISTS)
    endif()
  endforeach()
endif()
add_test(NAME public_session_io_consumer COMMAND public_session_io_consumer)

add_executable(private_session_header_consumer main.cpp)
target_compile_features(private_session_header_consumer PRIVATE cxx_std_20)
target_link_libraries(private_session_header_consumer PRIVATE Orpheus::core)
