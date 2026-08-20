/*
  ==============================================================================

    TransportContainer.cpp
    Created: shmui Component Library

    Slot-based, composable transport strip.

  ==============================================================================
*/

#include "TransportContainer.h"

namespace shmui {

//==============================================================================
TransportContainer::TransportContainer() {
  const auto& theme = defaultTheme();
  m_style.backgroundColor = theme.bgPanel;
  m_style.textColor = theme.fg;
  m_style.dimTextColor = theme.fgMuted;
  m_style.separatorColor = theme.stroke;
  addDefaultThemeListener(this);
}

TransportContainer::~TransportContainer() {
  removeDefaultThemeListener(this);
}

//==============================================================================
std::vector<TransportContainer::Item>& TransportContainer::regionItems(Region region) {
  switch (region) {
  case Region::Left:
    return m_left;
  case Region::Center:
    return m_center;
  case Region::Right:
    return m_right;
  }
  return m_left;
}

void TransportContainer::addItem(Region region, juce::Component* child, int flexWidth) {
  if (!requireMessageThread() || child == nullptr)
    return;

  addAndMakeVisible(*child);
  regionItems(region).push_back({child, flexWidth});
  resized();
}

void TransportContainer::addSpacer(Region region, int px) {
  if (!requireMessageThread())
    return;
  regionItems(region).push_back({nullptr, juce::jmax(0, px)});
  resized();
}

void TransportContainer::clearItems() {
  if (!requireMessageThread())
    return;
  for (auto* v : {&m_left, &m_center, &m_right}) {
    for (auto& item : *v)
      if (item.component != nullptr)
        removeChildComponent(item.component);
    v->clear();
  }
  resized();
}

//==============================================================================
void TransportContainer::setStyle(const TransportBarStyle& style) {
  if (!requireMessageThread())
    return;
  m_style = style;
  m_usesDefaultThemeStyle = false;
  repaint();
  resized();
}

void TransportContainer::defaultThemeChanged(const ShmuiTheme& theme) {
  if (!requireMessageThread() || !m_usesDefaultThemeStyle)
    return;
  m_style.backgroundColor = theme.bgPanel;
  m_style.textColor = theme.fg;
  m_style.dimTextColor = theme.fgMuted;
  m_style.separatorColor = theme.stroke;
  repaint();
}

//==============================================================================
void TransportContainer::paint(juce::Graphics& g) {
  g.fillAll(m_style.backgroundColor);

  // Separators between the three regions (drawn where content meets).
  g.setColour(m_style.separatorColor);
  // A single hairline under the whole strip reads as a console bezel.
  g.drawLine(0.0f, static_cast<float>(getHeight()) - 0.5f, static_cast<float>(getWidth()),
             static_cast<float>(getHeight()) - 0.5f, 1.0f);
}

void TransportContainer::layoutRegion(juce::FlexBox& box, std::vector<Item>& items) {
  for (auto& item : items) {
    juce::FlexItem fi;
    if (item.component != nullptr) {
      fi = juce::FlexItem(*item.component);
      const int w = (item.flexWidth >= 0) ? item.flexWidth : item.component->getWidth();
      fi = fi.withWidth(static_cast<float>(juce::jmax(0, w)));
    } else {
      // Spacer.
      fi = juce::FlexItem().withWidth(static_cast<float>(juce::jmax(0, item.flexWidth)));
    }
    const float margin = juce::jmax(0.0f, m_style.buttonSpacing * 0.5f);
    fi = fi.withMargin(juce::FlexItem::Margin(0.0f, margin, 0.0f, margin));
    box.items.add(fi);
  }
}

void TransportContainer::resized() {
  if (!requireMessageThread())
    return;
  auto bounds = getLocalBounds();
  const int third = bounds.getWidth() / 3;

  auto leftArea = bounds.removeFromLeft(third);
  auto rightArea = bounds.removeFromRight(third);
  auto centerArea = bounds; // remainder

  juce::FlexBox leftBox, centerBox, rightBox;
  leftBox.flexDirection = centerBox.flexDirection = rightBox.flexDirection =
      juce::FlexBox::Direction::row;
  leftBox.alignItems = centerBox.alignItems = rightBox.alignItems =
      juce::FlexBox::AlignItems::center;

  leftBox.justifyContent = juce::FlexBox::JustifyContent::flexStart;
  centerBox.justifyContent = juce::FlexBox::JustifyContent::center;
  rightBox.justifyContent = juce::FlexBox::JustifyContent::flexEnd;

  layoutRegion(leftBox, m_left);
  layoutRegion(centerBox, m_center);
  layoutRegion(rightBox, m_right);

  leftBox.performLayout(leftArea);
  centerBox.performLayout(centerArea);
  rightBox.performLayout(rightArea);
}

} // namespace shmui
