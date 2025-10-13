// SPDX-License-Identifier: MIT

#include "TransportControls.h"

//==============================================================================
TransportControls::TransportControls()
{
    // Create Stop All button
    m_stopAllButton = std::make_unique<juce::TextButton>("Stop All");
    m_stopAllButton->setButtonText("Stop All");
    m_stopAllButton->onClick = [this]() {
        if (onStopAll)
            onStopAll();
    };
    addAndMakeVisible(m_stopAllButton.get());

    // Create Panic button (red, emergency stop)
    m_panicButton = std::make_unique<juce::TextButton>("Panic");
    m_panicButton->setButtonText("PANIC");
    m_panicButton->onClick = [this]() {
        if (onPanic)
            onPanic();
    };
    m_panicButton->setColour(juce::TextButton::buttonColourId, juce::Colours::darkred);
    m_panicButton->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    addAndMakeVisible(m_panicButton.get());
}

//==============================================================================
void TransportControls::paint(juce::Graphics& g)
{
    // Background
    g.fillAll(juce::Colour(0xff252525));

    // Separator line at top
    g.setColour(juce::Colour(0xff404040));
    g.drawLine(0.0f, 0.0f, static_cast<float>(getWidth()), 0.0f, 2.0f);
}

void TransportControls::resized()
{
    auto bounds = getLocalBounds().reduced(10);

    // Layout buttons horizontally from right side
    int buttonWidth = 100;
    int buttonHeight = 32;
    int gap = 10;

    // Panic button (rightmost)
    auto panicBounds = bounds.removeFromRight(buttonWidth);
    panicBounds = panicBounds.withSizeKeepingCentre(buttonWidth, buttonHeight);
    m_panicButton->setBounds(panicBounds);

    bounds.removeFromRight(gap);

    // Stop All button (left of Panic)
    auto stopBounds = bounds.removeFromRight(buttonWidth);
    stopBounds = stopBounds.withSizeKeepingCentre(buttonWidth, buttonHeight);
    m_stopAllButton->setBounds(stopBounds);
}
