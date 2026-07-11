/*
  ==============================================================================

    ShmUI.h
    Created: shmui JUCE Component Library

    Main include header for all shmui JUCE components.

    This library provides JUCE C++ implementations for audio applications:
    - Audio visualization components (waveform, spectrum, orb, matrix)
    - Button system with style variants (Primary, Secondary, Ghost, etc.)
    - Transport controls (play/pause/stop/record)
    - Level meters (VU, PPM, Peak)
    - Waveform editor with trim/fade
    - Icon library (Transport, Audio, Mixer, Files, Edit, UI, Arrows, Status)

    Components:
    - AudioAnalyzer: Core audio analysis (FFT, RMS, frequency bands)
    - WaveformVisualizer: Multiple waveform display variants
    - WaveformEditor: Advanced waveform with trim/fade/seek
    - BarVisualizer: Frequency band display with state animations
    - OrbVisualizer: OpenGL shader-based 3D orb
    - MatrixDisplay: LED-style matrix display with animations
    - LevelMeter: Professional VU/PPM meter with peak hold
    - TransportBar: Full transport control strip

    Controls:
    - Button: Base button with style/size variants
    - IconButton: Icon-only button
    - TextButton: Text label button
    - ToggleButton: Stateful toggle
    - TransportButton: Play/Pause/Stop/Record buttons
    - MuteButton: Mute/Solo/Bypass toggles
    - ClipButton: Clip trigger with state machine

    Icons:
    - shmui::Icons::getIcon() / shmui::Icons::drawIcon()
    - Transport, Audio, Mixer, Files, Edit, UI, Arrows, Status categories

    Usage:
    1. Include this header in your JUCE project
    2. Create visualization/control components
    3. Connect AudioAnalyzer to your audio source
    4. Use callbacks for user interaction

    Threading:
    - AudioAnalyzer is thread-safe for audio/UI communication
    - UI components should be used on the message thread
    - Use juce::MessageManager::callAsync for cross-thread updates

    Sync to Orpheus SDK:
    scripts/sync-juce.sh   (mirrors juce/Source/ → packages/shmui-juce/,
                            formats to the SDK clang-format, regenerates the
                            package CMakeLists; --check verifies no drift)

  ==============================================================================
*/

#pragma once

//==============================================================================
// Design tokens + theme (must precede components — they read these by default)
#include "Utils/DesignTokens.h"
#include "Utils/ShmuiTheme.h"

//==============================================================================
// Icons
#include "Icons/Icons.h"

//==============================================================================
// Core Audio
#include "Audio/AudioAnalyzer.h"

//==============================================================================
// Controls (Button System)
#include "Controls/Button.h"
#include "Controls/ButtonStyles.h"
#include "Controls/ClipButton.h"
#include "Controls/IconButton.h"
#include "Controls/MuteButton.h"
#include "Controls/TextButton.h"
#include "Controls/ToggleButton.h"
#include "Controls/TransportButton.h"

//==============================================================================
// Visualization Components
#include "Components/AudioPlayerControls.h"
#include "Components/BarVisualizer.h"
#include "Components/LevelMeter.h"
#include "Components/MatrixDisplay.h"
#include "Components/MeterGroup.h"
#include "Components/ScrubBar.h"
#include "Components/TransportBar.h"
#include "Components/TransportContainer.h"
#include "Components/WaveformEditor.h"
#include "Components/WaveformVisualizer.h"

// OrbVisualizer is the only OpenGL component (orpheus_shmui_juce_gl). Pulling it
// into the umbrella unconditionally forces every consumer of <ShmUI.h> to link
// juce_opengl even when GL is disabled (SHMUI_JUCE_ENABLE_OPENGL=OFF). Guard it
// so GL-free consumers (Clip Composer, FreqFinder) don't need the module.
#if defined(SHMUI_JUCE_ENABLE_OPENGL) && SHMUI_JUCE_ENABLE_OPENGL
#include "Components/OrbVisualizer.h"
#endif

//==============================================================================
// Utilities
#include "Utils/AgentState.h"
#include "Utils/ColorUtils.h"
#include "Utils/Interpolation.h"
#include "Utils/RepaintThrottle.h"

namespace shmui {

/**
 * @brief Library version information.
 */
namespace Version {
constexpr int major = 2;
constexpr int minor = 4;
constexpr int patch = 0;
constexpr const char* string = "2.4.0";
} // namespace Version

} // namespace shmui
