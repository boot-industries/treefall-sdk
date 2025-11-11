// SPDX-License-Identifier: MIT

#include "TransportControls.h"

//==============================================================================
TransportControls::TransportControls() {
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

  // Create latency label
  m_latencyLabel = std::make_unique<juce::Label>("Latency", "Latency: -- ms");
  m_latencyLabel->setFont(juce::FontOptions(12.0f, juce::Font::plain));
  m_latencyLabel->setColour(juce::Label::textColourId, juce::Colours::lightgrey);
  m_latencyLabel->setJustificationType(juce::Justification::centredLeft);
  addAndMakeVisible(m_latencyLabel.get());

  // OCC109 v0.2.2: Create CPU usage label
  m_cpuLabel = std::make_unique<juce::Label>("CPU", "CPU: --");
  m_cpuLabel->setFont(juce::FontOptions(12.0f, juce::Font::plain));
  m_cpuLabel->setColour(juce::Label::textColourId, juce::Colours::lightgrey);
  m_cpuLabel->setJustificationType(juce::Justification::centredLeft);
  addAndMakeVisible(m_cpuLabel.get());

  // OCC109 v0.2.2: Create memory usage label
  m_memoryLabel = std::make_unique<juce::Label>("Memory", "MEM: --");
  m_memoryLabel->setFont(juce::FontOptions(12.0f, juce::Font::plain));
  m_memoryLabel->setColour(juce::Label::textColourId, juce::Colours::lightgrey);
  m_memoryLabel->setJustificationType(juce::Justification::centredLeft);
  addAndMakeVisible(m_memoryLabel.get());
}

//==============================================================================
void TransportControls::paint(juce::Graphics& g) {
  // Background
  g.fillAll(juce::Colour(0xff252525));

  // Separator line at top
  g.setColour(juce::Colour(0xff404040));
  g.drawLine(0.0f, 0.0f, static_cast<float>(getWidth()), 0.0f, 2.0f);
}

void TransportControls::resized() {
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

  // OCC109 v0.2.2: Performance labels (right side, before buttons)
  bounds.removeFromRight(gap);

  // Memory label
  auto memoryBounds = bounds.removeFromRight(90);
  memoryBounds = memoryBounds.withSizeKeepingCentre(90, buttonHeight);
  m_memoryLabel->setBounds(memoryBounds);

  bounds.removeFromRight(gap);

  // CPU label
  auto cpuBounds = bounds.removeFromRight(80);
  cpuBounds = cpuBounds.withSizeKeepingCentre(80, buttonHeight);
  m_cpuLabel->setBounds(cpuBounds);

  // Latency label (left side)
  auto latencyBounds = bounds.removeFromLeft(200);
  latencyBounds = latencyBounds.withSizeKeepingCentre(200, buttonHeight);
  m_latencyLabel->setBounds(latencyBounds);
}

void TransportControls::setLatencyInfo(double latencyMs, int bufferSize, int sampleRate) {
  juce::String text =
      juce::String::formatted("Latency: %.1f ms (%d @ %dHz)", latencyMs, bufferSize, sampleRate);

  // Color-code for user feedback (green < 10ms, orange < 20ms, red >= 20ms)
  juce::Colour color;
  if (latencyMs < 10.0) {
    color = juce::Colours::lightgreen;
  } else if (latencyMs < 20.0) {
    color = juce::Colours::orange;
  } else {
    color = juce::Colours::red;
  }

  m_latencyLabel->setText(text, juce::dontSendNotification);
  m_latencyLabel->setColour(juce::Label::textColourId, color);
}

void TransportControls::setPerformanceInfo(float cpuPercent, int memoryMB) {
  // OCC109 v0.2.2: Update CPU usage display
  juce::String cpuText = juce::String::formatted("CPU: %.0f%%", cpuPercent);
  m_cpuLabel->setText(cpuText, juce::dontSendNotification);

  // Color-code CPU usage (green < 50%, orange < 80%, red >= 80%)
  juce::Colour cpuColor;
  if (cpuPercent < 50.0f) {
    cpuColor = juce::Colours::lightgreen;
  } else if (cpuPercent < 80.0f) {
    cpuColor = juce::Colours::orange;
  } else {
    cpuColor = juce::Colours::red;
  }
  m_cpuLabel->setColour(juce::Label::textColourId, cpuColor);

  // OCC109 v0.2.2: Update memory usage display
  juce::String memoryText = juce::String::formatted("MEM: %d MB", memoryMB);
  m_memoryLabel->setText(memoryText, juce::dontSendNotification);

  // Color-code memory usage (green < 200MB, orange < 500MB, red >= 500MB)
  juce::Colour memColor;
  if (memoryMB < 200) {
    memColor = juce::Colours::lightgreen;
  } else if (memoryMB < 500) {
    memColor = juce::Colours::orange;
  } else {
    memColor = juce::Colours::red;
  }
  m_memoryLabel->setColour(juce::Label::textColourId, memColor);
}
