/*
  ==============================================================================

    LevelMetersWindow.cpp
    Created: 12 Jan 2026
    Author:  Orpheus Clip Composer

    Sprint 19: Level Meters Window with Play History (OCC117)

  ==============================================================================
*/

#include "LevelMetersWindow.h"

//==============================================================================
// LevelMetersWindow

LevelMetersWindow::LevelMetersWindow(AudioEngine* audioEngine)
    : DocumentWindow("Level Meters", juce::Colour(0xff2d2d2d),
                     DocumentWindow::closeButton | DocumentWindow::minimiseButton) {

  m_content = std::make_unique<Content>(audioEngine);
  setContentOwned(m_content.release(), true);

  setUsingNativeTitleBar(true);
  setResizable(true, false);
  centreWithSize(300, 500);
  setVisible(true);
}

LevelMetersWindow::~LevelMetersWindow() = default;

void LevelMetersWindow::closeButtonPressed() {
  setVisible(false);
}

void LevelMetersWindow::updateLevels(const std::array<float, 4>& groupLevels, float masterLevel) {
  if (auto* content = dynamic_cast<Content*>(getContentComponent())) {
    content->updateLevels(groupLevels, masterLevel);
  }
}

void LevelMetersWindow::addPlayHistoryEntry(int clipIndex, const juce::String& clipName,
                                            int groupIndex) {
  if (auto* content = dynamic_cast<Content*>(getContentComponent())) {
    content->addPlayHistoryEntry(clipIndex, clipName, groupIndex);
  }
}

//==============================================================================
// Content

LevelMetersWindow::Content::Content(AudioEngine* audioEngine) : m_audioEngine(audioEngine) {

  // Title label
  addAndMakeVisible(m_titleLabel);
  m_titleLabel.setText("Level Meters", juce::dontSendNotification);
  m_titleLabel.setFont(juce::Font(16.0f, juce::Font::bold));
  m_titleLabel.setJustificationType(juce::Justification::centred);

  // History text area
  addAndMakeVisible(m_historyText);
  m_historyText.setMultiLine(true);
  m_historyText.setReadOnly(true);
  m_historyText.setScrollbarsShown(true);
  m_historyText.setCaretVisible(false);
  m_historyText.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff1a1a1a));
  m_historyText.setColour(juce::TextEditor::textColourId, juce::Colours::lightgrey);
  m_historyText.setFont(
      juce::Font(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain));

  // Initialize peak hold times
  auto now = juce::Time::getCurrentTime();
  for (auto& t : m_peakHoldTimes) {
    t = now;
  }

  // Start update timer
  startTimer(33); // ~30 FPS
}

void LevelMetersWindow::Content::paint(juce::Graphics& g) {
  g.fillAll(juce::Colour(0xff2d2d2d));

  // Draw meters
  auto area = getLocalBounds().reduced(10);
  area.removeFromTop(30); // Title

  // Meter section (top 200px)
  auto meterArea = area.removeFromTop(200);

  // Draw 4 group meters + master
  int meterWidth = (meterArea.getWidth() - 40) / 5; // 5 meters with gaps
  int meterHeight = meterArea.getHeight() - 20;

  const juce::String labels[] = {"Grp 1", "Grp 2", "Grp 3", "Grp 4", "Master"};

  for (int i = 0; i < 5; ++i) {
    auto bounds = juce::Rectangle<int>(meterArea.getX() + i * (meterWidth + 10), meterArea.getY(),
                                       meterWidth, meterHeight);

    float level, peak;
    if (i < 4) {
      level = m_groupLevels[i].load();
      peak = m_groupPeaks[i];
    } else {
      level = m_masterLevel.load();
      peak = m_masterPeak;
    }

    paintMeter(g, bounds, level, peak, labels[i]);
  }
}

void LevelMetersWindow::Content::paintMeter(juce::Graphics& g, juce::Rectangle<int> bounds,
                                            float level, float peakLevel,
                                            const juce::String& label) {
  // Background
  g.setColour(juce::Colour(0xff1a1a1a));
  g.fillRect(bounds);

  // Border
  g.setColour(juce::Colour(0xff4a4a4a));
  g.drawRect(bounds, 1);

  // Meter fill
  auto meterBounds = bounds.reduced(2);
  auto labelArea = meterBounds.removeFromBottom(20);
  meterBounds.removeFromBottom(2);

  // Calculate fill height
  float fillRatio = juce::jlimit(0.0f, 1.0f, level);
  int fillHeight = static_cast<int>(meterBounds.getHeight() * fillRatio);

  // Gradient fill (green -> yellow -> red)
  auto fillBounds = meterBounds.removeFromBottom(fillHeight);

  if (fillHeight > 0) {
    // Simple color based on level
    juce::Colour meterColor;
    if (fillRatio > 0.9f) {
      meterColor = juce::Colours::red;
    } else if (fillRatio > 0.7f) {
      meterColor = juce::Colours::yellow;
    } else {
      meterColor = juce::Colours::green;
    }
    g.setColour(meterColor);
    g.fillRect(fillBounds);
  }

  // Peak indicator
  if (peakLevel > 0.0f) {
    float peakRatio = juce::jlimit(0.0f, 1.0f, peakLevel);
    int peakY = bounds.getBottom() - 22 - static_cast<int>((bounds.getHeight() - 24) * peakRatio);

    g.setColour(juce::Colours::white);
    g.drawHorizontalLine(peakY, bounds.getX() + 2.0f, bounds.getRight() - 2.0f);
  }

  // Label
  g.setColour(juce::Colours::white);
  g.setFont(juce::Font(11.0f));
  g.drawText(label, labelArea, juce::Justification::centred);
}

void LevelMetersWindow::Content::resized() {
  auto area = getLocalBounds().reduced(10);

  // Title
  m_titleLabel.setBounds(area.removeFromTop(25));
  area.removeFromTop(5);

  // Meters take top 200px (painted in paint())
  area.removeFromTop(200);
  area.removeFromTop(10);

  // History label
  auto historyLabelArea = area.removeFromTop(20);
  juce::Graphics g(juce::Image(juce::Image::RGB, 1, 1, false));
  g.setFont(juce::Font(12.0f, juce::Font::bold));

  // History text takes remaining space
  m_historyText.setBounds(area);
}

void LevelMetersWindow::Content::updateLevels(const std::array<float, 4>& groupLevels,
                                              float masterLevel) {
  for (int i = 0; i < 4; ++i) {
    m_groupLevels[i].store(groupLevels[i]);
  }
  m_masterLevel.store(masterLevel);
}

void LevelMetersWindow::Content::addPlayHistoryEntry(int clipIndex, const juce::String& clipName,
                                                     int groupIndex) {
  juce::ScopedLock lock(m_historyLock);

  PlayHistoryEntry entry;
  entry.timestamp = juce::Time::getCurrentTime();
  entry.clipIndex = clipIndex;
  entry.clipName = clipName;
  entry.groupIndex = groupIndex;

  m_playHistory.push_front(entry);

  // Limit size
  while (m_playHistory.size() > MAX_HISTORY_ENTRIES) {
    m_playHistory.pop_back();
  }
}

void LevelMetersWindow::Content::timerCallback() {
  auto now = juce::Time::getCurrentTime();

  // Update peaks
  for (int i = 0; i < 4; ++i) {
    float currentLevel = m_groupLevels[i].load();
    if (currentLevel > m_groupPeaks[i]) {
      m_groupPeaks[i] = currentLevel;
      m_peakHoldTimes[i] = now;
    } else if ((now - m_peakHoldTimes[i]).inMilliseconds() > PEAK_HOLD_MS) {
      // Decay peak
      m_groupPeaks[i] *= 0.9f;
      if (m_groupPeaks[i] < 0.01f) {
        m_groupPeaks[i] = 0.0f;
      }
    }
  }

  // Master peak
  float currentMaster = m_masterLevel.load();
  if (currentMaster > m_masterPeak) {
    m_masterPeak = currentMaster;
    m_peakHoldTimes[4] = now;
  } else if ((now - m_peakHoldTimes[4]).inMilliseconds() > PEAK_HOLD_MS) {
    m_masterPeak *= 0.9f;
    if (m_masterPeak < 0.01f) {
      m_masterPeak = 0.0f;
    }
  }

  // Update history display
  {
    juce::ScopedLock lock(m_historyLock);
    juce::String historyText;
    historyText << "Play History:\n";
    historyText << "─────────────────────────\n";

    for (const auto& entry : m_playHistory) {
      historyText << entry.timestamp.formatted("%H:%M:%S") << " | ";
      historyText << "G" << (entry.groupIndex + 1) << " | ";
      historyText << entry.clipName.substring(0, 20);
      if (entry.clipName.length() > 20) {
        historyText << "...";
      }
      historyText << "\n";
    }

    if (m_historyText.getText() != historyText) {
      m_historyText.setText(historyText, false);
    }
  }

  // Trigger repaint
  repaint();
}
