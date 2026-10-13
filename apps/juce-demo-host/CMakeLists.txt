# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.22)

project(orpheus_demo_host LANGUAGES C CXX)

include(FetchContent)

if(NOT TARGET juce::juce_gui_basics)
  FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(juce)
endif()

juce_add_gui_app(orpheus_demo_host_app
  PRODUCT_NAME "Orpheus Demo Host"
  VERSION 0.1.0
  COMPANY_NAME "Orpheus"
  BUNDLE_ID "com.orpheus.demo.host"
  NEEDS_CURL FALSE
  NEEDS_WEB_BROWSER FALSE
)

# Source-tree-only demo: session I/O is public, OTIO and graph rendering remain private.
target_sources(orpheus_demo_host_app PRIVATE Source/Main.cpp)

target_compile_features(orpheus_demo_host_app PRIVATE cxx_std_20)

target_include_directories(orpheus_demo_host_app
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
)

target_compile_definitions(orpheus_demo_host_app
  PRIVATE
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_DISABLE_JUCE_VERSION_PRINTING=1
)

target_link_libraries(orpheus_demo_host_app
  PRIVATE
    Orpheus::core
    juce::juce_gui_extra
    juce::juce_audio_utils
)

set_target_properties(orpheus_demo_host_app PROPERTIES
  OUTPUT_NAME "OrpheusDemoHost"
)

include(${CMAKE_SOURCE_DIR}/cmake/CompilerWarnings.cmake)
orpheus_enable_warnings(orpheus_demo_host_app Source/Main.cpp)
