// SPDX-License-Identifier: MIT

#include "TransportControls.h"
#include "../UI/ConsoleTheme.h"
#include "../UI/DesignTokens.h"

//==============================================================================
TransportControls::TransportControls() {
  // Create Stop All button
  m_stopAllButton = std::make_unique<juce::TextButton>("Stop All");
  m_stopAllButton->setButtonText("STOP ALL");
  m_stopAllButton->setColour(juce::TextButton::buttonColourId,
                             juce::Colour(OCC::Design::kMeterRed));
  m_stopAllButton->setColour(juce::TextButton::textColourOffId,
                             juce::Colour(OCC::Design::kTextPrimary));
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
  m_panicButton->setColour(juce::TextButton::buttonColourId, juce::Colour(OCC::Design::kAmber));
  m_panicButton->setColour(juce::TextButton::textColourOffId, juce::Colours::black);
  addAndMakeVisible(m_panicButton.get());

  // Cue button (ghost variant — matte grey, far right).
  m_cueButton = std::make_unique<juce::TextButton>("Cue");
  m_cueButton->setButtonText("CUE");
  m_cueButton->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff383d40));
  m_cueButton->setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff45494c));
  m_cueButton->setColour(juce::TextButton::textColourOffId,
                         juce::Colour(OCC::Design::kTextSecondary));
  m_cueButton->onClick = [this]() {
    if (onCue)
      onCue();
  };
  addAndMakeVisible(m_cueButton.get());

  // Create latency label
  m_latencyLabel = std::make_unique<juce::Label>("Latency", "Latency: -- ms");
  m_latencyLabel->setFont(juce::FontOptions(12.0f, juce::Font::plain));
  m_latencyLabel->setColour(juce::Label::textColourId, juce::Colour(OCC::Design::kTextSecondary));
  m_latencyLabel->setJustificationType(juce::Justification::centredLeft);
  addAndMakeVisible(m_latencyLabel.get());

  // OCC109 v0.2.2: Create CPU usage label
  m_cpuLabel = std::make_unique<juce::Label>("CPU", "CPU: --");
  m_cpuLabel->setFont(juce::FontOptions(12.0f, juce::Font::plain));
  m_cpuLabel->setColour(juce::Label::textColourId, juce::Colour(OCC::Design::kTextSecondary));
  m_cpuLabel->setJustificationType(juce::Justification::centredLeft);
  addAndMakeVisible(m_cpuLabel.get());

  // OCC109 v0.2.2: Create memory usage label
  m_memoryLabel = std::make_unique<juce::Label>("Memory", "MEM: --");
  m_memoryLabel->setFont(juce::FontOptions(12.0f, juce::Font::plain));
  m_memoryLabel->setColour(juce::Label::textColourId, juce::Colour(OCC::Design::kTextSecondary));
  m_memoryLabel->setJustificationType(juce::Justification::centredLeft);
  addAndMakeVisible(m_memoryLabel.get());
}

//==============================================================================
void TransportControls::paint(juce::Graphics& g) {
  auto bounds = getLocalBounds().toFloat();
  OCC::Console::fillVerticalGradient(g, bounds, juce::Colour(OCC::Design::kBgSecondary),
                                     juce::Colour(OCC::Design::kBgPrimary));

  g.setColour(juce::Colour(OCC::Design::kBorderDefault));
  g.drawLine(0.0f, 0.0f, static_cast<float>(getWidth()), 0.0f, 1.0f);

  auto status = getLocalBounds().reduced(10, 0);
  status.removeFromLeft(398);
  status.removeFromRight(350);
  g.setColour(juce::Colour(OCC::Design::kBorderDefault));
  g.drawLine(status.getX() - 10.0f, 12.0f, status.getX() - 10.0f, getHeight() - 12.0f, 1.0f);

  int playing = 0;
  juce::StringArray names;
  for (const auto& clip : m_snapshot.session.clips) {
    if (clip.hasClip && clip.playbackState != orpheus::PlaybackState::Stopped) {
      ++playing;
      if (names.size() < 2 && clip.displayName.isNotEmpty())
        names.add(clip.displayName);
    }
  }

  g.setFont(OCC::Console::monoFont(10.5f));
  g.setColour(playing > 0 ? juce::Colour(0xff5af070) : juce::Colour(OCC::Design::kTextMuted));
  g.fillEllipse(status.getX(), getHeight() / 2 - 4, 8, 8);
  g.setColour(juce::Colour(OCC::Design::kTextSecondary));
  g.drawText("PLAYING  .  " + juce::String(playing), status.withTrimmedLeft(18),
             juce::Justification::centredLeft, false);
  if (!names.isEmpty()) {
    g.setFont(OCC::Console::consoleFont(14.0f, juce::Font::bold));
    g.setColour(juce::Colour(OCC::Design::kTextPrimary));
    g.drawText(names.joinIntoString("   |   "), status.withTrimmedLeft(128),
               juce::Justification::centredLeft, true);
  }

  // Master cluster: leave room on the far right for the CUE button (laid out in resized()).
  auto meterArea = getLocalBounds().reduced(10, 0).removeFromRight(380);
  meterArea.removeFromRight(72); // CUE button slot.

  g.setFont(OCC::Console::monoFont(11.0f, juce::Font::plain));
  g.setColour(juce::Colour(OCC::Design::kTextSecondary));
  g.drawText("MASTER", meterArea.removeFromLeft(64), juce::Justification::centredRight, false);
  meterArea.removeFromLeft(10);

  // Meter bar.
  auto meter = meterArea.removeFromLeft(160).withSizeKeepingCentre(160, 12).toFloat();
  g.setColour(juce::Colour(OCC::Design::kBgInset));
  g.fillRoundedRectangle(meter, 2.0f);
  g.setColour(juce::Colour(OCC::Design::kBorderDefault));
  g.drawRoundedRectangle(meter, 2.0f, 1.0f);

  const float level = juce::jlimit(0.0f, 1.0f, m_snapshot.audio.masterRmsLevel);
  if (level > 0.0f) {
    auto fill = meter.reduced(1.0f);
    fill = fill.withWidth(fill.getWidth() * level);
    juce::ColourGradient grad(juce::Colour(OCC::Design::kMeterGreen), fill.getX(), fill.getY(),
                              juce::Colour(OCC::Design::kMeterRed), meter.getRight(), fill.getY(),
                              false);
    grad.addColour(0.60, juce::Colour(OCC::Design::kMeterGreen));
    grad.addColour(0.78, juce::Colour(OCC::Design::kMeterYellow));
    grad.addColour(0.92, juce::Colour(OCC::Design::kMeterOrange));
    g.setGradientFill(grad);
    g.fillRoundedRectangle(fill, 2.0f);
  }

  // dB readout to the right of the meter (mono 11 pt, amber if signal present).
  meterArea.removeFromLeft(10);
  juce::String dbText;
  if (level > 0.0001f) {
    const float db = juce::jlimit(-60.0f, 6.0f, 20.0f * std::log10(level));
    dbText = juce::String::formatted("%+.1f dB", static_cast<double>(db));
  } else {
    dbText = "-inf";
  }
  g.setFont(OCC::Console::monoFont(11.0f, juce::Font::plain));
  g.setColour(level > 0.0001f ? juce::Colour(OCC::Design::kAmber)
                              : juce::Colour(OCC::Design::kTextMuted));
  g.drawText(dbText, meterArea.removeFromLeft(70), juce::Justification::centredLeft, false);
}

void TransportControls::resized() {
  auto bounds = getLocalBounds().reduced(10);

  constexpr int stopWidth = 136;
  constexpr int panicWidth = 116;
  constexpr int buttonHeight = 36;
  constexpr int gap = 10;
  constexpr int cueWidth = 60;

  auto stopBounds = bounds.removeFromLeft(stopWidth);
  stopBounds = stopBounds.withSizeKeepingCentre(stopWidth, buttonHeight);
  m_stopAllButton->setBounds(stopBounds);

  bounds.removeFromLeft(gap);

  auto panicBounds = bounds.removeFromLeft(panicWidth);
  panicBounds = panicBounds.withSizeKeepingCentre(panicWidth, buttonHeight);
  m_panicButton->setBounds(panicBounds);

  bounds.removeFromLeft(gap + 8);

  // CUE button on the far right.
  auto cueBounds = bounds.removeFromRight(cueWidth);
  cueBounds = cueBounds.withSizeKeepingCentre(cueWidth, 28);
  if (m_cueButton)
    m_cueButton->setBounds(cueBounds);

  // The remaining 320 px on the right is reserved for the master meter (drawn in paint()).
  bounds.removeFromRight(320);

  auto latencyBounds = bounds.removeFromLeft(160);
  latencyBounds = latencyBounds.withSizeKeepingCentre(160, buttonHeight);
  m_latencyLabel->setBounds(latencyBounds);

  auto cpuBounds = bounds.removeFromLeft(90);
  cpuBounds = cpuBounds.withSizeKeepingCentre(90, buttonHeight);
  m_cpuLabel->setBounds(cpuBounds);

  auto memoryBounds = bounds.removeFromLeft(100);
  memoryBounds = memoryBounds.withSizeKeepingCentre(100, buttonHeight);
  m_memoryLabel->setBounds(memoryBounds);
}

void TransportControls::setLatencyInfo(double latencyMs, int bufferSize, int sampleRate) {
  juce::String text =
      juce::String::formatted("Latency: %.1f ms (%d @ %dHz)", latencyMs, bufferSize, sampleRate);

  // Color-code for user feedback (green < 10ms, orange < 20ms, red >= 20ms)
  juce::Colour color;
  if (latencyMs < 10.0) {
    color = juce::Colour(OCC::Design::kMeterGreen);
  } else if (latencyMs < 20.0) {
    color = juce::Colour(OCC::Design::kMeterYellow);
  } else {
    color = juce::Colour(OCC::Design::kMeterRed);
  }

  m_latencyLabel->setText(text, juce::dontSendNotification);
  m_latencyLabel->setColour(juce::Label::textColourId, color);
}

void TransportControls::setTransportSnapshot(const occ::ui::ClipComposerUiSnapshot& snapshot) {
  m_snapshot = snapshot;
  repaint();
}

void TransportControls::setPerformanceInfo(float cpuPercent, int memoryMB) {
  // OCC109 v0.2.2: Update CPU usage display
  juce::String cpuText = juce::String::formatted("CPU: %.0f%%", cpuPercent);
  m_cpuLabel->setText(cpuText, juce::dontSendNotification);

  // Color-code CPU usage (green < 50%, orange < 80%, red >= 80%)
  juce::Colour cpuColor;
  if (cpuPercent < 50.0f) {
    cpuColor = juce::Colour(OCC::Design::kMeterGreen);
  } else if (cpuPercent < 80.0f) {
    cpuColor = juce::Colour(OCC::Design::kMeterYellow);
  } else {
    cpuColor = juce::Colour(OCC::Design::kMeterRed);
  }
  m_cpuLabel->setColour(juce::Label::textColourId, cpuColor);

  // OCC109 v0.2.2: Update memory usage display
  juce::String memoryText = juce::String::formatted("MEM: %d MB", memoryMB);
  m_memoryLabel->setText(memoryText, juce::dontSendNotification);

  // Color-code memory usage (green < 200MB, orange < 500MB, red >= 500MB)
  juce::Colour memColor;
  if (memoryMB < 200) {
    memColor = juce::Colour(OCC::Design::kTextSecondary);
  } else if (memoryMB < 500) {
    memColor = juce::Colour(OCC::Design::kMeterYellow);
  } else {
    memColor = juce::Colour(OCC::Design::kMeterRed);
  }
  m_memoryLabel->setColour(juce::Label::textColourId, memColor);
}
