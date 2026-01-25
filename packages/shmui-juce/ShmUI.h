/*
  ==============================================================================

    ShmUI.h
    Created: Shmui-to-JUCE Audio Visualization Port

    Main include header for all shmui JUCE visualization components.

    This library provides JUCE C++ implementations of the ElevenLabs UI
    audio visualization components, translated from the original
    React/TypeScript shmui library.

    Components:
    - AudioAnalyzer: Core audio analysis (FFT, RMS, frequency bands)
    - WaveformVisualizer: Multiple waveform display variants
    - BarVisualizer: Frequency band display with state animations
    - OrbVisualizer: OpenGL shader-based 3D orb
    - MatrixDisplay: LED-style matrix display with animations
    - AudioPlayerControls: Transport controls (play/pause, time, speed)
    - ScrubBar: Timeline scrub bar for position control

    Usage:
    1. Include this header in your JUCE project
    2. Create visualization components
    3. Connect AudioAnalyzer to your audio source
    4. Call component methods to update state and appearance

    Threading:
    - AudioAnalyzer is thread-safe for audio/UI communication
    - Visualization components should be used on the message thread
    - Use juce::MessageManager::callAsync for cross-thread updates

    Sync to Orpheus SDK:
    rsync -av --delete ~/dev/shmui/juce/Source/ ~/dev/orpheus-sdk/packages/shmui-juce/

  ==============================================================================
*/

#pragma once

// Core Audio
#include "Audio/AudioAnalyzer.h"

// Visualization Components
#include "Components/AudioPlayerControls.h"
#include "Components/BarVisualizer.h"
#include "Components/MatrixDisplay.h"
#include "Components/OrbVisualizer.h"
#include "Components/ScrubBar.h"
#include "Components/WaveformVisualizer.h"

// Utilities
#include "Utils/AgentState.h"
#include "Utils/ColorUtils.h"
#include "Utils/Interpolation.h"

namespace shmui {

/**
 * @brief Library version information.
 */
namespace Version {
constexpr int major = 1;
constexpr int minor = 0;
constexpr int patch = 0;
constexpr const char* string = "1.0.0";
} // namespace Version

} // namespace shmui
