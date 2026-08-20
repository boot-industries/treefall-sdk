/*
  ==============================================================================

    Button.cpp
    Created: shmui Button System

    Base button implementation with animation and state handling.

  ==============================================================================
*/

#include "Button.h"

namespace shmui {

//==============================================================================
/**
 * @brief Internal timer for button animations.
 */
class Button::AnimationTimer : public juce::Timer {
public:
  AnimationTimer(Button& owner) : m_owner(owner) {}

  void timerCallback() override {
    m_owner.animationTick();
  }

private:
  Button& m_owner;
};

//==============================================================================
Button::Button() : m_animTimer(std::make_unique<AnimationTimer>(*this)) {
  setWantsKeyboardFocus(true);
  setMouseCursor(juce::MouseCursor::PointingHandCursor);
  addDefaultThemeListener(this);
}

Button::~Button() {
  stopAnimation();
  removeDefaultThemeListener(this);
}

void Button::defaultThemeChanged(const ShmuiTheme&) {
  if (requireMessageThread())
    repaint();
}

//==============================================================================
void Button::setStyle(ButtonStyle style) {
  if (!requireMessageThread())
    return;
  if (m_style != style) {
    m_style = style;
    repaint();
  }
}

void Button::setSize(ButtonSize size) {
  if (!requireMessageThread())
    return;
  if (m_size != size) {
    m_size = size;
    repaint();
  }
}

void Button::setDarkTheme(bool isDark) {
  if (!requireMessageThread())
    return;
  if (m_isDarkTheme != isDark) {
    m_isDarkTheme = isDark;
    repaint();
  }
}

void Button::setCustomColors(const ButtonColors& colors) {
  if (!requireMessageThread())
    return;
  m_customColors = colors;
  m_hasCustomColors = true;
  repaint();
}

void Button::clearCustomColors() {
  if (!requireMessageThread())
    return;
  m_hasCustomColors = false;
  repaint();
}

ButtonColors Button::getEffectiveColors() const {
  if (m_hasCustomColors)
    return m_customColors;

  // Flavor-aware: resolve from the active Orpheus theme (Lab by default).
  // Each flavor already encodes its own dark-first surfaces, so the legacy
  // m_isDarkTheme flag no longer selects a palette (kept for API compat).
  return getButtonColors(m_style, defaultTheme());
}

//==============================================================================
void Button::setEnabled(bool enabled) {
  if (!requireMessageThread())
    return;
  if (m_isEnabled != enabled) {
    m_isEnabled = enabled;
    if (!enabled) {
      m_isHovered = false;
      m_isPressed = false;
    }
    setMouseCursor(enabled ? juce::MouseCursor::PointingHandCursor
                           : juce::MouseCursor::NormalCursor);
    repaint();
  }
}

void Button::setLoading(bool loading) {
  if (!requireMessageThread())
    return;
  if (m_isLoading != loading) {
    m_isLoading = loading;
    if (loading) {
      startAnimation();
    } else if (!m_isHovered && !m_isPressed) {
      stopAnimation();
    }
    repaint();
  }
}

void Button::setTooltipText(const juce::String& tooltip) {
  if (!requireMessageThread())
    return;
  m_tooltipText = tooltip;
  setTooltip(tooltip);
}

//==============================================================================
void Button::paint(juce::Graphics& g) {
  auto bounds = getLocalBounds().toFloat();
  const auto colors = getEffectiveColors();
  const float cornerRadius = getCornerRadiusForButton(m_size);
  const float padding = getPaddingForButton(m_size);

  // Calculate animated background color
  juce::Colour bgColor = colors.background;
  if (!m_isEnabled) {
    bgColor = colors.background.withAlpha(0.5f);
  } else {
    bgColor = bgColor.interpolatedWith(colors.backgroundHover, m_hoverOpacity);
    bgColor = bgColor.interpolatedWith(colors.backgroundPressed, m_pressOpacity);
  }

  // Draw background
  if (bgColor.getAlpha() > 0) {
    g.setColour(bgColor);
    g.fillRoundedRectangle(bounds, cornerRadius);
  }

  // Draw border
  juce::Colour borderColor = colors.border.interpolatedWith(colors.borderHover, m_hoverOpacity);
  if (borderColor.getAlpha() > 0) {
    g.setColour(borderColor);
    g.drawRoundedRectangle(bounds.reduced(0.5f), cornerRadius, 1.0f);
  }

  // Draw focus ring
  if (m_focusOpacity > 0.0f) {
    g.setColour(colors.foreground.withAlpha(m_focusOpacity * 0.3f));
    g.drawRoundedRectangle(bounds.expanded(2.0f), cornerRadius + 2.0f, 2.0f);
  }

  // Calculate foreground color
  juce::Colour fgColor = m_isEnabled ? colors.foreground : colors.foregroundDisabled;

  // Draw content
  auto contentBounds = bounds.reduced(padding);

  if (m_isLoading) {
    // Draw loading spinner
    auto spinnerBounds = contentBounds.withSizeKeepingCentre(getIconSizeForButton(m_size),
                                                             getIconSizeForButton(m_size));

    g.setColour(fgColor);
    juce::Path spinner;
    spinner.addArc(spinnerBounds.getX(), spinnerBounds.getY(), spinnerBounds.getWidth(),
                   spinnerBounds.getHeight(), m_loadingAngle,
                   m_loadingAngle + juce::MathConstants<float>::pi * 1.5f, true);
    g.strokePath(spinner, juce::PathStrokeType(2.0f));
  } else {
    paintContent(g, contentBounds, fgColor);
  }
}

void Button::paintContent(juce::Graphics& g, juce::Rectangle<float> bounds,
                          juce::Colour foregroundColor) {
  // Base implementation does nothing - subclasses override
  juce::ignoreUnused(g, bounds, foregroundColor);
}

void Button::resized() {
  // Base implementation does nothing
}

//==============================================================================
void Button::mouseEnter(const juce::MouseEvent& e) {
  juce::ignoreUnused(e);
  if (m_isEnabled && !m_isLoading) {
    m_isHovered = true;
    startAnimation();
  }
}

void Button::mouseExit(const juce::MouseEvent& e) {
  juce::ignoreUnused(e);
  m_isHovered = false;
  m_isPressed = false;
  if (!m_isLoading && m_hoverOpacity < 0.01f && m_pressOpacity < 0.01f) {
    stopAnimation();
  }
}

void Button::mouseDown(const juce::MouseEvent& e) {
  if (m_isEnabled && !m_isLoading) {
    if (e.mods.isRightButtonDown()) {
      if (onRightClick) {
        juce::Component::SafePointer<Button> safeThis(this);
        onRightClick();
        if (safeThis == nullptr)
          return;
      }
    } else {
      m_isPressed = true;
      startAnimation();
    }
  }
}

void Button::mouseUp(const juce::MouseEvent& e) {
  if (m_isPressed && m_isEnabled && !m_isLoading) {
    m_isPressed = false;

    if (getLocalBounds().contains(e.getPosition())) {
      if (onClick) {
        juce::Component::SafePointer<Button> safeThis(this);
        onClick();
        if (safeThis == nullptr)
          return;
      }
    }
  }
}

void Button::focusGained(FocusChangeType cause) {
  juce::ignoreUnused(cause);
  m_hasFocus = true;
  startAnimation();
}

void Button::focusLost(FocusChangeType cause) {
  juce::ignoreUnused(cause);
  m_hasFocus = false;
  if (!m_isHovered && !m_isLoading) {
    stopAnimation();
  }
}

bool Button::keyPressed(const juce::KeyPress& key) {
  if (m_isEnabled && !m_isLoading) {
    if (key == juce::KeyPress::returnKey || key == juce::KeyPress::spaceKey) {
      if (onClick) {
        juce::Component::SafePointer<Button> safeThis(this);
        onClick();
        if (safeThis == nullptr)
          return true;
      }
      return true;
    }
  }
  return false;
}

//==============================================================================
void Button::startAnimation() {
  if (!m_animTimer->isTimerRunning()) {
    m_lastAnimTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    m_animTimer->startTimerHz(60);
  }
}

void Button::stopAnimation() {
  m_animTimer->stopTimer();
}

void Button::animationTick() {
  const double currentTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
  const float deltaTime = static_cast<float>(currentTime - m_lastAnimTime);
  m_lastAnimTime = currentTime;

  bool needsRepaint = false;
  bool animationComplete = true;

  // Animate hover
  const float targetHover = (m_isHovered && m_isEnabled && !m_isLoading) ? 1.0f : 0.0f;
  if (std::abs(m_hoverOpacity - targetHover) > 0.001f) {
    m_hoverOpacity = Interpolation::smoothDelta(m_hoverOpacity, targetHover, 0.3f, deltaTime);
    needsRepaint = true;
    animationComplete = false;
  }

  // Animate press
  const float targetPress = (m_isPressed && m_isEnabled && !m_isLoading) ? 1.0f : 0.0f;
  if (std::abs(m_pressOpacity - targetPress) > 0.001f) {
    m_pressOpacity = Interpolation::smoothDelta(m_pressOpacity, targetPress, 0.5f, deltaTime);
    needsRepaint = true;
    animationComplete = false;
  }

  // Animate focus
  const float targetFocus = m_hasFocus ? 1.0f : 0.0f;
  if (std::abs(m_focusOpacity - targetFocus) > 0.001f) {
    m_focusOpacity = Interpolation::smoothDelta(m_focusOpacity, targetFocus, 0.25f, deltaTime);
    needsRepaint = true;
    animationComplete = false;
  }

  // Animate loading spinner
  if (m_isLoading) {
    m_loadingAngle += deltaTime * juce::MathConstants<float>::twoPi * 1.5f; // 1.5 rotations/sec
    if (m_loadingAngle > juce::MathConstants<float>::twoPi)
      m_loadingAngle -= juce::MathConstants<float>::twoPi;
    needsRepaint = true;
    animationComplete = false;
  }

  if (needsRepaint)
    repaint();

  if (animationComplete && !m_isHovered && !m_hasFocus && !m_isLoading)
    stopAnimation();
}

} // namespace shmui
