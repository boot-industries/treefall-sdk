# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.22)

project(orpheus_shmui_no_opengl_smoke LANGUAGES C CXX)

if(NOT DEFINED SHMUI_JUCE_SOURCE_DIR)
  message(FATAL_ERROR "SHMUI_JUCE_SOURCE_DIR must point at packages/shmui-juce")
endif()

include(FetchContent)
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  JUCE
  URL https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.4.tar.gz
  URL_HASH SHA256=0effe9823a6fd8a4f98e5c08be7d308350e417b2d15dcf34011e778fefbbf005
  DOWNLOAD_EXTRACT_TIMESTAMP FALSE)
FetchContent_MakeAvailable(JUCE)

set(SHMUI_JUCE_ENABLE_OPENGL OFF CACHE BOOL "" FORCE)
add_subdirectory(${SHMUI_JUCE_SOURCE_DIR} ${CMAKE_BINARY_DIR}/shmui-juce)

if(NOT TARGET Orpheus::shmui_juce)
  message(FATAL_ERROR "Orpheus::shmui_juce target is missing")
endif()
if(TARGET Orpheus::shmui_juce_gl)
  message(FATAL_ERROR "OpenGL target must not exist when SHMUI_JUCE_ENABLE_OPENGL=OFF")
endif()

get_target_property(_shmui_links orpheus_shmui_juce LINK_LIBRARIES)
if("juce::juce_opengl" IN_LIST _shmui_links)
  message(FATAL_ERROR "Core ShmUI target must not link juce_opengl")
endif()

add_executable(orpheus_shmui_no_opengl_smoke main.cpp)
target_link_libraries(orpheus_shmui_no_opengl_smoke PRIVATE Orpheus::shmui_juce)
target_compile_features(orpheus_shmui_no_opengl_smoke PRIVATE cxx_std_20)

enable_testing()
add_test(NAME shmui_no_opengl_smoke COMMAND orpheus_shmui_no_opengl_smoke)
