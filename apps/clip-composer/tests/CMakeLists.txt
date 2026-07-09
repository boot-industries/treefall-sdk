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
  test_display_preferences.cpp
  test_clip_load_plan.cpp
  test_clip_replacement_policy.cpp
  test_clip_edit_dialog_layout_policy.cpp
  test_transport_sync.cpp
  test_device_swap.cpp
  test_sample_rate.cpp
  test_choke_policy.cpp
)

# C++20 required (matches main application)
target_compile_features(clip_composer_tests PRIVATE cxx_std_20)

# Include directories
target_include_directories(clip_composer_tests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../Source
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../packages/shmui-juce  # ShmUI audio visualization components
)

# Link against GoogleTest
target_link_libraries(clip_composer_tests
  PRIVATE
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
    occ_app_platform
)

# Link against Orpheus SDK modules
target_link_libraries(clip_composer_tests
  PRIVATE
    Orpheus::core
)

if(TARGET orpheus_audio_driver_manager)
  target_link_libraries(clip_composer_tests PRIVATE orpheus_audio_driver_manager)
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
    ${CMAKE_CURRENT_SOURCE_DIR}/../Source/UI/PreviewPlayer.cpp  # OCC151 T3: grid/dialog sync
)

# Register tests with CTest
include(GoogleTest)
gtest_discover_tests(clip_composer_tests
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
