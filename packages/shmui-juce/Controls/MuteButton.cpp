/*
  ==============================================================================

    MuteButton.cpp
    Created: shmui Button System

    Mute/Solo button implementation.

  ==============================================================================
*/

#include "MuteButton.h"

namespace shmui {

//==============================================================================
MuteButton::MuteButton(Type type) : m_type(type) {
  setStyle(ButtonStyle::Ghost);
  setSize(ButtonSize::Small);
  onClick = [this] { handleClick(); };
}

//==============================================================================
void MuteButton::setType(Type type) {
  if (m_type != type) {
    m_type = type;
    repaint();
  }
}

void MuteButton::setActive(bool active) {
  if (m_isActive != active) {
    m_isActive = active;
    repaint();
  }
}

int MuteButton::getPreferredSize() const {
  return static_cast<int>(getButtonHeight(getButtonSize()));
}

//==============================================================================
void MuteButton::paintContent(juce::Graphics& g, juce::Rectangle<float> bounds,
                              juce::Colour foregroundColor) {
  const float iconSize = getIconSizeForButton(getButtonSize());

  // Use active color when active
  juce::Colour iconColor = m_isActive ? getActiveColor() : foregroundColor;

  // Center the icon in bounds
  auto iconBounds = bounds.withSizeKeepingCentre(iconSize, iconSize);

  Icons::drawIcon(g, getCurrentIcon(), iconBounds, iconColor);
}

void MuteButton::handleClick() {
  setActive(!m_isActive);
  if (onToggle)
    onToggle(m_isActive);
}

IconType MuteButton::getCurrentIcon() const {
  switch (m_type) {
  case Type::Mute:
    return IconType::Mute;
  case Type::Solo:
    return IconType::Solo;
  case Type::Bypass:
    return IconType::Bypass;
  default:
    return IconType::Mute;
  }
}

juce::Colour MuteButton::getActiveColor() const {
  switch (m_type) {
  case Type::Mute:
    return juce::Colour(0xFFEF4444); // Red for mute

  case Type::Solo:
    return juce::Colour(0xFFF59E0B); // Amber for solo

  case Type::Bypass:
    return juce::Colour(0xFF6B7280); // Gray for bypass

  default:
    return juce::Colours::white;
  }
}

} // namespace shmui
