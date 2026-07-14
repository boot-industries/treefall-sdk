/*
  ==============================================================================

    TransportButton.cpp
    Created: shmui Button System

    Transport button implementation.

  ==============================================================================
*/

#include "TransportButton.h"

#include "../Utils/DesignTokens.h"

namespace shmui {

//==============================================================================
TransportButton::TransportButton(Type type) : m_type(type) {
  // Default style based on type
  switch (type) {
  case Type::Stop:
    setStyle(ButtonStyle::Destructive);
    break;
  case Type::Record:
    setStyle(ButtonStyle::Destructive);
    break;
  default:
    setStyle(ButtonStyle::Ghost);
    break;
  }
}

TransportButton::TransportButton(Type type, ButtonStyle style) : m_type(type) {
  setStyle(style);
}

//==============================================================================
void TransportButton::setType(Type type) {
  if (m_type != type) {
    m_type = type;
    repaint();
  }
}

void TransportButton::setPlaying(bool playing) {
  if (m_isPlaying != playing) {
    m_isPlaying = playing;
    if (m_type == Type::PlayPause)
      repaint();
  }
}

void TransportButton::setRecording(bool recording) {
  if (m_isRecording != recording) {
    m_isRecording = recording;
    if (m_type == Type::Record)
      repaint();
  }
}

void TransportButton::setLooping(bool looping) {
  if (m_isLooping != looping) {
    m_isLooping = looping;
    if (m_type == Type::Loop)
      repaint();
  }
}

int TransportButton::getPreferredSize() const {
  return static_cast<int>(getButtonHeight(getButtonSize()));
}

//==============================================================================
void TransportButton::paintContent(juce::Graphics& g, juce::Rectangle<float> bounds,
                                   juce::Colour foregroundColor) {
  const float iconSize = getIconSizeForButton(getButtonSize());

  // Special color handling for active states
  juce::Colour iconColor = foregroundColor;
  if (m_type == Type::Record && m_isRecording) {
    iconColor = tokens::lab::danger(); // --lab-danger when recording
  } else if (m_type == Type::Loop && m_isLooping) {
    iconColor = tokens::lab::tone(); // accent (--lab-tone) when looping
  }

  // Center the icon in bounds
  auto iconBounds = bounds.withSizeKeepingCentre(iconSize, iconSize);

  Icons::drawIcon(g, getCurrentIcon(), iconBounds, iconColor);
}

IconType TransportButton::getCurrentIcon() const {
  switch (m_type) {
  case Type::Play:
    return IconType::Play;

  case Type::Pause:
    return IconType::Pause;

  case Type::PlayPause:
    return m_isPlaying ? IconType::Pause : IconType::Play;

  case Type::Stop:
    return IconType::Stop;

  case Type::Record:
    return IconType::Record;

  case Type::FastForward:
    return IconType::FastForward;

  case Type::Rewind:
    return IconType::Rewind;

  case Type::SkipNext:
    return IconType::SkipNext;

  case Type::SkipPrev:
    return IconType::SkipPrev;

  case Type::Loop:
    return IconType::Loop;

  default:
    return IconType::Play;
  }
}

} // namespace shmui
