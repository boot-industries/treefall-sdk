# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.22)

project(orpheus_shmui_package_consumer LANGUAGES C CXX)

if(NOT DEFINED ORPHEUS_REQUIRED_VERSION)
  message(FATAL_ERROR "ORPHEUS_REQUIRED_VERSION is required")
endif()

# The optional ShmUI package links the application's JUCE module targets. This
# models FreqFinder's pinned JUCE 8.0.4 provision without supplying any SDK or
# ShmUI source-tree path or fallback.
include(FetchContent)
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  JUCE
  URL https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.4.tar.gz
  URL_HASH SHA256=0effe9823a6fd8a4f98e5c08be7d308350e417b2d15dcf34011e778fefbbf005
  DOWNLOAD_EXTRACT_TIMESTAMP FALSE)
FetchContent_MakeAvailable(JUCE)

find_package(OrpheusSDK ${ORPHEUS_REQUIRED_VERSION} EXACT REQUIRED CONFIG)

foreach(required_target Orpheus::core Orpheus::audio_utils Orpheus::shmui_juce)
  if(NOT TARGET ${required_target})
    message(FATAL_ERROR "Installed package is missing ${required_target}")
  endif()
endforeach()

if(TARGET Orpheus::shmui_juce_gl)
  message(FATAL_ERROR "Non-OpenGL package consumer unexpectedly received OpenGL")
endif()

add_executable(orpheus_shmui_package_consumer main.cpp)
target_compile_features(orpheus_shmui_package_consumer PRIVATE cxx_std_20)
target_link_libraries(orpheus_shmui_package_consumer PRIVATE
  Orpheus::core
  Orpheus::audio_utils
  Orpheus::shmui_juce)

# JUCE's Linux networking layer compiles against libcurl; the consumer binary
# must carry the link dependency on hosts where the headers are present.
if(UNIX AND NOT APPLE)
  find_package(CURL REQUIRED)
  target_link_libraries(orpheus_shmui_package_consumer PRIVATE CURL::libcurl)
endif()

if(APPLE)
  target_compile_options(orpheus_shmui_package_consumer PRIVATE -stdlib=libc++)
  target_link_options(orpheus_shmui_package_consumer PRIVATE -stdlib=libc++)
endif()

enable_testing()
add_test(NAME orpheus_shmui_package_consumer COMMAND orpheus_shmui_package_consumer)
