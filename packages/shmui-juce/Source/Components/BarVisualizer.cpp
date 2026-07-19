/*
  ==============================================================================

    BarVisualizer.cpp
    Created: Shmui-to-JUCE Audio Visualization Port

    Implementation of the bar visualizer component.

  ==============================================================================
*/

#include "BarVisualizer.h"
#include "../Utils/Interpolation.h"

namespace shmui
{

BarVisualizer::BarVisualizer()
{
    volumeBands.resize(barCount, 0.0f);
    fakeVolumeBands.resize(barCount, 0.2f);

    setOpaque(false);
    startTimerHz(60);
}

BarVisualizer::~BarVisualizer()
{
    stopTimer();
}

void BarVisualizer::setAudioAnalyzer(AudioAnalyzer* analyzer)
{
    audioAnalyzer = analyzer;
}

void BarVisualizer::setVolumeBands(const std::vector<float>& bands)
{
    volumeBands = bands;

    // Resize if needed
    while (volumeBands.size() < static_cast<size_t>(barCount))
    {
        volumeBands.push_back(0.0f);
    }

    repaint();
}

void BarVisualizer::setAgentState(AgentState state)
{
    if (agentState != state)
    {
        agentState = state;
        animationStep = 0;
        lastAnimTime = juce::Time::currentTimeMillis();

        // Generate animation sequence based on state
        switch (state)
        {
            case AgentState::Connecting:
            case AgentState::Initializing:
                generateConnectingSequence();
                break;

            case AgentState::Listening:
            case AgentState::Thinking:
                generateListeningSequence();
                break;

            default:
                animationSequence.clear();
                break;
        }

        repaint();
    }
}

void BarVisualizer::setBarCount(int count)
{
    barCount = std::max(1, count);
    volumeBands.resize(barCount, 0.0f);
    fakeVolumeBands.resize(barCount, 0.2f);

    // Regenerate animation sequences
    if (agentState == AgentState::Connecting || agentState == AgentState::Initializing)
    {
        generateConnectingSequence();
    }
    else if (agentState == AgentState::Listening || agentState == AgentState::Thinking)
    {
        generateListeningSequence();
    }

    repaint();
}

void BarVisualizer::setHeightRange(float minPct, float maxPct)
{
    minHeightPct = juce::jlimit(0.0f, 100.0f, minPct);
    maxHeightPct = juce::jlimit(minHeightPct, 100.0f, maxPct);
    repaint();
}

void BarVisualizer::setDemoMode(bool demo)
{
    demoMode = demo;
    repaint();
}

void BarVisualizer::setCenterAlign(bool center)
{
    centerAlign = center;
    repaint();
}

void BarVisualizer::setBarColour(const juce::Colour& colour)
{
    barColour = colour;
    repaint();
}

void BarVisualizer::setHighlightColour(const juce::Colour& colour)
{
    highlightColour = colour;
    repaint();
}

void BarVisualizer::setBackgroundColour(const juce::Colour& colour)
{
    backgroundColour = colour;
    repaint();
}

void BarVisualizer::setGradientMode(bool gradient)
{
    gradientMode = gradient;
    repaint();
}

void BarVisualizer::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced(16.0f);

    // Background
    g.setColour(backgroundColour);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);

    if (barCount <= 0)
        return;

    // Get current data
    const auto& data = demoMode ? fakeVolumeBands : volumeBands;

    // Get highlighted indices
    const auto highlighted = getHighlightedIndices();

    // Calculate bar dimensions
    const float gap = 6.0f;  // gap-1.5 = 6px
    const float totalGap = gap * (barCount - 1);
    const float availableWidth = bounds.getWidth() - totalGap;
    const float barWidth = juce::jlimit(8.0f, 12.0f, availableWidth / barCount);

    // Calculate total width and starting position for centering
    const float totalWidth = barCount * barWidth + totalGap;
    float startX = bounds.getX() + (bounds.getWidth() - totalWidth) / 2.0f;

    // Draw bars
    for (int i = 0; i < barCount; ++i)
    {
        const float volume = (i < static_cast<int>(data.size())) ? data[i] : 0.0f;
        const float heightPct = juce::jlimit(minHeightPct, maxHeightPct, volume * 100.0f + 5.0f);
        const float barHeight = bounds.getHeight() * (heightPct / 100.0f);

        const float x = startX + i * (barWidth + gap);
        float y;

        if (centerAlign)
        {
            y = bounds.getCentreY() - barHeight / 2.0f;
        }
        else
        {
            y = bounds.getBottom() - barHeight;
        }

        // Determine if highlighted
        const bool isHighlighted = std::find(highlighted.begin(), highlighted.end(), i) != highlighted.end();

        // Choose color based on state
        juce::Colour colour;

        if (gradientMode)
        {
            // VU meter gradient: green -> yellow -> red based on level
            const float level = juce::jlimit(0.0f, 1.0f, volume);
            if (level < 0.5f)
            {
                // Green to yellow
                const float t = level * 2.0f;
                colour = juce::Colour::fromRGB(
                    static_cast<uint8_t>(t * 255),
                    255,
                    0);
            }
            else
            {
                // Yellow to red
                const float t = (level - 0.5f) * 2.0f;
                colour = juce::Colour::fromRGB(
                    255,
                    static_cast<uint8_t>((1.0f - t) * 255),
                    0);
            }
        }
        else if (isHighlighted)
        {
            colour = highlightColour;
        }
        else if (agentState == AgentState::Speaking)
        {
            colour = highlightColour;
        }
        else
        {
            colour = barColour;
        }

        g.setColour(colour);
        g.fillRoundedRectangle(x, y, barWidth, barHeight, barWidth / 2.0f);

        // Pulsing effect for thinking state
        if (agentState == AgentState::Thinking && isHighlighted)
        {
            const float pulseAlpha = 0.5f + 0.5f * std::sin(demoTime * 10.0f);
            g.setColour(colour.withAlpha(pulseAlpha * 0.5f));
            g.fillRoundedRectangle(x - 2, y - 2, barWidth + 4, barHeight + 4, (barWidth + 4) / 2.0f);
        }
    }
}

void BarVisualizer::resized()
{
    repaint();
}

void BarVisualizer::timerCallback()
{
    const int64_t currentTime = juce::Time::currentTimeMillis();

    // Update demo time
    demoTime += 1.0f / 60.0f;

    // Update animation
    const int interval = getAnimationInterval();
    if (currentTime - lastAnimTime >= interval && !animationSequence.empty())
    {
        animationStep = (animationStep + 1) % static_cast<int>(animationSequence.size());
        lastAnimTime = currentTime;
    }

    // Update audio data
    if (audioAnalyzer != nullptr && !demoMode)
    {
        audioAnalyzer->getFrequencyBands(volumeBands, barCount, kLoPass, kHiPass);
    }

    // Update fake data for demo mode
    if (demoMode && (agentState == AgentState::Speaking || agentState == AgentState::Listening))
    {
        updateFakeVolumeBands();
    }
    else if (demoMode)
    {
        // Reset to idle values
        std::fill(fakeVolumeBands.begin(), fakeVolumeBands.end(), 0.2f);
    }

    repaint();
}

int BarVisualizer::getAnimationInterval() const
{
    // From bar-visualizer.tsx getAnimationInterval
    switch (agentState)
    {
        case AgentState::Connecting:
            return 2000 / barCount;

        case AgentState::Thinking:
            return 150;

        case AgentState::Listening:
            return 500;

        default:
            return 1000;
    }
}

std::vector<int> BarVisualizer::getHighlightedIndices() const
{
    if (animationSequence.empty() || agentState == AgentState::Idle ||
        agentState == AgentState::Speaking)
    {
        // Return all indices for speaking state
        if (agentState == AgentState::Speaking)
        {
            std::vector<int> all;
            for (int i = 0; i < barCount; ++i)
                all.push_back(i);
            return all;
        }
        return {};
    }

    const int step = animationStep % static_cast<int>(animationSequence.size());
    return animationSequence[step];
}

void BarVisualizer::updateFakeVolumeBands()
{
    // From bar-visualizer.tsx demo mode
    const float time = demoTime;
    const float startTime = 0.0f;

    for (int i = 0; i < barCount; ++i)
    {
        const float waveOffset = i * 0.5f;
        const float baseVolume = std::sin((time - startTime) * 2.0f + waveOffset) * 0.3f + 0.5f;
        const float randomNoise = Interpolation::seededRandom(time * 1000.0f + i) * 0.2f;

        fakeVolumeBands[i] = juce::jlimit(0.1f, 1.0f, baseVolume + randomNoise);
    }
}

void BarVisualizer::generateConnectingSequence()
{
    // From bar-visualizer.tsx generateConnectingSequenceBar
    animationSequence.clear();

    for (int x = 0; x < barCount; ++x)
    {
        std::vector<int> frame = {x, barCount - 1 - x};
        animationSequence.push_back(frame);
    }
}

void BarVisualizer::generateListeningSequence()
{
    // From bar-visualizer.tsx generateListeningSequenceBar
    animationSequence.clear();

    const int center = barCount / 2;
    const int noIndex = -1;

    animationSequence.push_back({center});
    animationSequence.push_back({noIndex});
}

} // namespace shmui
