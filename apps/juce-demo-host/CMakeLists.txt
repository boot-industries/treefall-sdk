# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.22)

project(orpheus_demo_host LANGUAGES CXX)

include(FetchContent)

if(NOT TARGET juce::juce_gui_basics)
  FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 7.0.9
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

set(ORPHEUS_DEMO_HOST_SOURCES
  Source/Main.cpp
  ${CMAKE_SOURCE_DIR}/src/core/session/session_graph.cpp
  ${CMAKE_SOURCE_DIR}/src/core/session/json_io.cpp
)

target_sources(orpheus_demo_host_app PRIVATE ${ORPHEUS_DEMO_HOST_SOURCES})

target_compile_features(orpheus_demo_host_app PRIVATE cxx_std_20)

target_include_directories(orpheus_demo_host_app
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core/session
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_definitions(orpheus_demo_host_app
  PRIVATE
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_DISABLE_JUCE_VERSION_PRINTING=1
)

target_link_libraries(orpheus_demo_host_app
  PRIVATE
    juce::juce_gui_extra
    juce::juce_audio_utils
)

set_target_properties(orpheus_demo_host_app PROPERTIES
  OUTPUT_NAME "OrpheusDemoHost"
)

include(${CMAKE_SOURCE_DIR}/cmake/CompilerWarnings.cmake)
orpheus_enable_warnings(orpheus_demo_host_app)
