/*
  ==============================================================================

    ShmUI.h
    Orpheus SDK Audio Visualization Components

    Professional audio visualization components for broadcast and theater.
    Design aesthetic: Neo-vintage console (Neve-inspired).

    Components:
    - AudioAnalyzer: Core audio analysis (FFT, RMS, frequency bands)
    - WaveformVisualizer: Multiple waveform display variants
    - BarVisualizer: Frequency band display with state animations
    - MatrixDisplay: LED-style matrix display with animations

    Usage:
    1. Include this header in your JUCE project
    2. Create visualization components
    3. Connect AudioAnalyzer to your audio source
    4. Call component methods to update state and appearance

    Threading:
    - AudioAnalyzer is thread-safe for audio/UI communication
    - Visualization components should be used on the message thread
    - Use juce::MessageManager::callAsync for cross-thread updates

  ==============================================================================
*/

#pragma once

// Core Audio
#include "Audio/AudioAnalyzer.h"

// Visualization Components
#include "Components/BarVisualizer.h"
#include "Components/MatrixDisplay.h"
#include "Components/WaveformVisualizer.h"

// Utilities
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
