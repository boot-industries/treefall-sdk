# shmui JUCE Integration Skill

## Overview

Patterns and guidance for integrating shmui-juce visualization components into Orpheus SDK applications. The shmui-juce package provides audio visualization components ported from the ElevenLabs UI library.

## Package Location

```
packages/shmui-juce/
├── ShmUI.h              # Main include header
├── Audio/
│   └── AudioAnalyzer.h  # Thread-safe audio analysis
├── Components/
│   ├── BarVisualizer.h  # Frequency band display
│   ├── WaveformVisualizer.h
│   ├── MatrixDisplay.h  # LED grid visualization
│   └── OrbVisualizer.h  # OpenGL 3D orb
└── Utils/
    ├── ColorUtils.h
    └── Interpolation.h
```

## Available Components

### AudioAnalyzer

Thread-safe FFT, RMS, and peak level analysis.

```cpp
#include <ShmUI.h>

// Create analyzer
auto analyzer = std::make_unique<shmui::AudioAnalyzer>();

// In audio callback (real-time safe)
analyzer->processBlock(buffer);

// On UI thread
float peakLevel = analyzer->getPeakLevel();  // 0.0 to 1.0
float rmsLevel = analyzer->getRMSLevel();    // 0.0 to 1.0
```

### BarVisualizer

Multi-band frequency display with state-based animations.

```cpp
#include <ShmUI.h>

auto visualizer = std::make_unique<shmui::BarVisualizer>();
visualizer->setBarCount(12);
visualizer->setBarColour(juce::Colour(0xFFc8e9fd));  // Cyan
visualizer->setBackgroundColour(juce::Colour(0xFF000000));
visualizer->setHeightRange(10.0f, 100.0f);
visualizer->setAudioAnalyzer(analyzer.get());
addAndMakeVisible(visualizer.get());
```

**Agent States** (for AI/voice interfaces):
- `AgentState::Idle` - Inactive
- `AgentState::Listening` - Waiting for input
- `AgentState::Thinking` - Processing
- `AgentState::Speaking` - Output active

### WaveformVisualizer

Multiple waveform display variants:
- Static waveform (full file)
- Scrolling waveform (live)
- Scrubber with playhead

```cpp
auto waveform = std::make_unique<shmui::WaveformVisualizer>();
waveform->setWaveformColour(juce::Colour(0xFF3ab7a8));
waveform->setBackgroundColour(juce::Colour(0xFF151515));
```

### MatrixDisplay

LED-style grid visualization with brightness gradient.

```cpp
auto matrix = std::make_unique<shmui::MatrixDisplay>();
matrix->setGridSize(16, 8);  // 16 columns, 8 rows
matrix->setColors(greenColor, yellowColor, redColor);
```

## Threading Model

**AudioAnalyzer**: Safe for audio thread (lock-free internal design)
- Call `processBlock()` from audio callback
- Call getters from message thread

**All Visualizers**: Message thread only
- Create, configure, and destroy on message thread
- Use `juce::MessageManager::callAsync()` for cross-thread updates

## Integration Pattern

```cpp
// In MainComponent.h
#include <ShmUI.h>

class MainComponent : public juce::Component {
private:
    std::unique_ptr<shmui::BarVisualizer> m_barVisualizer;
};

// In MainComponent.cpp
MainComponent::MainComponent() {
    // Create and configure
    m_barVisualizer = std::make_unique<shmui::BarVisualizer>();
    m_barVisualizer->setBarCount(12);
    m_barVisualizer->setBarColour(juce::Colour(OCC::Design::kAccentCyan));
    m_barVisualizer->setBackgroundColour(juce::Colour(OCC::Design::kBgPrimary));
    addAndMakeVisible(m_barVisualizer.get());

    // Connect to analyzer after audio engine starts
    if (m_audioEngine && m_audioEngine->getAudioAnalyzer()) {
        m_barVisualizer->setAudioAnalyzer(m_audioEngine->getAudioAnalyzer());
    }
}

void MainComponent::resized() {
    auto bounds = getLocalBounds();
    auto visualizerArea = bounds.removeFromRight(60);
    m_barVisualizer->setBounds(visualizerArea);
}
```

## Design Token Integration

Use OCC::Design tokens for consistent styling:

```cpp
#include "UI/DesignTokens.h"

// Background colors
m_visualizer->setBackgroundColour(juce::Colour(OCC::Design::kBgPrimary));

// Accent colors
m_visualizer->setBarColour(juce::Colour(OCC::Design::kAccentCyan));

// Metering colors
juce::Colour greenColor(OCC::Design::kMeterGreen);
juce::Colour yellowColor(OCC::Design::kMeterYellow);
juce::Colour redColor(OCC::Design::kMeterRed);
```

## Color Utilities

```cpp
#include <ShmUI.h>

// Interpolate between colors
auto blended = shmui::ColorUtils::lerp(color1, color2, 0.5f);

// Create gradient
auto gradient = shmui::ColorUtils::createGradient(startColor, endColor, steps);
```

## Common Patterns

### VU Meter Replacement

Replace simple VU meters with BarVisualizer for richer visualization:

```cpp
// Before (VUMeterComponent)
auto vuMeter = std::make_unique<VUMeterComponent>();

// After (BarVisualizer)
auto visualizer = std::make_unique<shmui::BarVisualizer>();
visualizer->setBarCount(12);
visualizer->setAudioAnalyzer(analyzer);
```

### Responsive Sizing

```cpp
void resized() {
    // Give visualizer proportional width
    auto visualizerWidth = juce::jmax(40, getWidth() / 20);
    auto visualizerArea = bounds.removeFromRight(visualizerWidth);
    m_barVisualizer->setBounds(visualizerArea);
}
```

## Trigger Patterns

This skill activates for:
- Files containing `shmui::` namespace references
- Tasks involving audio visualization
- Component integration work in Clip Composer or SDK apps

## References

- Package source: `packages/shmui-juce/`
- Main header: `packages/shmui-juce/ShmUI.h`
- Design tokens (consumer example): `Source/UI/DesignTokens.h` in the Clip Composer repo (`~/dev/clip-composer`)
