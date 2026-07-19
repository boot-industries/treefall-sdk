/*
  ==============================================================================

    WaveformVisualizer.cpp
    Created: Shmui-to-JUCE Audio Visualization Port

    Implementation of waveform visualization components.

  ==============================================================================
*/

#include "WaveformVisualizer.h"
#include "../Utils/ColorUtils.h"

namespace shmui
{

//==============================================================================
// WaveformVisualizer

WaveformVisualizer::WaveformVisualizer()
{
    setOpaque(false);
}

void WaveformVisualizer::setData(const std::vector<float>& data)
{
    waveformData = data;
    repaint();
}

void WaveformVisualizer::setStyle(const WaveformStyle& newStyle)
{
    style = newStyle;
    repaint();
}

void WaveformVisualizer::setBarColour(const juce::Colour& colour)
{
    style.barColour = colour;
    repaint();
}

void WaveformVisualizer::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    renderWaveform(g, bounds);

    if (style.fadeEdges && style.fadeWidth > 0.0f)
    {
        applyEdgeFade(g, bounds);
    }
}

void WaveformVisualizer::renderWaveform(juce::Graphics& g,
                                        const juce::Rectangle<float>& bounds)
{
    if (waveformData.empty())
        return;

    const int barCount = getBarCount();
    if (barCount <= 0)
        return;

    const float centerY = bounds.getCentreY();
    const float maxHeight = bounds.getHeight() * style.heightScale;

    for (int i = 0; i < barCount; ++i)
    {
        // Map bar index to data index
        const int dataIndex = static_cast<int>((static_cast<float>(i) / barCount) *
                                                waveformData.size());
        const float value = (dataIndex >= 0 && dataIndex < static_cast<int>(waveformData.size()))
                           ? waveformData[dataIndex] : 0.0f;

        // Calculate bar dimensions
        const float barHeight = std::max(style.barHeight, value * maxHeight);
        const float x = bounds.getX() + i * (style.barWidth + style.barGap);
        const float y = centerY - barHeight / 2.0f;

        // Set alpha based on value
        const float alpha = style.alphaMin + value * (style.alphaMax - style.alphaMin);
        g.setColour(style.barColour.withAlpha(alpha));

        // Draw bar
        if (style.barRadius > 0.0f)
        {
            g.fillRoundedRectangle(x, y, style.barWidth, barHeight, style.barRadius);
        }
        else
        {
            g.fillRect(x, y, style.barWidth, barHeight);
        }
    }
}

void WaveformVisualizer::applyEdgeFade(juce::Graphics& g,
                                       const juce::Rectangle<float>& bounds)
{
    // Create edge fade using destination-out compositing
    // In JUCE, we simulate this by drawing transparent gradients

    const float fadePercent = std::min(0.2f, style.fadeWidth / bounds.getWidth());

    // Left fade
    juce::ColourGradient leftGradient = juce::ColourGradient::horizontal(
        juce::Colours::white, bounds.getX(),
        juce::Colours::transparentWhite, bounds.getX() + bounds.getWidth() * fadePercent);

    // Right fade
    juce::ColourGradient rightGradient = juce::ColourGradient::horizontal(
        juce::Colours::transparentWhite, bounds.getRight() - bounds.getWidth() * fadePercent,
        juce::Colours::white, bounds.getRight());

    // Note: True destination-out compositing requires custom blending
    // For now, this creates a visual approximation
    g.setGradientFill(leftGradient);
    g.fillRect(bounds.getX(), bounds.getY(),
               bounds.getWidth() * fadePercent, bounds.getHeight());

    g.setGradientFill(rightGradient);
    g.fillRect(bounds.getRight() - bounds.getWidth() * fadePercent, bounds.getY(),
               bounds.getWidth() * fadePercent, bounds.getHeight());
}

void WaveformVisualizer::mouseDown(const juce::MouseEvent& e)
{
    if (onBarClick && !waveformData.empty())
    {
        const int barIndex = static_cast<int>(e.position.x / (style.barWidth + style.barGap));
        const int dataIndex = static_cast<int>(
            (static_cast<float>(barIndex) / getBarCount()) * waveformData.size());

        if (dataIndex >= 0 && dataIndex < static_cast<int>(waveformData.size()))
        {
            onBarClick(dataIndex, waveformData[dataIndex]);
        }
    }
}

void WaveformVisualizer::resized()
{
    // Trigger repaint on resize
    repaint();
}

int WaveformVisualizer::getBarCount() const
{
    return static_cast<int>(getWidth() / (style.barWidth + style.barGap));
}

//==============================================================================
// ScrollingWaveformVisualizer

ScrollingWaveformVisualizer::ScrollingWaveformVisualizer()
{
    setOpaque(false);
}

ScrollingWaveformVisualizer::~ScrollingWaveformVisualizer()
{
    stopTimer();
}

void ScrollingWaveformVisualizer::setSpeed(float pixelsPerSecond)
{
    scrollSpeed = pixelsPerSecond;
}

void ScrollingWaveformVisualizer::setBarCount(int count)
{
    targetBarCount = count;
}

void ScrollingWaveformVisualizer::start()
{
    lastTime = juce::Time::currentTimeMillis();
    startTimerHz(60);  // 60 FPS
}

void ScrollingWaveformVisualizer::stop()
{
    stopTimer();
}

void ScrollingWaveformVisualizer::setDataSource(const std::vector<float>* source)
{
    dataSource = source;
    dataIndex = 0;
}

void ScrollingWaveformVisualizer::setSeed(uint32_t seed)
{
    randomSeed = seed;
}

void ScrollingWaveformVisualizer::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float centerY = bounds.getCentreY();
    const float maxHeight = bounds.getHeight() * 0.6f;  // From shmui

    // Draw all bars
    for (const auto& bar : bars)
    {
        if (bar.x >= 0.0f && bar.x + style.barWidth <= bounds.getWidth())
        {
            const float barHeight = std::max(style.barHeight, bar.height * maxHeight);
            const float y = centerY - barHeight / 2.0f;

            const float alpha = style.alphaMin + bar.height * (style.alphaMax - style.alphaMin);
            g.setColour(style.barColour.withAlpha(alpha));

            if (style.barRadius > 0.0f)
            {
                g.fillRoundedRectangle(bar.x, y, style.barWidth, barHeight, style.barRadius);
            }
            else
            {
                g.fillRect(bar.x, y, style.barWidth, barHeight);
            }
        }
    }

    // Apply edge fade
    if (style.fadeEdges && style.fadeWidth > 0.0f)
    {
        applyEdgeFade(g, bounds);
    }
}

void ScrollingWaveformVisualizer::resized()
{
    // Initialize bars if empty
    if (bars.empty())
    {
        const float step = style.barWidth + style.barGap;
        float currentX = static_cast<float>(getWidth());

        Interpolation::SeedRandom rng(randomSeed);

        while (currentX > -step)
        {
            bars.push_back({currentX, 0.2f + rng.next() * 0.6f});
            currentX -= step;
        }
    }
}

void ScrollingWaveformVisualizer::timerCallback()
{
    const int64_t currentTime = juce::Time::currentTimeMillis();
    const float deltaTime = static_cast<float>(currentTime - lastTime) / 1000.0f;
    lastTime = currentTime;

    // Move all bars to the left
    for (auto& bar : bars)
    {
        bar.x -= scrollSpeed * deltaTime;
    }

    // Remove bars that have scrolled off screen
    removeOldBars();

    // Add new bars as needed
    while (bars.empty() || bars.back().x < getWidth())
    {
        addNewBar();

        // Safety limit
        if (bars.size() > static_cast<size_t>(targetBarCount * 2))
            break;
    }

    repaint();
}

void ScrollingWaveformVisualizer::addNewBar()
{
    const float step = style.barWidth + style.barGap;
    const float lastX = bars.empty() ? static_cast<float>(getWidth())
                                     : bars.back().x + step;

    float newHeight;

    if (dataSource != nullptr && !dataSource->empty())
    {
        // Use data source
        newHeight = (*dataSource)[dataIndex % dataSource->size()];
        dataIndex = (dataIndex + 1) % static_cast<int>(dataSource->size());
    }
    else
    {
        // Generate pseudo-random value (from ScrollingWaveform in shmui)
        const float time = static_cast<float>(juce::Time::currentTimeMillis()) / 1000.0f;
        const float uniqueIndex = static_cast<float>(bars.size()) + time * 0.01f;

        const float wave1 = std::sin(uniqueIndex * 0.1f) * 0.2f;
        const float wave2 = std::cos(uniqueIndex * 0.05f) * 0.15f;
        const float randomComponent = Interpolation::seededRandom(
            static_cast<float>(randomSeed) * 10000.0f + uniqueIndex * 137.5f) * 0.4f;

        newHeight = juce::jlimit(0.1f, 0.9f, 0.3f + wave1 + wave2 + randomComponent);
    }

    bars.push_back({lastX, newHeight});
}

void ScrollingWaveformVisualizer::removeOldBars()
{
    const float step = style.barWidth + style.barGap;

    bars.erase(
        std::remove_if(bars.begin(), bars.end(),
                      [step](const Bar& bar) { return bar.x + step < 0.0f; }),
        bars.end());
}

//==============================================================================
// AudioScrubberVisualizer

AudioScrubberVisualizer::AudioScrubberVisualizer()
{
    // Scrubber defaults
    style.barWidth = 3.0f;
    style.barGap = 1.0f;
    style.barRadius = 1.0f;
    style.fadeEdges = false;

    setOpaque(false);
}

void AudioScrubberVisualizer::setCurrentTime(float time)
{
    if (!isDragging && duration > 0.0f)
    {
        currentTime = time;
        localProgress = time / duration;
        repaint();
    }
}

void AudioScrubberVisualizer::setDuration(float dur)
{
    duration = dur;
    if (duration > 0.0f)
    {
        localProgress = currentTime / duration;
    }
    repaint();
}

float AudioScrubberVisualizer::getProgress() const
{
    return localProgress;
}

void AudioScrubberVisualizer::setShowHandle(bool show)
{
    showHandle = show;
    repaint();
}

void AudioScrubberVisualizer::setPlayheadColour(const juce::Colour& colour)
{
    playheadColour = colour;
    repaint();
}

void AudioScrubberVisualizer::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    // Draw waveform
    WaveformVisualizer::paint(g);

    // Draw progress overlay
    const float progressX = bounds.getX() + localProgress * bounds.getWidth();

    // Played region (with overlay)
    g.setColour(playheadColour.withAlpha(0.2f));
    g.fillRect(bounds.getX(), bounds.getY(),
               progressX - bounds.getX(), bounds.getHeight());

    // Playhead line
    g.setColour(playheadColour);
    g.drawVerticalLine(static_cast<int>(progressX),
                       bounds.getY(), bounds.getBottom());

    // Handle
    if (showHandle)
    {
        const float handleSize = 16.0f;
        const float handleY = bounds.getCentreY();

        // Handle shadow
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.fillEllipse(progressX - handleSize / 2.0f + 1.0f,
                      handleY - handleSize / 2.0f + 1.0f,
                      handleSize, handleSize);

        // Handle
        g.setColour(playheadColour);
        g.fillEllipse(progressX - handleSize / 2.0f,
                      handleY - handleSize / 2.0f,
                      handleSize, handleSize);

        // Handle border
        g.setColour(juce::Colours::white);
        g.drawEllipse(progressX - handleSize / 2.0f,
                      handleY - handleSize / 2.0f,
                      handleSize, handleSize, 2.0f);
    }
}

void AudioScrubberVisualizer::mouseDown(const juce::MouseEvent& e)
{
    isDragging = true;
    handleScrub(e.position.x);
}

void AudioScrubberVisualizer::mouseDrag(const juce::MouseEvent& e)
{
    if (isDragging)
    {
        handleScrub(e.position.x);
    }
}

void AudioScrubberVisualizer::mouseUp(const juce::MouseEvent&)
{
    isDragging = false;
}

void AudioScrubberVisualizer::handleScrub(float x)
{
    const auto bounds = getLocalBounds().toFloat();
    const float clampedX = juce::jlimit(bounds.getX(), bounds.getRight(), x);

    localProgress = (clampedX - bounds.getX()) / bounds.getWidth();
    const float newTime = localProgress * duration;

    if (onSeek)
    {
        onSeek(newTime);
    }

    repaint();
}

//==============================================================================
// LiveWaveformVisualizer

LiveWaveformVisualizer::LiveWaveformVisualizer()
{
    // Live waveform defaults
    style.barWidth = 3.0f;
    style.barGap = 1.0f;
    style.barRadius = 1.0f;

    setOpaque(false);
}

LiveWaveformVisualizer::~LiveWaveformVisualizer()
{
    stopTimer();
}

void LiveWaveformVisualizer::setAudioAnalyzer(AudioAnalyzer* analyzer)
{
    audioAnalyzer = analyzer;
}

void LiveWaveformVisualizer::setActive(bool isActive)
{
    if (active != isActive)
    {
        active = isActive;

        if (active)
        {
            clearHistory();
            startTimer(updateRate);
        }
        else
        {
            stopTimer();
        }
    }
}

void LiveWaveformVisualizer::setHistorySize(int size)
{
    historySize = size;
}

void LiveWaveformVisualizer::setUpdateRate(int milliseconds)
{
    updateRate = milliseconds;

    if (isTimerRunning())
    {
        stopTimer();
        startTimer(updateRate);
    }
}

void LiveWaveformVisualizer::setSensitivity(float sens)
{
    sensitivity = sens;
}

void LiveWaveformVisualizer::setStyle(const WaveformStyle& newStyle)
{
    style = newStyle;
    repaint();
}

void LiveWaveformVisualizer::clearHistory()
{
    history.clear();
    repaint();
}

void LiveWaveformVisualizer::paint(juce::Graphics& g)
{
    if (history.empty())
        return;

    const auto bounds = getLocalBounds().toFloat();
    const float step = style.barWidth + style.barGap;
    const int barCount = static_cast<int>(bounds.getWidth() / step);
    const float centerY = bounds.getCentreY();
    const float maxHeight = bounds.getHeight() * 0.7f;

    // Render from right to left (newest first)
    for (int i = 0; i < barCount; ++i)
    {
        const int dataIndex = static_cast<int>(history.size()) - 1 - i;

        if (dataIndex >= 0 && dataIndex < static_cast<int>(history.size()))
        {
            const float value = history[dataIndex];
            const float x = bounds.getRight() - (i + 1) * step;
            const float barHeight = std::max(style.barHeight, value * maxHeight);
            const float y = centerY - barHeight / 2.0f;

            const float alpha = style.alphaMin + value * (style.alphaMax - style.alphaMin);
            g.setColour(style.barColour.withAlpha(alpha));

            if (style.barRadius > 0.0f)
            {
                g.fillRoundedRectangle(x, y, style.barWidth, barHeight, style.barRadius);
            }
            else
            {
                g.fillRect(x, y, style.barWidth, barHeight);
            }
        }
    }

    // Apply edge fade
    if (style.fadeEdges && style.fadeWidth > 0.0f)
    {
        const float fadePercent = std::min(0.2f, style.fadeWidth / bounds.getWidth());

        juce::ColourGradient leftGradient = juce::ColourGradient::horizontal(
            juce::Colours::white, bounds.getX(),
            juce::Colours::transparentWhite, bounds.getX() + bounds.getWidth() * fadePercent);

        juce::ColourGradient rightGradient = juce::ColourGradient::horizontal(
            juce::Colours::transparentWhite, bounds.getRight() - bounds.getWidth() * fadePercent,
            juce::Colours::white, bounds.getRight());

        g.setGradientFill(leftGradient);
        g.fillRect(bounds.getX(), bounds.getY(),
                   bounds.getWidth() * fadePercent, bounds.getHeight());

        g.setGradientFill(rightGradient);
        g.fillRect(bounds.getRight() - bounds.getWidth() * fadePercent, bounds.getY(),
                   bounds.getWidth() * fadePercent, bounds.getHeight());
    }
}

void LiveWaveformVisualizer::resized()
{
    repaint();
}

void LiveWaveformVisualizer::timerCallback()
{
    if (!audioAnalyzer || !active)
        return;

    // Get current RMS level
    const float level = audioAnalyzer->getRMSLevel() * sensitivity;
    const float clampedLevel = juce::jlimit(0.05f, 1.0f, level);

    // Add to history
    history.push_back(clampedLevel);

    // Trim history
    while (static_cast<int>(history.size()) > historySize)
    {
        history.erase(history.begin());
    }

    repaint();
}

} // namespace shmui
