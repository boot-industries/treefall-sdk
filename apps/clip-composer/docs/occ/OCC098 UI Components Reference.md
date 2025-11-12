# OCC098 - UI Components Reference

**Version:** 1.0
**Date:** 2025-10-30
**Status:** Reference Documentation

Complete JUCE component implementations for Orpheus Clip Composer.

---

## Overview

This document provides detailed JUCE component implementations for the OCC application. All components follow JUCE best practices and the OCC threading model (Message Thread for UI, Audio Thread for processing, Background Thread for I/O).

---

## ClipGrid Component (960 Buttons)

### Overview

The ClipGrid manages 960 clip buttons organized as:

- **10 columns × 12 rows** = 120 buttons per tab
- **8 tabs** = 960 total buttons

### Implementation

```cpp
// ClipGrid.h
#pragma once
#include <JuceHeader.h>
#include "ClipButton.h"
#include "SessionManager.h"
#include "TransportController.h"

class ClipGrid : public juce::Component
{
public:
    ClipGrid(TransportController* transport, SessionManager* session)
        : transportController(transport), sessionManager(session)
    {
        // Create 10×12 = 120 buttons per tab
        for (int row = 0; row < 12; ++row) {
            for (int col = 0; col < 10; ++col) {
                auto button = std::make_unique<ClipButton>();
                button->onClick = [this, row, col] { onClipTriggered(row, col); };
                buttons.push_back(std::move(button));
                addAndMakeVisible(buttons.back().get());
            }
        }
    }

    void resized() override
    {
        // Use juce::Grid for responsive layout
        juce::Grid grid;
        grid.templateColumns = juce::Array<juce::Grid::TrackInfo>(10, juce::Grid::TrackInfo(1_fr));
        grid.templateRows = juce::Array<juce::Grid::TrackInfo>(12, juce::Grid::TrackInfo(1_fr));

        for (auto& button : buttons) {
            grid.items.add(juce::GridItem(button.get()).withMargin(2));
        }

        grid.performLayout(getLocalBounds());
    }

    void setCurrentTab(int tabIndex)
    {
        if (tabIndex >= 0 && tabIndex < 8) {
            currentTab = tabIndex;
            refreshButtons();
        }
    }

    void refreshButtons()
    {
        for (int i = 0; i < 120; ++i) {
            auto clip = sessionManager->getClipAtButton(currentTab, i);
            buttons[i]->setClip(clip);
        }
    }

    void highlightPlayingClip(orpheus::ClipHandle handle)
    {
        for (auto& button : buttons) {
            if (button->getClipHandle() == handle) {
                button->setPlaying(true);
            }
        }
    }

    void clearAllHighlights()
    {
        for (auto& button : buttons) {
            button->setPlaying(false);
        }
    }

    void setButtonClip(int tabIndex, int buttonIndex, const ClipMetadata& clip)
    {
        if (tabIndex == currentTab && buttonIndex >= 0 && buttonIndex < 120) {
            buttons[buttonIndex]->setClip(clip);
        }
    }

private:
    void onClipTriggered(int row, int col)
    {
        int buttonIndex = row * 10 + col;
        auto clip = sessionManager->getClipAtButton(currentTab, buttonIndex);

        if (clip.isValid()) {
            transportController->startClip(clip.handle);
        }
    }

    TransportController* transportController;
    SessionManager* sessionManager;
    std::vector<std::unique_ptr<ClipButton>> buttons;
    int currentTab = 0;
};
```

---

## WaveformDisplay Component

### Overview

Displays audio waveform with playback position and trim markers. Rendering happens on background thread.

### Implementation

```cpp
// WaveformDisplay.h
#pragma once
#include <JuceHeader.h>
#include "ClipMetadata.h"

class WaveformDisplay : public juce::Component, private juce::Timer
{
public:
    WaveformDisplay()
    {
        startTimerHz(30);  // 30 Hz refresh for playback position
    }

    void setAudioFile(orpheus::IAudioFileReader* reader, const ClipMetadata& metadata)
    {
        currentMetadata = metadata;

        // Pre-render waveform on background thread
        ThreadPool::getInstance()->addJob([this, reader, metadata]() {
            renderWaveform(reader, metadata);

            // Update UI on message thread
            juce::MessageManager::callAsync([this]() {
                repaint();
            });
        });
    }

    void setPlaybackPosition(int64_t positionSamples)
    {
        playbackPosition = positionSamples;
        // Will repaint on next timer callback
    }

    void setTrimMarkers(int64_t trimInSamples, int64_t trimOutSamples)
    {
        trimIn = trimInSamples;
        trimOut = trimOutSamples;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black);

        if (waveformData.empty()) {
            g.setColour(juce::Colours::grey);
            g.drawText("Loading...", getLocalBounds(), juce::Justification::centred);
            return;
        }

        // Draw waveform
        g.setColour(juce::Colours::lightblue);

        float width = static_cast<float>(getWidth());
        float height = static_cast<float>(getHeight());
        float midY = height / 2.0f;

        for (size_t i = 0; i < waveformData.size(); ++i) {
            float x = (i / static_cast<float>(waveformData.size())) * width;
            float y = midY + (waveformData[i] * midY * 0.9f);

            g.drawLine(x, midY, x, y, 1.0f);
        }

        // Draw trim markers
        if (trimIn > 0) {
            float trimInX = (trimIn / static_cast<float>(currentMetadata.durationSamples)) * width;
            g.setColour(juce::Colours::yellow);
            g.drawVerticalLine(static_cast<int>(trimInX), 0.0f, height);
        }

        if (trimOut < currentMetadata.durationSamples) {
            float trimOutX = (trimOut / static_cast<float>(currentMetadata.durationSamples)) * width;
            g.setColour(juce::Colours::yellow);
            g.drawVerticalLine(static_cast<int>(trimOutX), 0.0f, height);
        }

        // Draw playback position
        if (playbackPosition >= 0) {
            float playheadX = (playbackPosition / static_cast<float>(currentMetadata.durationSamples)) * width;
            g.setColour(juce::Colours::white);
            g.drawVerticalLine(static_cast<int>(playheadX), 0.0f, height);
        }
    }

    void timerCallback() override
    {
        // Repaint for playback position updates
        if (playbackPosition >= 0) {
            repaint();
        }
    }

private:
    void renderWaveform(orpheus::IAudioFileReader* reader, const ClipMetadata& metadata)
    {
        // Read entire file and downsample for display
        const size_t displayWidth = 800;  // pixels
        const size_t samplesPerPixel = metadata.durationSamples / displayWidth;

        waveformData.resize(displayWidth);

        // Simplified - actual implementation would batch reads
        for (size_t i = 0; i < displayWidth; ++i) {
            float maxSample = 0.0f;
            // Read and find peak in this pixel's range
            waveformData[i] = maxSample;
        }
    }

    std::vector<float> waveformData;
    ClipMetadata currentMetadata;
    int64_t playbackPosition = -1;
    int64_t trimIn = 0;
    int64_t trimOut = 0;
};
```

---

## TransportControls Component

### Overview

Provides play, stop, panic, and transport position display.

### Implementation

```cpp
// TransportControls.h
#pragma once
#include <JuceHeader.h>
#include "TransportController.h"

class TransportControls : public juce::Component
{
public:
    TransportControls(TransportController* transport)
        : transportController(transport)
    {
        // Play button
        playButton.setButtonText("Play");
        playButton.onClick = [this] { onPlayClicked(); };
        addAndMakeVisible(playButton);

        // Stop button
        stopButton.setButtonText("Stop");
        stopButton.onClick = [this] { onStopClicked(); };
        addAndMakeVisible(stopButton);

        // Panic button (stop all)
        panicButton.setButtonText("PANIC");
        panicButton.onClick = [this] { onPanicClicked(); };
        panicButton.setColour(juce::TextButton::buttonColourId, juce::Colours::red);
        addAndMakeVisible(panicButton);

        // Position display
        positionLabel.setText("00:00:00.000", juce::dontSendNotification);
        positionLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(positionLabel);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();

        playButton.setBounds(bounds.removeFromLeft(80).reduced(2));
        stopButton.setBounds(bounds.removeFromLeft(80).reduced(2));
        panicButton.setBounds(bounds.removeFromLeft(100).reduced(2));
        positionLabel.setBounds(bounds.reduced(2));
    }

    void updatePosition(orpheus::TransportPosition pos)
    {
        // Format as HH:MM:SS.mmm
        int64_t totalMs = (pos.samples * 1000) / pos.sampleRate;
        int hours = totalMs / 3600000;
        int minutes = (totalMs % 3600000) / 60000;
        int seconds = (totalMs % 60000) / 1000;
        int milliseconds = totalMs % 1000;

        auto timeString = juce::String::formatted("%02d:%02d:%02d.%03d", hours, minutes, seconds, milliseconds);
        positionLabel.setText(timeString, juce::dontSendNotification);
    }

    void reset()
    {
        positionLabel.setText("00:00:00.000", juce::dontSendNotification);
    }

private:
    void onPlayClicked()
    {
        // Play current clip (if any selected)
        // This is simplified - actual implementation would track selected clip
    }

    void onStopClicked()
    {
        // Stop current clip (if any playing)
        // This is simplified - actual implementation would track playing clips
    }

    void onPanicClicked()
    {
        transportController->stopAllClips();
        reset();
    }

    TransportController* transportController;
    juce::TextButton playButton;
    juce::TextButton stopButton;
    juce::TextButton panicButton;
    juce::Label positionLabel;
};
```

---

## RoutingPanel Component

### Overview

Displays and controls 4 clip groups with gain/mute/solo controls.

### Implementation

```cpp
// RoutingPanel.h
#pragma once
#include <JuceHeader.h>
#include "RoutingMatrix.h"

class RoutingPanel : public juce::Component
{
public:
    RoutingPanel(RoutingMatrix* routing)
        : routingMatrix(routing)
    {
        // Create 4 clip group controls
        for (int i = 0; i < 4; ++i) {
            auto groupControl = std::make_unique<ClipGroupControl>(i, routing);
            groupControls.push_back(std::move(groupControl));
            addAndMakeVisible(groupControls.back().get());
        }

        // Master gain/mute
        masterGainSlider.setRange(-48.0, 12.0, 0.1);
        masterGainSlider.setValue(0.0);
        masterGainSlider.onValueChange = [this] { onMasterGainChanged(); };
        addAndMakeVisible(masterGainSlider);

        masterMuteButton.setButtonText("Mute Master");
        masterMuteButton.onClick = [this] { onMasterMuteClicked(); };
        addAndMakeVisible(masterMuteButton);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();

        // Layout 4 group controls horizontally
        int groupWidth = bounds.getWidth() / 4;
        for (int i = 0; i < 4; ++i) {
            groupControls[i]->setBounds(bounds.removeFromLeft(groupWidth).reduced(2));
        }

        // Master controls at bottom
        auto masterBounds = bounds.removeFromBottom(60);
        masterGainSlider.setBounds(masterBounds.removeFromTop(30));
        masterMuteButton.setBounds(masterBounds);
    }

    void refresh()
    {
        for (auto& control : groupControls) {
            control->refresh();
        }

        masterGainSlider.setValue(routingMatrix->getMasterGain(), juce::dontSendNotification);
        masterMuteButton.setToggleState(routingMatrix->getMasterMute(), juce::dontSendNotification);
    }

private:
    void onMasterGainChanged()
    {
        routingMatrix->setMasterGain(static_cast<float>(masterGainSlider.getValue()));
    }

    void onMasterMuteClicked()
    {
        bool mute = masterMuteButton.getToggleState();
        routingMatrix->setMasterMute(mute);
    }

    class ClipGroupControl : public juce::Component
    {
    public:
        ClipGroupControl(int index, RoutingMatrix* routing)
            : groupIndex(index), routingMatrix(routing)
        {
            // Group name label
            nameLabel.setText(routing->getGroupName(index), juce::dontSendNotification);
            addAndMakeVisible(nameLabel);

            // Gain slider
            gainSlider.setRange(-48.0, 12.0, 0.1);
            gainSlider.setSliderStyle(juce::Slider::LinearVertical);
            gainSlider.onValueChange = [this] { onGainChanged(); };
            addAndMakeVisible(gainSlider);

            // Mute button
            muteButton.setButtonText("M");
            muteButton.onClick = [this] { onMuteClicked(); };
            addAndMakeVisible(muteButton);

            // Solo button
            soloButton.setButtonText("S");
            soloButton.onClick = [this] { onSoloClicked(); };
            addAndMakeVisible(soloButton);

            refresh();
        }

        void resized() override
        {
            auto bounds = getLocalBounds();
            nameLabel.setBounds(bounds.removeFromTop(20));
            gainSlider.setBounds(bounds.removeFromTop(bounds.getHeight() - 30));

            auto buttonBounds = bounds;
            muteButton.setBounds(buttonBounds.removeFromLeft(buttonBounds.getWidth() / 2));
            soloButton.setBounds(buttonBounds);
        }

        void refresh()
        {
            gainSlider.setValue(routingMatrix->getGroupGain(groupIndex), juce::dontSendNotification);
            muteButton.setToggleState(routingMatrix->getGroupMute(groupIndex), juce::dontSendNotification);
            soloButton.setToggleState(routingMatrix->getGroupSolo(groupIndex), juce::dontSendNotification);
        }

    private:
        void onGainChanged()
        {
            routingMatrix->setGroupGain(groupIndex, static_cast<float>(gainSlider.getValue()));
        }

        void onMuteClicked()
        {
            routingMatrix->setGroupMute(groupIndex, muteButton.getToggleState());
        }

        void onSoloClicked()
        {
            routingMatrix->setGroupSolo(groupIndex, soloButton.getToggleState());
        }

        int groupIndex;
        RoutingMatrix* routingMatrix;
        juce::Label nameLabel;
        juce::Slider gainSlider;
        juce::ToggleButton muteButton;
        juce::ToggleButton soloButton;
    };

    RoutingMatrix* routingMatrix;
    std::vector<std::unique_ptr<ClipGroupControl>> groupControls;
    juce::Slider masterGainSlider;
    juce::ToggleButton masterMuteButton;
};
```

---

## PerformanceMonitor Component

### Overview

Displays CPU usage, latency, and performance diagnostics.

### Implementation

```cpp
// PerformanceMonitor.h
#pragma once
#include <JuceHeader.h>
#include "AudioEngine.h"

class PerformanceMonitor : public juce::Component, private juce::Timer
{
public:
    PerformanceMonitor(AudioEngine* engine)
        : audioEngine(engine)
    {
        startTimerHz(10);  // 10 Hz refresh

        cpuMeter.setText("CPU: 0%", juce::dontSendNotification);
        addAndMakeVisible(cpuMeter);

        latencyLabel.setText("Latency: 0ms", juce::dontSendNotification);
        addAndMakeVisible(latencyLabel);

        activeClipsLabel.setText("Active Clips: 0", juce::dontSendNotification);
        addAndMakeVisible(activeClipsLabel);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        cpuMeter.setBounds(bounds.removeFromTop(20));
        latencyLabel.setBounds(bounds.removeFromTop(20));
        activeClipsLabel.setBounds(bounds.removeFromTop(20));
    }

    void timerCallback() override
    {
        // Read atomic values (lock-free)
        auto cpuMicros = audioEngine->getCpuUsageMicroseconds();
        auto latencyMs = audioEngine->getLatencyMilliseconds();
        auto activeClips = audioEngine->getActiveClipCount();

        // Calculate CPU percentage (at 512 samples, 48kHz = 10.67ms budget)
        float cpuPercent = (cpuMicros / 10670.0f) * 100.0f;

        // Update UI
        cpuMeter.setText(juce::String::formatted("CPU: %.1f%%", cpuPercent), juce::dontSendNotification);
        latencyLabel.setText(juce::String::formatted("Latency: %.2fms", latencyMs), juce::dontSendNotification);
        activeClipsLabel.setText(juce::String::formatted("Active Clips: %d", activeClips), juce::dontSendNotification);

        // Color-code CPU usage
        if (cpuPercent > 80.0f) {
            cpuMeter.setColour(juce::Label::textColourId, juce::Colours::red);
        } else if (cpuPercent > 50.0f) {
            cpuMeter.setColour(juce::Label::textColourId, juce::Colours::yellow);
        } else {
            cpuMeter.setColour(juce::Label::textColourId, juce::Colours::green);
        }
    }

private:
    AudioEngine* audioEngine;
    juce::Label cpuMeter;
    juce::Label latencyLabel;
    juce::Label activeClipsLabel;
};
```

---

## Related Documentation

- **OCC096** - SDK Integration Patterns (SDK interaction code)
- **OCC097** - Session Format (session loading/saving)
- **OCC023** - Component Architecture (5-layer architecture)
- **OCC024** - User Interaction Flows (UI workflows)

---

**Last Updated:** 2025-10-30
**Maintainer:** OCC Development Team
**Status:** Reference Documentation
