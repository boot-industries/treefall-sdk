# SPDX-License-Identifier: MIT
# Shmui Audio Visualization & UI Components v2.0
# Icons, Buttons, Visualizers, and Controls for audio applications
cmake_minimum_required(VERSION 3.22)

project(orpheus_shmui_juce LANGUAGES CXX)

add_library(orpheus_shmui_juce STATIC
  # Icons
  Icons/Icons.cpp

  # Controls (Button System)
  Controls/Button.cpp
  Controls/IconButton.cpp
  Controls/TextButton.cpp
  Controls/ToggleButton.cpp
  Controls/TransportButton.cpp
  Controls/MuteButton.cpp
  Controls/ClipButton.cpp

  # Audio Analysis
  Audio/AudioAnalyzer.cpp

  # Visualization Components
  Components/BarVisualizer.cpp
  Components/MatrixDisplay.cpp
  Components/WaveformVisualizer.cpp
  Components/WaveformEditor.cpp
  Components/LevelMeter.cpp
  Components/AudioPlayerControls.cpp
  Components/ScrubBar.cpp
  Components/TransportBar.cpp
)

target_include_directories(orpheus_shmui_juce PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link against necessary JUCE modules
target_link_libraries(orpheus_shmui_juce PUBLIC
  juce::juce_core
  juce::juce_audio_basics
  juce::juce_audio_formats  # For WaveformEditor file loading
  juce::juce_dsp            # For FFT in AudioAnalyzer
  juce::juce_gui_basics     # For Component, Graphics, etc.
  juce::juce_opengl         # For OrbVisualizer OpenGL rendering
)

target_compile_features(orpheus_shmui_juce PRIVATE cxx_std_20)
