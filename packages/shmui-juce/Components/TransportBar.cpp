/*
  ==============================================================================

    TransportBar.cpp
    Created: shmui Component Library

    Transport bar implementation.

  ==============================================================================
*/

#include "TransportBar.h"
#include <algorithm>
#include <cmath>

namespace shmui {
namespace {
int64_t saturatingSamples(double seconds, int sampleRate) {
  if (!std::isfinite(seconds) || seconds <= 0.0)
    return 0;

  const long double samples =
      static_cast<long double>(seconds) * static_cast<long double>(juce::jmax(1, sampleRate));
  const long double maximum = static_cast<long double>(std::numeric_limits<int64_t>::max());
  return samples >= maximum ? std::numeric_limits<int64_t>::max() : static_cast<int64_t>(samples);
}
} // namespace

//==============================================================================
TransportBar::TransportBar() {
  const auto& theme = defaultTheme();
  m_style.backgroundColor = theme.bgPanel;
  m_style.textColor = theme.fg;
  m_style.dimTextColor = theme.fgMuted;
  m_style.separatorColor = theme.stroke;
  setupButtons();
  addDefaultThemeListener(this);
}

TransportBar::~TransportBar() {
  removeDefaultThemeListener(this);
}

//==============================================================================
void TransportBar::setPlaying(bool playing) {
  if (!requireMessageThread())
    return;
  if (m_isPlaying != playing) {
    m_isPlaying = playing;
    if (m_playPauseButton)
      m_playPauseButton->setPlaying(playing);
    repaint();
  }
}

void TransportBar::setRecording(bool recording) {
  if (!requireMessageThread())
    return;
  if (m_isRecording != recording) {
    m_isRecording = recording;
    if (m_recordButton)
      m_recordButton->setRecording(recording);
    repaint();
  }
}

void TransportBar::setLooping(bool looping) {
  if (!requireMessageThread())
    return;
  if (m_isLooping != looping) {
    m_isLooping = looping;
    if (m_loopButton)
      m_loopButton->setToggled(looping);
    repaint();
  }
}

//==============================================================================
void TransportBar::setPositionSeconds(double seconds) {
  if (!requireMessageThread())
    return;
  const double maxSeconds = static_cast<double>(std::numeric_limits<int64_t>::max()) /
                            static_cast<double>(juce::jmax(1, m_sampleRate));
  m_positionSeconds = std::isfinite(seconds) ? juce::jlimit(0.0, maxSeconds, seconds) : 0.0;
  m_positionSamples = saturatingSamples(m_positionSeconds, m_sampleRate);

  if (m_positionLabel)
    m_positionLabel->setText(formatTime(m_positionSeconds), juce::dontSendNotification);
}

void TransportBar::setPositionSamples(int64_t samples, int sampleRate) {
  if (!requireMessageThread())
    return;
  m_sampleRate = juce::jmax(1, sampleRate);
  m_positionSamples = juce::jmax<int64_t>(0, samples);
  m_positionSeconds = static_cast<double>(m_positionSamples) / m_sampleRate;

  if (m_positionLabel)
    m_positionLabel->setText(formatTime(m_positionSeconds), juce::dontSendNotification);
}

void TransportBar::setDurationSeconds(double seconds) {
  if (!requireMessageThread())
    return;
  const double maxSeconds = static_cast<double>(std::numeric_limits<int64_t>::max()) /
                            static_cast<double>(juce::jmax(1, m_sampleRate));
  m_durationSeconds = std::isfinite(seconds) ? juce::jlimit(0.0, maxSeconds, seconds) : 0.0;
  m_durationSamples = saturatingSamples(m_durationSeconds, m_sampleRate);

  if (m_durationLabel)
    m_durationLabel->setText(formatTime(m_durationSeconds), juce::dontSendNotification);
}

void TransportBar::setDurationSamples(int64_t samples, int sampleRate) {
  if (!requireMessageThread())
    return;
  m_sampleRate = juce::jmax(1, sampleRate);
  m_durationSamples = juce::jmax<int64_t>(0, samples);
  m_durationSeconds = static_cast<double>(m_durationSamples) / m_sampleRate;

  if (m_durationLabel)
    m_durationLabel->setText(formatTime(m_durationSeconds), juce::dontSendNotification);
}

void TransportBar::setTimeFormat(TimeDisplayFormat format) {
  if (!requireMessageThread())
    return;
  m_timeFormat = format;
  if (m_positionLabel)
    m_positionLabel->setText(formatTime(m_positionSeconds), juce::dontSendNotification);
  if (m_durationLabel)
    m_durationLabel->setText(formatTime(m_durationSeconds), juce::dontSendNotification);
}

//==============================================================================
void TransportBar::setTempo(double bpm) {
  if (!requireMessageThread())
    return;
  m_tempoBPM = std::isfinite(bpm) ? juce::jlimit(0.0, 1000000.0, bpm) : 0.0;
  if (m_tempoLabel)
    m_tempoLabel->setText(juce::String(m_tempoBPM, 1) + " BPM", juce::dontSendNotification);
}

void TransportBar::setTimeSignature(int numerator, int denominator) {
  if (!requireMessageThread())
    return;
  m_timeSignatureNum = juce::jmax(1, numerator);
  m_timeSignatureDenom = juce::jmax(1, denominator);
}

void TransportBar::setStyle(const TransportBarStyle& style) {
  if (!requireMessageThread())
    return;
  m_style = style;
  m_style.height =
      std::isfinite(m_style.height) ? juce::jlimit(1.0f, 4096.0f, m_style.height) : 48.0f;
  m_style.buttonSize =
      std::isfinite(m_style.buttonSize) ? juce::jlimit(1.0f, 4096.0f, m_style.buttonSize) : 36.0f;
  m_style.buttonSpacing =
      std::isfinite(m_style.buttonSpacing) ? juce::jmax(0.0f, m_style.buttonSpacing) : 4.0f;
  m_style.sectionSpacing =
      std::isfinite(m_style.sectionSpacing) ? juce::jmax(0.0f, m_style.sectionSpacing) : 16.0f;
  m_usesDefaultThemeStyle = false;

  if (m_recordButton)
    m_recordButton->setVisible(m_style.showRecord);
  if (m_loopButton) {
    m_loopButton->setVisible(m_style.showLoop);
    m_loopButton->setOnColor(m_style.textColor);
  }
  if (m_tempoLabel)
    m_tempoLabel->setVisible(m_style.showTempo);
  if (m_panicButton)
    m_panicButton->setVisible(m_style.showPanic);

  resized();
  repaint();
}

void TransportBar::defaultThemeChanged(const ShmuiTheme& theme) {
  if (!requireMessageThread() || !m_usesDefaultThemeStyle)
    return;

  m_style.backgroundColor = theme.bgPanel;
  m_style.textColor = theme.fg;
  m_style.dimTextColor = theme.fgMuted;
  m_style.separatorColor = theme.stroke;
  if (m_loopButton)
    m_loopButton->setOnColor(theme.accent);
  if (m_positionLabel)
    m_positionLabel->setColour(juce::Label::textColourId, m_style.textColor);
  if (m_durationLabel)
    m_durationLabel->setColour(juce::Label::textColourId, m_style.dimTextColor);
  if (m_tempoLabel)
    m_tempoLabel->setColour(juce::Label::textColourId, m_style.dimTextColor);
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
  if (!requireMessageThread())
    return;
  auto bounds = getLocalBounds();
  const int buttonSize = juce::jmax(1, static_cast<int>(m_style.buttonSize));
  const int spacing = juce::jmax(0, static_cast<int>(m_style.buttonSpacing));
  const int sectionSpacing = juce::jmax(0, static_cast<int>(m_style.sectionSpacing));
  const int buttonY = (bounds.getHeight() - buttonSize) / 2;

  int x = sectionSpacing;
  if (m_playPauseButton) {
    m_playPauseButton->setBounds(x, buttonY, buttonSize, buttonSize);
    x += buttonSize + spacing;
  }
  if (m_stopButton) {
    m_stopButton->setBounds(x, buttonY, buttonSize, buttonSize);
    x += buttonSize + spacing;
  }
  if (m_style.showRecord && m_recordButton) {
    m_recordButton->setBounds(x, buttonY, buttonSize, buttonSize);
    x += buttonSize + spacing;
  }

  x += sectionSpacing;
  if (m_style.showLoop && m_loopButton) {
    m_loopButton->setBounds(x, buttonY, buttonSize, buttonSize);
    x += buttonSize + sectionSpacing;
  }

  const int timeLabelWidth = 90;
  const int timeLabelHeight = 20;
  const int timeY = (bounds.getHeight() - timeLabelHeight) / 2;
  if (m_positionLabel) {
    m_positionLabel->setBounds(x, timeY, timeLabelWidth, timeLabelHeight);
    x += timeLabelWidth + 8;
  }
  if (m_durationLabel) {
    m_durationLabel->setBounds(x, timeY, timeLabelWidth, timeLabelHeight);
    x += timeLabelWidth + sectionSpacing;
  }
  if (m_style.showTempo && m_tempoLabel) {
    const int tempoWidth = 80;
    m_tempoLabel->setBounds(x, timeY, tempoWidth, timeLabelHeight);
    x += tempoWidth + sectionSpacing;
  }
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
  juce::Component::SafePointer<TransportBar> playThis(this);
  m_playPauseButton->onClick = [playThis] {
    if (auto* owner = playThis.getComponent())
      if (owner->onPlayPause)
        owner->onPlayPause();
  };
  addAndMakeVisible(*m_playPauseButton);

  // Stop button
  m_stopButton = std::make_unique<TransportButton>(TransportButton::Type::Stop);
  m_stopButton->setSize(ButtonSize::Large);
  m_stopButton->setStyle(ButtonStyle::Ghost);
  juce::Component::SafePointer<TransportBar> stopThis(this);
  m_stopButton->onClick = [stopThis] {
    if (auto* owner = stopThis.getComponent())
      if (owner->onStop)
        owner->onStop();
  };
  addAndMakeVisible(*m_stopButton);

  // Record button
  m_recordButton = std::make_unique<TransportButton>(TransportButton::Type::Record);
  m_recordButton->setSize(ButtonSize::Large);
  juce::Component::SafePointer<TransportBar> recordThis(this);
  m_recordButton->onClick = [recordThis] {
    if (auto* owner = recordThis.getComponent())
      if (owner->onRecord)
        owner->onRecord();
  };
  addAndMakeVisible(*m_recordButton);
  m_recordButton->setVisible(m_style.showRecord);

  // Loop button
  m_loopButton = std::make_unique<ToggleButton>(IconType::Loop);
  m_loopButton->setSize(ButtonSize::Large);
  m_loopButton->setStyle(ButtonStyle::Ghost);
  m_loopButton->setOnColor(tokens::lab::tone()); // accent (--lab-tone) when active
  juce::Component::SafePointer<TransportBar> loopThis(this);
  m_loopButton->onToggle = [loopThis](bool enabled) {
    if (auto* owner = loopThis.getComponent()) {
      owner->m_isLooping = enabled;
      if (owner->onLoopToggle)
        owner->onLoopToggle(enabled);
    }
  };
  addAndMakeVisible(*m_loopButton);
  m_loopButton->setVisible(m_style.showLoop);

  // Panic button
  m_panicButton = std::make_unique<TransportButton>(TransportButton::Type::Stop);
  m_panicButton->setSize(ButtonSize::Large);
  m_panicButton->setStyle(ButtonStyle::Destructive);
  m_panicButton->setTooltipText("Panic - Stop All");
  juce::Component::SafePointer<TransportBar> panicThis(this);
  m_panicButton->onClick = [panicThis] {
    if (auto* owner = panicThis.getComponent())
      if (owner->onPanic)
        owner->onPanic();
  };
  addAndMakeVisible(*m_panicButton);
  m_panicButton->setVisible(m_style.showPanic);

  // Position label
  m_positionLabel = std::make_unique<juce::Label>();
  m_positionLabel->setText("0:00.000", juce::dontSendNotification);
  m_positionLabel->setFont(juce::Font(14.0f, juce::Font::bold));
  m_positionLabel->setColour(juce::Label::textColourId, m_style.textColor);
  m_positionLabel->setJustificationType(juce::Justification::centredRight);
  addAndMakeVisible(*m_positionLabel);

  // Duration label
  m_durationLabel = std::make_unique<juce::Label>();
  m_durationLabel->setText("0:00.000", juce::dontSendNotification);
  m_durationLabel->setFont(juce::Font(14.0f));
  m_durationLabel->setColour(juce::Label::textColourId, m_style.dimTextColor);
  m_durationLabel->setJustificationType(juce::Justification::centredLeft);
  addAndMakeVisible(*m_durationLabel);

  // Tempo label
  m_tempoLabel = std::make_unique<juce::Label>();
  m_tempoLabel->setText("120.0 BPM", juce::dontSendNotification);
  m_tempoLabel->setFont(juce::Font(12.0f));
  m_tempoLabel->setColour(juce::Label::textColourId, m_style.dimTextColor);
  m_tempoLabel->setJustificationType(juce::Justification::centred);
  addAndMakeVisible(*m_tempoLabel);
  m_tempoLabel->setVisible(m_style.showTempo);
}
juce::String TransportBar::formatTime(double seconds) const {
  if (!std::isfinite(seconds) || seconds < 0.0)
    seconds = 0.0;

  const long double maxSeconds =
      static_cast<long double>(std::numeric_limits<int>::max()) * 3600.0L;
  const long double boundedSeconds = std::min(static_cast<long double>(seconds), maxSeconds);

  switch (m_timeFormat) {
  case TimeDisplayFormat::MinutesSeconds: {
    const int mins = static_cast<int>(boundedSeconds / 60.0L);
    const double secs = static_cast<double>(std::fmod(boundedSeconds, 60.0L));
    return juce::String::formatted("%d:%06.3f", mins, secs);
  }

  case TimeDisplayFormat::Timecode:
    return formatTimecode(static_cast<double>(boundedSeconds));

  case TimeDisplayFormat::Bars:
    return formatBars(static_cast<double>(boundedSeconds));

  case TimeDisplayFormat::Samples:
    return formatSamples(saturatingSamples(seconds, m_sampleRate));

  default:
    return juce::String(static_cast<double>(boundedSeconds), 3);
  }
}

juce::String TransportBar::formatTimecode(double seconds) const {
  const double boundedSeconds = std::min(
      std::max(0.0, seconds), static_cast<double>(std::numeric_limits<int>::max()) * 3600.0);
  const int hours = static_cast<int>(boundedSeconds / 3600.0);
  const int mins = static_cast<int>(std::fmod(boundedSeconds, 3600.0) / 60.0);
  const int secs = static_cast<int>(std::fmod(boundedSeconds, 60.0));
  const int frames = static_cast<int>(std::fmod(boundedSeconds * 30.0, 30.0));

  return juce::String::formatted("%02d:%02d:%02d:%02d", hours, mins, secs, frames);
}

juce::String TransportBar::formatBars(double seconds) const {
  const long double beatsPerSecond = static_cast<long double>(m_tempoBPM) / 60.0L;
  const int signature = juce::jmax(1, m_timeSignatureNum);
  const long double maxTotalBeats =
      static_cast<long double>(std::numeric_limits<int>::max() - 1) * signature;
  const long double totalBeats =
      std::min(std::max(0.0L, static_cast<long double>(seconds) * beatsPerSecond), maxTotalBeats);
  const int bars = static_cast<int>(totalBeats / signature) + 1;
  const int beats = static_cast<int>(std::fmod(totalBeats, signature)) + 1;
  const int ticks = static_cast<int>(std::fmod(totalBeats, 1.0L) * 480.0L);

  return juce::String::formatted("%d.%d.%03d", bars, beats, ticks);
}

juce::String TransportBar::formatSamples(int64_t samples) const {
  return juce::String(samples);
}

} // namespace shmui
