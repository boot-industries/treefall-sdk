/*
  ==============================================================================

    WaveformVisualizer.h
    Created: Shmui-to-JUCE Audio Visualization Port

    Waveform visualization component ported from waveform.tsx.
    Supports static display, scrolling animation, and audio scrubbing.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Audio/AudioAnalyzer.h"
#include "../Utils/Interpolation.h"
#include <vector>

namespace shmui
{

/**
 * @brief Configuration for waveform visual appearance.
 *
 * Default values match shmui's waveform.tsx defaults.
 */
struct WaveformStyle
{
    float barWidth = 4.0f;          ///< Width of each bar in pixels
    float barHeight = 4.0f;         ///< Minimum bar height in pixels
    float barGap = 2.0f;            ///< Gap between bars in pixels
    float barRadius = 2.0f;         ///< Corner radius for rounded bars
    juce::Colour barColour = juce::Colours::black;  ///< Bar fill colour
    bool fadeEdges = true;          ///< Enable edge fade gradient
    float fadeWidth = 24.0f;        ///< Width of edge fade in pixels
    float alphaMin = 0.3f;          ///< Minimum alpha (for low values)
    float alphaMax = 1.0f;          ///< Maximum alpha (for high values)
    float heightScale = 0.8f;       ///< Maximum height as fraction of container
};

//==============================================================================

/**
 * @brief Static waveform display from data array.
 *
 * Displays a fixed waveform visualization from a provided data array.
 * Port of the Waveform component from waveform.tsx.
 */
class WaveformVisualizer : public juce::Component
{
public:
    WaveformVisualizer();
    ~WaveformVisualizer() override = default;

    //==============================================================================
    // Data

    /**
     * @brief Set waveform data to display.
     *
     * @param data Vector of normalized values (0-1)
     */
    void setData(const std::vector<float>& data);

    /**
     * @brief Get current waveform data.
     */
    const std::vector<float>& getData() const { return waveformData; }

    //==============================================================================
    // Style

    /**
     * @brief Set the visual style.
     */
    void setStyle(const WaveformStyle& newStyle);

    /**
     * @brief Get current style.
     */
    const WaveformStyle& getStyle() const { return style; }

    /**
     * @brief Set bar colour.
     */
    void setBarColour(const juce::Colour& colour);

    //==============================================================================
    // Callbacks

    /**
     * @brief Callback when a bar is clicked.
     *
     * @param index Bar index
     * @param value Bar value (0-1)
     */
    std::function<void(int index, float value)> onBarClick;

    //==============================================================================
    // Component overrides

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void resized() override;

protected:
    void renderWaveform(juce::Graphics& g, const juce::Rectangle<float>& bounds);
    void applyEdgeFade(juce::Graphics& g, const juce::Rectangle<float>& bounds);
    int getBarCount() const;

    std::vector<float> waveformData;
    WaveformStyle style;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformVisualizer)
};

//==============================================================================

/**
 * @brief Scrolling waveform with automatic animation.
 *
 * Displays bars that scroll across the display, creating a dynamic visualization.
 * Port of the ScrollingWaveform component from waveform.tsx.
 */
class ScrollingWaveformVisualizer : public WaveformVisualizer,
                                    public juce::Timer
{
public:
    ScrollingWaveformVisualizer();
    ~ScrollingWaveformVisualizer() override;

    //==============================================================================
    // Animation

    /**
     * @brief Set scroll speed in pixels per second.
     */
    void setSpeed(float pixelsPerSecond);

    /**
     * @brief Set target bar count.
     */
    void setBarCount(int count);

    /**
     * @brief Start scrolling animation.
     */
    void start();

    /**
     * @brief Stop scrolling animation.
     */
    void stop();

    /**
     * @brief Check if animation is running.
     */
    bool isRunning() const { return isTimerRunning(); }

    //==============================================================================
    // Data Source

    /**
     * @brief Set data source for new bars.
     *
     * If provided, new bars will use values from this data.
     * If not provided, generates pseudo-random values.
     */
    void setDataSource(const std::vector<float>* source);

    /**
     * @brief Set random seed for pseudo-random bar generation.
     */
    void setSeed(uint32_t seed);

    //==============================================================================
    // Component overrides

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void addNewBar();
    void removeOldBars();

    struct Bar
    {
        float x;
        float height;  // 0-1 normalized
    };

    std::vector<Bar> bars;
    float scrollSpeed = 50.0f;  // pixels per second
    int targetBarCount = 60;
    int64_t lastTime = 0;
    uint32_t randomSeed = 42;
    int dataIndex = 0;
    const std::vector<float>* dataSource = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScrollingWaveformVisualizer)
};

//==============================================================================

/**
 * @brief Audio scrubber with waveform display.
 *
 * Displays a waveform with a playhead that can be dragged to seek.
 * Port of the AudioScrubber component from waveform.tsx.
 */
class AudioScrubberVisualizer : public WaveformVisualizer
{
public:
    AudioScrubberVisualizer();
    ~AudioScrubberVisualizer() override = default;

    //==============================================================================
    // Playback Position

    /**
     * @brief Set current playback time.
     *
     * @param time Current time in seconds
     */
    void setCurrentTime(float time);

    /**
     * @brief Set total duration.
     *
     * @param duration Duration in seconds
     */
    void setDuration(float duration);

    /**
     * @brief Get current progress (0-1).
     */
    float getProgress() const;

    //==============================================================================
    // Appearance

    /**
     * @brief Show/hide the handle.
     */
    void setShowHandle(bool show);

    /**
     * @brief Set playhead colour.
     */
    void setPlayheadColour(const juce::Colour& colour);

    //==============================================================================
    // Callbacks

    /**
     * @brief Callback when user seeks.
     *
     * @param time New time in seconds
     */
    std::function<void(float time)> onSeek;

    //==============================================================================
    // Component overrides

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

private:
    void handleScrub(float x);

    float currentTime = 0.0f;
    float duration = 100.0f;
    float localProgress = 0.0f;
    bool isDragging = false;
    bool showHandle = true;
    juce::Colour playheadColour = juce::Colours::blue;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioScrubberVisualizer)
};

//==============================================================================

/**
 * @brief Live microphone waveform with history.
 *
 * Displays real-time audio input as a scrolling waveform with history.
 * Port of the LiveMicrophoneWaveform component from waveform.tsx.
 */
class LiveWaveformVisualizer : public juce::Component,
                                public juce::Timer
{
public:
    LiveWaveformVisualizer();
    ~LiveWaveformVisualizer() override;

    //==============================================================================
    // Audio Input

    /**
     * @brief Set the audio analyzer to get data from.
     */
    void setAudioAnalyzer(AudioAnalyzer* analyzer);

    /**
     * @brief Set active state (recording).
     */
    void setActive(bool active);

    /**
     * @brief Check if active.
     */
    bool isActive() const { return active; }

    //==============================================================================
    // Configuration

    /**
     * @brief Set history size (number of bars to keep).
     */
    void setHistorySize(int size);

    /**
     * @brief Set update rate in milliseconds.
     */
    void setUpdateRate(int milliseconds);

    /**
     * @brief Set sensitivity multiplier.
     */
    void setSensitivity(float sens);

    /**
     * @brief Set the visual style.
     */
    void setStyle(const WaveformStyle& newStyle);

    //==============================================================================
    // History Access

    /**
     * @brief Get recorded history data.
     */
    const std::vector<float>& getHistory() const { return history; }

    /**
     * @brief Clear history.
     */
    void clearHistory();

    //==============================================================================
    // Component overrides

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    AudioAnalyzer* audioAnalyzer = nullptr;
    std::vector<float> history;
    WaveformStyle style;

    bool active = false;
    int historySize = 150;
    int updateRate = 50;
    float sensitivity = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LiveWaveformVisualizer)
};

} // namespace shmui
