/*
  ==============================================================================

    ToggleButton.cpp
    Created: shmui Button System

    Toggle button implementation.

  ==============================================================================
*/

#include "ToggleButton.h"

namespace shmui {

//==============================================================================
ToggleButton::ToggleButton(IconType iconOff, IconType iconOn)
    : m_iconOff(iconOff), m_iconOn(iconOn) {
  juce::Component::SafePointer<ToggleButton> safeThis(this);
  onClick = [safeThis] {
    if (auto* owner = safeThis.getComponent())
      owner->handleClick();
  };
}

ToggleButton::ToggleButton(IconType icon) : m_iconOff(icon), m_iconOn(icon) {
  juce::Component::SafePointer<ToggleButton> safeThis(this);
  onClick = [safeThis] {
    if (auto* owner = safeThis.getComponent())
      owner->handleClick();
  };
}

//==============================================================================
void ToggleButton::setToggled(bool toggled) {
  if (!requireMessageThread())
    return;
  if (m_isToggled != toggled) {
    m_isToggled = toggled;
    repaint();
  }
}

void ToggleButton::setIcons(IconType iconOff, IconType iconOn) {
  if (!requireMessageThread())
    return;
  m_iconOff = iconOff;
  m_iconOn = iconOn;
  repaint();
}

void ToggleButton::setOnColor(juce::Colour color) {
  if (!requireMessageThread())
    return;
  m_onColor = color;
  m_hasOnColor = true;
  repaint();
}

void ToggleButton::clearOnColor() {
  if (!requireMessageThread())
    return;
  m_hasOnColor = false;
  repaint();
}

int ToggleButton::getPreferredSize() const {
  return static_cast<int>(getButtonHeight(getButtonSize()));
}

//==============================================================================
void ToggleButton::paintContent(juce::Graphics& g, juce::Rectangle<float> bounds,
                                juce::Colour foregroundColor) {
  const float iconSize = getIconSizeForButton(getButtonSize());

  // Use on color if toggled and custom color is set
  juce::Colour iconColor = foregroundColor;
  if (m_isToggled && m_hasOnColor) {
    iconColor = m_onColor;
  }

  // Center the icon in bounds
  auto iconBounds = bounds.withSizeKeepingCentre(iconSize, iconSize);

  IconType currentIcon = m_isToggled ? m_iconOn : m_iconOff;
  Icons::drawIcon(g, currentIcon, iconBounds, iconColor);
}

void ToggleButton::handleClick() {
  if (!requireMessageThread())
    return;
  setToggled(!m_isToggled);
  if (onToggle) {
    juce::Component::SafePointer<ToggleButton> safeThis(this);
    onToggle(m_isToggled);
    if (safeThis == nullptr)
      return;
  }
}

} // namespace shmui
