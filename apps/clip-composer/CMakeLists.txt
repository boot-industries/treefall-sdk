# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.22)

project(orpheus_clip_composer LANGUAGES C CXX)

# Fetch JUCE if not already available
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

# Create JUCE GUI application
juce_add_gui_app(orpheus_clip_composer_app
  PRODUCT_NAME "Orpheus Clip Composer"
  VERSION 0.1.0
  COMPANY_NAME "Orpheus"
  BUNDLE_ID "com.orpheus.clipcomposer"
  NEEDS_CURL FALSE
  NEEDS_WEB_BROWSER FALSE
)

# Source files
set(CLIP_COMPOSER_SOURCES
  Source/Main.cpp
  Source/MainComponent.cpp
  Source/ClipGrid/ClipButton.cpp
  Source/ClipGrid/ClipGrid.cpp
  Source/Session/SessionManager.cpp
  Source/Transport/TransportControls.cpp
  Source/UI/TabSwitcher.cpp
  Source/UI/AudioSettingsDialog.cpp
  Source/UI/ClipEditDialog.cpp
  Source/UI/WaveformDisplay.cpp
  Source/UI/PreviewPlayer.cpp
  Source/Audio/AudioEngine.cpp
)

target_sources(orpheus_clip_composer_app PRIVATE ${CLIP_COMPOSER_SOURCES})

# C++20 required (matches Orpheus SDK)
target_compile_features(orpheus_clip_composer_app PRIVATE cxx_std_20)

# Include directories
target_include_directories(orpheus_clip_composer_app
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${CMAKE_SOURCE_DIR}/include  # Orpheus SDK public headers
)

# JUCE compile definitions
target_compile_definitions(orpheus_clip_composer_app
  PRIVATE
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_DISABLE_JUCE_VERSION_PRINTING=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
)

# Link against JUCE modules
target_link_libraries(orpheus_clip_composer_app
  PRIVATE
    juce::juce_gui_extra
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_utils
    juce::juce_events
)

# Link against Orpheus SDK modules
# Note: These libraries are built by the SDK CMakeLists.txt in src/core/
set(SDK_LIB_DIR "${CMAKE_SOURCE_DIR}/build/src/core")
set(SDK_PLATFORM_LIB_DIR "${CMAKE_SOURCE_DIR}/build/src/platform/audio_drivers")

# Core SDK libraries (static linking for now)
target_link_libraries(orpheus_clip_composer_app
  PRIVATE
    ${SDK_LIB_DIR}/transport/liborpheus_transport.a
    ${SDK_LIB_DIR}/audio_io/liborpheus_audio_io.a
    ${SDK_LIB_DIR}/routing/liborpheus_routing.a
    ${SDK_PLATFORM_LIB_DIR}/liborpheus_audio_driver_coreaudio.a
)

# Link libsndfile (required by audio_io)
find_library(LIBSNDFILE_LIBRARY sndfile)
if(LIBSNDFILE_LIBRARY)
  target_link_libraries(orpheus_clip_composer_app PRIVATE ${LIBSNDFILE_LIBRARY})
else()
  message(FATAL_ERROR "libsndfile not found. Install with: brew install libsndfile")
endif()

# Link CoreAudio framework (required by audio_driver_coreaudio)
if(APPLE)
  target_link_libraries(orpheus_clip_composer_app PRIVATE
    "-framework CoreAudio"
    "-framework AudioToolbox"
    "-framework AudioUnit"
  )
endif()

# Output name
set_target_properties(orpheus_clip_composer_app PROPERTIES
  OUTPUT_NAME "OrpheusClipComposer"
)

# Apply compiler warnings (matches SDK build system)
# Note: JUCE 8.0.4 has some warnings we can't control, so we don't use orpheus_enable_warnings
# which enables -Werror. We'll re-enable once JUCE issues are resolved or we upgrade.
# include(${CMAKE_SOURCE_DIR}/cmake/CompilerWarnings.cmake)
# orpheus_enable_warnings(orpheus_clip_composer_app)
