/*
  ==============================================================================

    TransportBar.cpp
    Created: shmui Component Library

    Transport bar implementation.

  ==============================================================================
*/

#include "TransportBar.h"

namespace shmui {

//==============================================================================
TransportBar::TransportBar() {
  setupButtons();
}

//==============================================================================
void TransportBar::setPlaying(bool playing) {
  if (m_isPlaying != playing) {
    m_isPlaying = playing;
    m_playPauseButton->setPlaying(playing);
    repaint();
  }
}

void TransportBar::setRecording(bool recording) {
  if (m_isRecording != recording) {
    m_isRecording = recording;
    if (m_recordButton)
      m_recordButton->setRecording(recording);
    repaint();
  }
}

void TransportBar::setLooping(bool looping) {
  if (m_isLooping != looping) {
    m_isLooping = looping;
    if (m_loopButton)
      m_loopButton->setToggled(looping);
    repaint();
  }
}

//==============================================================================
void TransportBar::setPositionSeconds(double seconds) {
  m_positionSeconds = seconds;
  m_positionSamples = static_cast<int64_t>(seconds * m_sampleRate);

  if (m_positionLabel)
    m_positionLabel->setText(formatTime(seconds), juce::dontSendNotification);
}

void TransportBar::setPositionSamples(int64_t samples, int sampleRate) {
  m_positionSamples = samples;
  m_sampleRate = sampleRate;
  m_positionSeconds = static_cast<double>(samples) / sampleRate;

  if (m_positionLabel)
    m_positionLabel->setText(formatTime(m_positionSeconds), juce::dontSendNotification);
}

void TransportBar::setDurationSeconds(double seconds) {
  m_durationSeconds = seconds;
  m_durationSamples = static_cast<int64_t>(seconds * m_sampleRate);

  if (m_durationLabel)
    m_durationLabel->setText(formatTime(seconds), juce::dontSendNotification);
}

void TransportBar::setDurationSamples(int64_t samples, int sampleRate) {
  m_durationSamples = samples;
  m_sampleRate = sampleRate;
  m_durationSeconds = static_cast<double>(samples) / sampleRate;

  if (m_durationLabel)
    m_durationLabel->setText(formatTime(m_durationSeconds), juce::dontSendNotification);
}

void TransportBar::setTimeFormat(TimeDisplayFormat format) {
  m_timeFormat = format;
  setPositionSeconds(m_positionSeconds); // Refresh display
  setDurationSeconds(m_durationSeconds);
}

//==============================================================================
void TransportBar::setTempo(double bpm) {
  m_tempoBPM = bpm;
  if (m_tempoLabel)
    m_tempoLabel->setText(juce::String(bpm, 1) + " BPM", juce::dontSendNotification);
}

void TransportBar::setTimeSignature(int numerator, int denominator) {
  m_timeSignatureNum = numerator;
  m_timeSignatureDenom = denominator;
}

void TransportBar::setStyle(const TransportBarStyle& style) {
  m_style = style;

  // Update visibility of optional components
  if (m_recordButton)
    m_recordButton->setVisible(style.showRecord);
  if (m_loopButton)
    m_loopButton->setVisible(style.showLoop);
  if (m_tempoLabel)
    m_tempoLabel->setVisible(style.showTempo);
  if (m_panicButton)
    m_panicButton->setVisible(style.showPanic);

  resized();
  repaint();
}

//==============================================================================
void TransportBar::paint(juce::Graphics& g) {
  auto bounds = getLocalBounds().toFloat();

  // Background
  g.fillAll(m_style.backgroundColor);

  // Top border line
  g.setColour(m_style.separatorColor);
  g.drawHorizontalLine(0, bounds.getX(), bounds.getRight());
}

void TransportBar::resized() {
  auto bounds = getLocalBounds();
  const int buttonSize = static_cast<int>(m_style.buttonSize);
  const int spacing = static_cast<int>(m_style.buttonSpacing);
  const int sectionSpacing = static_cast<int>(m_style.sectionSpacing);

  // Center buttons vertically
  int buttonY = (bounds.getHeight() - buttonSize) / 2;

  int x = sectionSpacing;

  // Play/Pause button
  m_playPauseButton->setBounds(x, buttonY, buttonSize, buttonSize);
  x += buttonSize + spacing;

  // Stop button
  m_stopButton->setBounds(x, buttonY, buttonSize, buttonSize);
  x += buttonSize + spacing;

  // Record button (if visible)
  if (m_style.showRecord && m_recordButton) {
    m_recordButton->setBounds(x, buttonY, buttonSize, buttonSize);
    x += buttonSize + spacing;
  }

  x += sectionSpacing;

  // Loop button (if visible)
  if (m_style.showLoop && m_loopButton) {
    m_loopButton->setBounds(x, buttonY, buttonSize, buttonSize);
    x += buttonSize + sectionSpacing;
  }

  // Time display
  const int timeLabelWidth = 90;
  const int timeLabelHeight = 20;
  int timeY = (bounds.getHeight() - timeLabelHeight) / 2;

  m_positionLabel->setBounds(x, timeY, timeLabelWidth, timeLabelHeight);
  x += timeLabelWidth + 8;

  // Separator "/"
  // (drawn in paint if needed)

  m_durationLabel->setBounds(x, timeY, timeLabelWidth, timeLabelHeight);
  x += timeLabelWidth + sectionSpacing;

  // Tempo display (if visible)
  if (m_style.showTempo && m_tempoLabel) {
    const int tempoWidth = 80;
    m_tempoLabel->setBounds(x, timeY, tempoWidth, timeLabelHeight);
    x += tempoWidth + sectionSpacing;
  }

  // Panic button on the right
  if (m_style.showPanic && m_panicButton) {
    m_panicButton->setBounds(bounds.getRight() - sectionSpacing - buttonSize, buttonY, buttonSize,
                             buttonSize);
  }
}

//==============================================================================
void TransportBar::setupButtons() {
  // Play/Pause button
  m_playPauseButton = std::make_unique<TransportButton>(TransportButton::Type::PlayPause);
  m_playPauseButton->setSize(ButtonSize::Large);
  m_playPauseButton->setStyle(ButtonStyle::Primary);
  m_playPauseButton->onClick = [this] {
    if (onPlayPause)
      onPlayPause();
  };
  addAndMakeVisible(*m_playPauseButton);

  // Stop button
  m_stopButton = std::make_unique<TransportButton>(TransportButton::Type::Stop);
  m_stopButton->setSize(ButtonSize::Large);
  m_stopButton->setStyle(ButtonStyle::Ghost);
  m_stopButton->onClick = [this] {
    if (onStop)
      onStop();
  };
  addAndMakeVisible(*m_stopButton);

  // Record button
  m_recordButton = std::make_unique<TransportButton>(TransportButton::Type::Record);
  m_recordButton->setSize(ButtonSize::Large);
  m_recordButton->onClick = [this] {
    if (onRecord)
      onRecord();
  };
  addAndMakeVisible(*m_recordButton);
  m_recordButton->setVisible(m_style.showRecord);

  // Loop button
  m_loopButton = std::make_unique<ToggleButton>(IconType::Loop);
  m_loopButton->setSize(ButtonSize::Large);
  m_loopButton->setStyle(ButtonStyle::Ghost);
  m_loopButton->setOnColor(juce::Colour(0xFF3B82F6)); // Blue when active
  m_loopButton->onToggle = [this](bool enabled) {
    m_isLooping = enabled;
    if (onLoopToggle)
      onLoopToggle(enabled);
  };
  addAndMakeVisible(*m_loopButton);
  m_loopButton->setVisible(m_style.showLoop);

  // Panic button
  m_panicButton = std::make_unique<TransportButton>(TransportButton::Type::Stop);
  m_panicButton->setSize(ButtonSize::Large);
  m_panicButton->setStyle(ButtonStyle::Destructive);
  m_panicButton->setTooltipText("Panic - Stop All");
  m_panicButton->onClick = [this] {
    if (onPanic)
      onPanic();
  };
  addAndMakeVisible(*m_panicButton);
  m_panicButton->setVisible(m_style.showPanic);

  // Position label
  m_positionLabel = std::make_unique<juce::Label>();
  m_positionLabel->setText("0:00.000", juce::dontSendNotification);
  m_positionLabel->setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
  m_positionLabel->setColour(juce::Label::textColourId, m_style.textColor);
  m_positionLabel->setJustificationType(juce::Justification::centredRight);
  addAndMakeVisible(*m_positionLabel);

  // Duration label
  m_durationLabel = std::make_unique<juce::Label>();
  m_durationLabel->setText("0:00.000", juce::dontSendNotification);
  m_durationLabel->setFont(juce::Font(juce::FontOptions(14.0f)));
  m_durationLabel->setColour(juce::Label::textColourId, m_style.dimTextColor);
  m_durationLabel->setJustificationType(juce::Justification::centredLeft);
  addAndMakeVisible(*m_durationLabel);

  // Tempo label
  m_tempoLabel = std::make_unique<juce::Label>();
  m_tempoLabel->setText("120.0 BPM", juce::dontSendNotification);
  m_tempoLabel->setFont(juce::Font(juce::FontOptions(12.0f)));
  m_tempoLabel->setColour(juce::Label::textColourId, m_style.dimTextColor);
  m_tempoLabel->setJustificationType(juce::Justification::centred);
  addAndMakeVisible(*m_tempoLabel);
  m_tempoLabel->setVisible(m_style.showTempo);
}

juce::String TransportBar::formatTime(double seconds) const {
  switch (m_timeFormat) {
  case TimeDisplayFormat::MinutesSeconds: {
    int mins = static_cast<int>(seconds / 60.0);
    double secs = std::fmod(seconds, 60.0);
    return juce::String::formatted("%d:%06.3f", mins, secs);
  }

  case TimeDisplayFormat::Timecode:
    return formatTimecode(seconds);

  case TimeDisplayFormat::Bars:
    return formatBars(seconds);

  case TimeDisplayFormat::Samples:
    return formatSamples(static_cast<int64_t>(seconds * m_sampleRate));

  default:
    return juce::String(seconds, 3);
  }
}

juce::String TransportBar::formatTimecode(double seconds) const {
  int hours = static_cast<int>(seconds / 3600.0);
  int mins = static_cast<int>(std::fmod(seconds, 3600.0) / 60.0);
  int secs = static_cast<int>(std::fmod(seconds, 60.0));
  int frames = static_cast<int>(std::fmod(seconds * 30.0, 30.0)); // 30fps

  return juce::String::formatted("%02d:%02d:%02d:%02d", hours, mins, secs, frames);
}

juce::String TransportBar::formatBars(double seconds) const {
  // Calculate beats
  double beatsPerSecond = m_tempoBPM / 60.0;
  double totalBeats = seconds * beatsPerSecond;

  int bars = static_cast<int>(totalBeats / m_timeSignatureNum) + 1;
  int beats = static_cast<int>(std::fmod(totalBeats, m_timeSignatureNum)) + 1;
  int ticks = static_cast<int>(std::fmod(totalBeats, 1.0) * 480); // 480 ticks per beat

  return juce::String::formatted("%d.%d.%03d", bars, beats, ticks);
}

juce::String TransportBar::formatSamples(int64_t samples) const {
  return juce::String(samples);
}

} // namespace shmui
