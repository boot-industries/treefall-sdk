# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.22)

project(orpheus_shmui_juce LANGUAGES CXX)

add_library(orpheus_shmui_juce STATIC
  Audio/AudioAnalyzer.cpp
  Components/BarVisualizer.cpp
  Components/MatrixDisplay.cpp
  Components/OrbVisualizer.cpp
  Components/WaveformVisualizer.cpp
)

target_include_directories(orpheus_shmui_juce PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link against necessary JUCE modules
target_link_libraries(orpheus_shmui_juce PUBLIC
  juce::juce_core
  juce::juce_audio_basics
  juce::juce_dsp          # For FFT in AudioAnalyzer
  juce::juce_gui_basics   # For Component, Graphics, etc.
  juce::juce_opengl       # For OrbVisualizer
)

target_compile_features(orpheus_shmui_juce PRIVATE cxx_std_20)
