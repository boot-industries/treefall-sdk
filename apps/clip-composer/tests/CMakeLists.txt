# SPDX-License-Identifier: MIT
# Clip Composer Test Suite (Sprint A4)

# Test executable
add_executable(clip_composer_tests
  test_audio_engine_init.cpp
  test_audio_engine_clips.cpp
  test_audio_engine_playback.cpp
  test_audio_engine_multi_tab.cpp
  test_session_manager.cpp
  test_performance.cpp
)

# C++20 required (matches main application)
target_compile_features(clip_composer_tests PRIVATE cxx_std_20)

# Include directories
# Use relative path from tests directory to SDK root (consistent with main CMakeLists.txt)
set(SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
target_include_directories(clip_composer_tests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../Source
    ${SDK_ROOT}/include  # Orpheus SDK headers
    ${SDK_ROOT}/packages/shmui-juce  # ShmUI audio visualization components
)

# Link against GoogleTest
target_link_libraries(clip_composer_tests
  PRIVATE
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
)

# Link against JUCE modules (needed for AudioEngine)
target_link_libraries(clip_composer_tests
  PRIVATE
    juce::juce_gui_extra
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_utils
    juce::juce_events
    juce::juce_dsp
)

# Link against shmui (needed for AudioEngine's AudioAnalyzer)
target_link_libraries(clip_composer_tests
  PRIVATE
    orpheus_shmui_juce
)

# Link against Orpheus SDK modules
# Use SDK_ROOT defined above for consistent path resolution
set(SDK_LIB_DIR "${SDK_ROOT}/build/src/core")
set(SDK_PLATFORM_LIB_DIR "${SDK_ROOT}/build/src/platform/audio_drivers")
set(SDK_CORE_LIB_DIR "${SDK_ROOT}/build/src")

target_link_libraries(clip_composer_tests
  PRIVATE
    ${SDK_CORE_LIB_DIR}/liborpheus_session.dylib
    ${SDK_LIB_DIR}/transport/liborpheus_transport.a
    ${SDK_LIB_DIR}/audio_io/liborpheus_audio_io.a
    ${SDK_LIB_DIR}/routing/liborpheus_routing.a
    ${SDK_PLATFORM_LIB_DIR}/liborpheus_audio_driver_coreaudio.a
)

# Link libsndfile
find_library(LIBSNDFILE_LIBRARY sndfile)
if(LIBSNDFILE_LIBRARY)
  target_link_libraries(clip_composer_tests PRIVATE ${LIBSNDFILE_LIBRARY})
else()
  message(FATAL_ERROR "libsndfile not found. Install with: brew install libsndfile")
endif()

# Link CoreAudio framework (macOS)
if(APPLE)
  target_link_libraries(clip_composer_tests PRIVATE
    "-framework CoreAudio"
    "-framework AudioToolbox"
    "-framework AudioUnit"
  )
endif()

# JUCE compile definitions
target_compile_definitions(clip_composer_tests
  PRIVATE
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_DISABLE_JUCE_VERSION_PRINTING=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
)

# Add source files from main application (we need AudioEngine implementation)
target_sources(clip_composer_tests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../Source/Audio/AudioEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../Source/Session/SessionManager.cpp
)

# Register tests with CTest
include(GoogleTest)
gtest_discover_tests(clip_composer_tests
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
