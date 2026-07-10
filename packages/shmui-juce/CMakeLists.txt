# SPDX-License-Identifier: MIT
# ==============================================================================
# shmui JUCE component library — source-of-truth CMake
# ==============================================================================
# Shmui Audio Visualization & UI Components (v2.1)
# Icons, Buttons, Visualizers, and Controls for Orpheus SDK audio applications.
#
# This is the SOURCE OF TRUTH for the shmui-juce build. It is synced (verbatim,
# including this file adapted for the flattened layout) into the Orpheus SDK at
# packages/shmui-juce/ by scripts/sync-juce.sh. Edit here, not there.
#
# TARGETS
#   orpheus_shmui_juce        core: Icons, Controls, Audio, non-GL Components.
#                             Alias: Orpheus::shmui_juce
#   orpheus_shmui_juce_gl     optional: OrbVisualizer (OpenGL). Built only when
#                             SHMUI_JUCE_ENABLE_OPENGL=ON. Alias:
#                             Orpheus::shmui_juce_gl. Links juce_opengl.
#
# The core target does NOT link juce_opengl, so consumers that only need meters,
# waveforms, buttons, and analysis (FreqFinder, Clip Composer) avoid the GL
# dependency entirely. Only the agent/voice OrbVisualizer needs OpenGL.
#
# PER-COMPONENT JUCE MODULE NEEDS (documented for selective consumers)
#   Icons, Buttons (Button/Icon/Text/Toggle/Transport/Mute/Clip)  juce_gui_basics
#   AudioAnalyzer                                                  juce_dsp
#   LevelMeter, WaveformVisualizer, BarVisualizer, MatrixDisplay,
#     TransportBar, ScrubBar, AudioPlayerControls                 juce_gui_basics
#   WaveformEditor (file loading)                    juce_audio_formats + basics
#   OrbVisualizer (orpheus_shmui_juce_gl only)                    juce_opengl
# ==============================================================================
cmake_minimum_required(VERSION 3.22)

project(orpheus_shmui_juce LANGUAGES CXX)

option(SHMUI_JUCE_ENABLE_OPENGL
  "Build the OpenGL OrbVisualizer component (orpheus_shmui_juce_gl)" OFF)

# Sources are under Source/ in the shmui repo. The SDK-synced copy flattens this
# to the package root; sync-juce.sh rewrites SHMUI_SRC accordingly.
set(SHMUI_SRC "${CMAKE_CURRENT_SOURCE_DIR}")

# ------------------------------------------------------------------------------
# Core library (no OpenGL)
# ------------------------------------------------------------------------------
add_library(orpheus_shmui_juce STATIC
  # Icons
  ${SHMUI_SRC}/Icons/Icons.cpp

  # Controls (Button System)
  ${SHMUI_SRC}/Controls/Button.cpp
  ${SHMUI_SRC}/Controls/IconButton.cpp
  ${SHMUI_SRC}/Controls/TextButton.cpp
  ${SHMUI_SRC}/Controls/ToggleButton.cpp
  ${SHMUI_SRC}/Controls/TransportButton.cpp
  ${SHMUI_SRC}/Controls/MuteButton.cpp
  ${SHMUI_SRC}/Controls/ClipButton.cpp

  # Audio Analysis
  ${SHMUI_SRC}/Audio/AudioAnalyzer.cpp

  # Visualization Components (non-GL)
  ${SHMUI_SRC}/Components/BarVisualizer.cpp
  ${SHMUI_SRC}/Components/MatrixDisplay.cpp
  ${SHMUI_SRC}/Components/WaveformVisualizer.cpp
  ${SHMUI_SRC}/Components/WaveformEditor.cpp
  ${SHMUI_SRC}/Components/LevelMeter.cpp
  ${SHMUI_SRC}/Components/MeterGroup.cpp
  ${SHMUI_SRC}/Components/AudioPlayerControls.cpp
  ${SHMUI_SRC}/Components/ScrubBar.cpp
  ${SHMUI_SRC}/Components/TransportBar.cpp
  ${SHMUI_SRC}/Components/TransportContainer.cpp

  # Utilities
  ${SHMUI_SRC}/Utils/ShmuiTheme.cpp
)

target_include_directories(orpheus_shmui_juce PUBLIC ${SHMUI_SRC})

target_link_libraries(orpheus_shmui_juce PUBLIC
  juce::juce_core
  juce::juce_audio_basics
  juce::juce_audio_formats  # WaveformEditor file loading
  juce::juce_dsp            # AudioAnalyzer FFT
  juce::juce_gui_basics     # Component, Graphics
)

target_compile_features(orpheus_shmui_juce PUBLIC cxx_std_20)

# Short-form alias mirroring Orpheus::core.
if(NOT TARGET Orpheus::shmui_juce)
  add_library(Orpheus::shmui_juce ALIAS orpheus_shmui_juce)
endif()

# ------------------------------------------------------------------------------
# Optional OpenGL component (OrbVisualizer)
# ------------------------------------------------------------------------------
if(SHMUI_JUCE_ENABLE_OPENGL)
  add_library(orpheus_shmui_juce_gl STATIC
    ${SHMUI_SRC}/Components/OrbVisualizer.cpp
  )
  target_include_directories(orpheus_shmui_juce_gl PUBLIC ${SHMUI_SRC})
  target_link_libraries(orpheus_shmui_juce_gl PUBLIC
    orpheus_shmui_juce
    juce::juce_opengl
  )
  # PUBLIC so this target's own sources AND consumers that link it compile the
  # OpenGL OrbVisualizer include in <ShmUI.h> and the juce_opengl include in the
  # JuceHeader shim. GL-free consumers never see the define, so they don't pull
  # juce_opengl.
  target_compile_definitions(orpheus_shmui_juce_gl PUBLIC SHMUI_JUCE_ENABLE_OPENGL=1)
  target_compile_features(orpheus_shmui_juce_gl PUBLIC cxx_std_20)

  if(NOT TARGET Orpheus::shmui_juce_gl)
    add_library(Orpheus::shmui_juce_gl ALIAS orpheus_shmui_juce_gl)
  endif()
endif()
