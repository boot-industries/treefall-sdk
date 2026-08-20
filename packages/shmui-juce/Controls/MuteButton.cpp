/*
  ==============================================================================

    MuteButton.cpp
    Created: shmui Button System

    Mute/Solo button implementation.

  ==============================================================================
*/

#include "MuteButton.h"

#include "../Utils/DesignTokens.h"

namespace shmui {

//==============================================================================
MuteButton::MuteButton(Type type) : m_type(type) {
  setStyle(ButtonStyle::Ghost);
  setSize(ButtonSize::Small);
  juce::Component::SafePointer<MuteButton> safeThis(this);
  onClick = [safeThis] {
    if (auto* owner = safeThis.getComponent())
      owner->handleClick();
  };
}

//==============================================================================
void MuteButton::setType(Type type) {
  if (!requireMessageThread())
    return;
  if (m_type != type) {
    m_type = type;
    repaint();
  }
}

void MuteButton::setActive(bool active) {
  if (!requireMessageThread())
    return;
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
  const juce::Colour iconColor = m_isActive ? getActiveColor() : foregroundColor;
  const auto iconBounds = bounds.withSizeKeepingCentre(iconSize, iconSize);
  Icons::drawIcon(g, getCurrentIcon(), iconBounds, iconColor);
}

void MuteButton::handleClick() {
  if (!requireMessageThread())
    return;
  setActive(!m_isActive);
  if (onToggle) {
    juce::Component::SafePointer<MuteButton> safeThis(this);
    onToggle(m_isActive);
    if (safeThis == nullptr)
      return;
  }
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
  const auto& theme = defaultTheme();
  switch (m_type) {
  case Type::Mute:
    return theme.danger;

  case Type::Solo:
    return theme.warning;

  case Type::Bypass:
    return theme.fgMuted;

  default:
    return theme.fg;
  }
}

} // namespace shmui
