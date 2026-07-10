/*
  ==============================================================================

    MeterGroup.cpp
    Created: shmui Component Library

    Composite "N groups + Master" level-meter layout.

  ==============================================================================
*/

#include "MeterGroup.h"

namespace shmui {

//==============================================================================
namespace {
// Default group accent colours = Orpheus --group-* (A/B/C/D), cycling if
// there are more than four groups.
juce::Colour defaultGroupColour(int index) {
  switch (index % 4) {
  case 0:
    return tokens::group::blue();
  case 1:
    return tokens::group::green();
  case 2:
    return tokens::group::orange();
  default:
    return tokens::group::red();
  }
}
} // namespace

//==============================================================================
MeterGroup::MeterGroup(int numGroups, bool withMaster) {
  numGroups = juce::jmax(1, numGroups);

  for (int i = 0; i < numGroups; ++i) {
    GroupStrip strip;
    strip.meter = std::make_unique<LevelMeter>(2); // stereo per group
    strip.label = juce::String::charToString(static_cast<juce_wchar>('A' + i));
    strip.colour = defaultGroupColour(i);
    addAndMakeVisible(*strip.meter);
    m_groups.push_back(std::move(strip));
  }

  if (withMaster) {
    m_master = std::make_unique<LevelMeter>(2);
    addAndMakeVisible(*m_master);
  }
}

//==============================================================================
void MeterGroup::setGroupLevel(int group, float linear) {
  if (group >= 0 && group < getNumGroups()) {
    m_groups[static_cast<size_t>(group)].meter->setLevel(0, linear);
    m_groups[static_cast<size_t>(group)].meter->setLevel(1, linear);
  }
}

void MeterGroup::setGroupLevelDB(int group, float dB) {
  if (group >= 0 && group < getNumGroups()) {
    m_groups[static_cast<size_t>(group)].meter->setLevelDB(0, dB);
    m_groups[static_cast<size_t>(group)].meter->setLevelDB(1, dB);
  }
}

void MeterGroup::setMasterLevel(float linear) {
  if (m_master) {
    m_master->setLevel(0, linear);
    m_master->setLevel(1, linear);
  }
}

void MeterGroup::setMasterLevelDB(float dB) {
  if (m_master) {
    m_master->setLevelDB(0, dB);
    m_master->setLevelDB(1, dB);
  }
}

void MeterGroup::reset() {
  for (auto& s : m_groups)
    s.meter->reset();
  if (m_master)
    m_master->reset();
}

//==============================================================================
void MeterGroup::setGroupLabel(int group, const juce::String& label) {
  if (group >= 0 && group < getNumGroups()) {
    m_groups[static_cast<size_t>(group)].label = label;
    repaint();
  }
}

void MeterGroup::setMasterLabel(const juce::String& label) {
  m_masterLabel = label;
  repaint();
}

void MeterGroup::setGroupColour(int group, juce::Colour colour) {
  if (group >= 0 && group < getNumGroups()) {
    m_groups[static_cast<size_t>(group)].colour = colour;
    repaint();
  }
}

void MeterGroup::setMeterStyle(const LevelMeterStyle& style) {
  for (auto& s : m_groups)
    s.meter->setStyle(style);
  if (m_master)
    m_master->setStyle(style);
}

void MeterGroup::setBallistics(MeterBallistics ballistics) {
  for (auto& s : m_groups)
    s.meter->setBallistics(ballistics);
  if (m_master)
    m_master->setBallistics(ballistics);
}

//==============================================================================
LevelMeter& MeterGroup::getGroupMeter(int group) {
  jassert(group >= 0 && group < getNumGroups());
  return *m_groups[static_cast<size_t>(group)].meter;
}

LevelMeter& MeterGroup::getMasterMeter() {
  jassert(m_master != nullptr);
  return *m_master;
}

//==============================================================================
void MeterGroup::enableHistory(int capacityPerMeter) {
  const int masterChannel = getNumGroups();

  for (int i = 0; i < getNumGroups(); ++i) {
    auto& meter = *m_groups[static_cast<size_t>(i)].meter;
    meter.enableHistory(capacityPerMeter);
    meter.onLevelEvent = [this, i](const LevelEvent& ev) {
      LevelEvent out = ev;
      out.channel = i; // report the group index
      if (onLevelEvent)
        onLevelEvent(out);
    };
  }

  if (m_master) {
    m_master->enableHistory(capacityPerMeter);
    m_master->onLevelEvent = [this, masterChannel](const LevelEvent& ev) {
      LevelEvent out = ev;
      out.channel = masterChannel; // Master reported as numGroups
      if (onLevelEvent)
        onLevelEvent(out);
    };
  }
}

void MeterGroup::disableHistory() {
  for (auto& s : m_groups) {
    s.meter->disableHistory();
    s.meter->onLevelEvent = nullptr;
  }
  if (m_master) {
    m_master->disableHistory();
    m_master->onLevelEvent = nullptr;
  }
}

//==============================================================================
void MeterGroup::paint(juce::Graphics& g) {
  auto area = getLocalBounds();
  auto labelRow = area.removeFromBottom(static_cast<int>(LABEL_HEIGHT));

  const int total = getNumGroups() + (m_master ? 1 : 0);
  if (total == 0)
    return;

  // Recompute the same strip widths used in resized() to place labels.
  const int gaps = (getNumGroups() > 0 ? getNumGroups() - 1 : 0);
  const float extraMasterGap = m_master ? (MASTER_GAP - STRIP_GAP) : 0.0f;
  const float usable = static_cast<float>(labelRow.getWidth()) -
                       STRIP_GAP * static_cast<float>(gaps) - extraMasterGap;
  const float stripW = usable / static_cast<float>(total);

  float x = static_cast<float>(labelRow.getX());
  for (int i = 0; i < getNumGroups(); ++i) {
    juce::Rectangle<int> lb(juce::roundToInt(x), labelRow.getY(), juce::roundToInt(stripW),
                            labelRow.getHeight());
    drawLabel(g, lb, m_groups[static_cast<size_t>(i)].label,
              m_groups[static_cast<size_t>(i)].colour);
    x += stripW + STRIP_GAP;
  }
  if (m_master) {
    x += (MASTER_GAP - STRIP_GAP);
    juce::Rectangle<int> lb(juce::roundToInt(x), labelRow.getY(), juce::roundToInt(stripW),
                            labelRow.getHeight());
    drawLabel(g, lb, m_masterLabel, m_masterColour);
  }
}

void MeterGroup::drawLabel(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& label,
                           juce::Colour colour) {
  g.setColour(colour);
  g.setFont(juce::Font(11.0f, juce::Font::bold));
  g.drawText(label, area, juce::Justification::centred, false);
}

void MeterGroup::resized() {
  auto area = getLocalBounds();
  area.removeFromBottom(static_cast<int>(LABEL_HEIGHT)); // reserve label row

  const int total = getNumGroups() + (m_master ? 1 : 0);
  if (total == 0)
    return;

  const int gaps = (getNumGroups() > 0 ? getNumGroups() - 1 : 0);
  const float extraMasterGap = m_master ? (MASTER_GAP - STRIP_GAP) : 0.0f;
  const float usable =
      static_cast<float>(area.getWidth()) - STRIP_GAP * static_cast<float>(gaps) - extraMasterGap;
  const float stripW = usable / static_cast<float>(total);

  float x = static_cast<float>(area.getX());
  for (auto& s : m_groups) {
    s.meter->setBounds(juce::roundToInt(x), area.getY(), juce::roundToInt(stripW),
                       area.getHeight());
    x += stripW + STRIP_GAP;
  }
  if (m_master) {
    x += (MASTER_GAP - STRIP_GAP);
    m_master->setBounds(juce::roundToInt(x), area.getY(), juce::roundToInt(stripW),
                        area.getHeight());
  }
}

} // namespace shmui
