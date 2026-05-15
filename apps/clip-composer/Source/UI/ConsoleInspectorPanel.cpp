// SPDX-License-Identifier: MIT

#include "ConsoleInspectorPanel.h"
#include "ConsoleTheme.h"

void ConsoleInspectorPanel::setSnapshot(const occ::ui::ClipComposerUiSnapshot& snapshot) {
  m_snapshot = snapshot;
  repaint();
}

void ConsoleInspectorPanel::setOperatorViewMode(occ::ui::OperatorViewMode mode) {
  if (m_mode == mode)
    return;
  m_mode = mode;
  repaint();
}

void ConsoleInspectorPanel::paint(juce::Graphics& g) {
  g.fillAll(juce::Colour(OCC::Design::kBgSurface));
  g.setColour(juce::Colour(OCC::Design::kBorderDefault));
  g.drawLine(0, 0, 0, getHeight(), 1.0f);

  auto bounds = getLocalBounds().reduced(18, 16);
  switch (m_mode) {
  case occ::ui::OperatorViewMode::Playout:
    drawPlayout(g, bounds);
    break;
  case occ::ui::OperatorViewMode::Edit:
    drawEdit(g, bounds);
    break;
  case occ::ui::OperatorViewMode::Routing:
    drawRouting(g, bounds);
    break;
  case occ::ui::OperatorViewMode::Preferences:
    drawPreferences(g, bounds);
    break;
  }
}

void ConsoleInspectorPanel::drawSectionHeader(juce::Graphics& g, juce::Rectangle<int>& bounds,
                                              const juce::String& title,
                                              const juce::String& eyebrow) {
  g.setFont(OCC::Console::monoFont(10.0f));
  g.setColour(juce::Colour(OCC::Design::kTextSecondary));
  g.drawText(eyebrow, bounds.removeFromTop(18), juce::Justification::centredLeft, false);
  g.setFont(OCC::Console::consoleFont(20.0f, juce::Font::bold));
  g.setColour(juce::Colour(OCC::Design::kTextPrimary));
  g.drawText(title, bounds.removeFromTop(30), juce::Justification::centredLeft, false);
  bounds.removeFromTop(10);
}

void ConsoleInspectorPanel::drawRow(juce::Graphics& g, juce::Rectangle<int> row,
                                    juce::Colour stripe, const juce::String& left,
                                    const juce::String& right) {
  auto r = row.toFloat().reduced(0.5f);
  g.setColour(juce::Colour(OCC::Design::kBgInset));
  g.fillRoundedRectangle(r, 3.0f);
  g.setColour(juce::Colour(OCC::Design::kBorderDefault).withAlpha(0.9f));
  g.drawRoundedRectangle(r, 3.0f, 1.0f);
  g.setColour(stripe);
  g.fillRoundedRectangle(r.withWidth(4.0f), 3.0f);

  row.reduce(12, 0);
  g.setFont(OCC::Console::consoleFont(15.0f, juce::Font::bold));
  g.setColour(juce::Colour(OCC::Design::kTextPrimary));
  g.drawText(left, row.removeFromLeft(row.getWidth() - 120), juce::Justification::centredLeft,
             false);
  g.setFont(OCC::Console::monoFont(11.0f, juce::Font::plain));
  g.setColour(juce::Colour(OCC::Design::kTextPrimary));
  g.drawText(right, row, juce::Justification::centredRight, false);
}

void ConsoleInspectorPanel::drawPlayout(juce::Graphics& g, juce::Rectangle<int> bounds) {
  drawSectionHeader(g, bounds, "Playout", "NOW PLAYING");
  int rows = 0;
  for (const auto& clip : m_snapshot.session.clips) {
    if (!clip.hasClip || clip.playbackState == orpheus::PlaybackState::Stopped)
      continue;
    const auto row = bounds.removeFromTop(38);
    const auto stripe =
        clip.color.isTransparent() ? juce::Colour(OCC::Design::kGroupBlue) : clip.color;
    const auto time = juce::String(static_cast<int>(clip.playbackProgress * 100.0f)) + "%";
    drawRow(g, row, stripe,
            juce::String(clip.buttonIndex + 1).paddedLeft('0', 3) + "  " + clip.displayName, time);
    bounds.removeFromTop(8);
    if (++rows == 6)
      break;
  }

  if (rows == 0) {
    g.setFont(OCC::Console::consoleFont(15.0f, juce::Font::bold));
    g.setColour(juce::Colour(OCC::Design::kTextSecondary));
    g.drawText("No clips playing", bounds.removeFromTop(32), juce::Justification::centredLeft,
               false);
  }
}

void ConsoleInspectorPanel::drawEdit(juce::Graphics& g, juce::Rectangle<int> bounds) {
  drawSectionHeader(g, bounds, "Edit", "SELECTED CLIP");
  g.setFont(OCC::Console::consoleFont(15.0f, juce::Font::bold));
  g.setColour(juce::Colour(OCC::Design::kTextPrimary));
  g.drawText("Right-click a clip or use the clip edit command to open the detailed editor.",
             bounds.removeFromTop(64), juce::Justification::topLeft, true);
  bounds.removeFromTop(10);
  drawRow(g, bounds.removeFromTop(38), juce::Colour(OCC::Design::kGroupBlue), "Name / File",
          "Dialog");
  bounds.removeFromTop(8);
  drawRow(g, bounds.removeFromTop(38), juce::Colour(OCC::Design::kGroupGreen), "Trim / Fades",
          "Ready");
  bounds.removeFromTop(8);
  drawRow(g, bounds.removeFromTop(38), juce::Colour(OCC::Design::kGroupOrange), "Group / Routing",
          "Ready");
}

void ConsoleInspectorPanel::drawRouting(juce::Graphics& g, juce::Rectangle<int> bounds) {
  drawSectionHeader(g, bounds, "Routing", "GROUP OUTPUTS");
  const juce::String groups[4] = {"Group 1", "Group 2", "Group 3", "Group 4"};
  const uint32_t colors[4] = {OCC::Design::kGroupBlue, OCC::Design::kGroupGreen,
                              OCC::Design::kGroupOrange, OCC::Design::kGroupRed};
  for (int i = 0; i < 4; ++i) {
    drawRow(g, bounds.removeFromTop(40), juce::Colour(colors[i]), groups[i],
            juce::String(m_snapshot.audio.groupLevels[static_cast<size_t>(i)] * 100.0f, 0) + "%");
    bounds.removeFromTop(8);
  }
}

void ConsoleInspectorPanel::drawPreferences(juce::Graphics& g, juce::Rectangle<int> bounds) {
  drawSectionHeader(g, bounds, "Preferences", "DISPLAY");
  drawRow(g, bounds.removeFromTop(40), juce::Colour(OCC::Design::kNeveBlue), "Default density",
          "8 x 6");
  bounds.removeFromTop(8);
  drawRow(g, bounds.removeFromTop(40), juce::Colour(OCC::Design::kAmber), "Live dense ceiling",
          "12 x 8");
  bounds.removeFromTop(8);
  drawRow(g, bounds.removeFromTop(40), juce::Colour(OCC::Design::kGroupGreen),
          "Logical slots / tab", "100");
}
