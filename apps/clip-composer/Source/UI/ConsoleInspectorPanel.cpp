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

//==============================================================================
// Playout

void ConsoleInspectorPanel::drawPlayout(juce::Graphics& g, juce::Rectangle<int> bounds) {
  // Reserve the footer for the Stop All + Cue Buss action buttons.
  constexpr int kFooterHeight = 36;
  constexpr int kFooterGap = 12;
  auto footer = bounds.removeFromBottom(kFooterHeight);
  bounds.removeFromBottom(kFooterGap);

  drawSectionHeader(g, bounds, "Playout", "NOW PLAYING");

  int rows = 0;
  for (const auto& clip : m_snapshot.session.clips) {
    if (!clip.hasClip || clip.playbackState == orpheus::PlaybackState::Stopped)
      continue;
    const auto row = bounds.removeFromTop(38);
    // Stripe is the group routing colour (per OCC149 contract: stripe = group, not swatch).
    const auto stripe = OCC::Console::groupColour(juce::jlimit(0, 3, clip.clipGroup));
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

  // Footer: Stop All (danger) + Cue Buss (default), equal flex.
  const int half = (footer.getWidth() - 8) / 2;
  auto stopAll = footer.removeFromLeft(half);
  footer.removeFromLeft(8);
  auto cueBuss = footer;
  OCC::Console::drawActionButton(g, stopAll.toFloat(), "Stop All",
                                 OCC::Console::ActionVariant::Danger);
  OCC::Console::drawActionButton(g, cueBuss.toFloat(), "Cue Buss",
                                 OCC::Console::ActionVariant::Default);
}

//==============================================================================
// Edit — full editor mirror is built by ClipEditDialog primitives in phase 4.
// For now this panel renders an instructional state with the same eyebrow.

void ConsoleInspectorPanel::drawEdit(juce::Graphics& g, juce::Rectangle<int> bounds) {
  drawSectionHeader(g, bounds, "Edit", "SELECTED CLIP");
  g.setFont(OCC::Console::consoleFont(13.0f, juce::Font::plain));
  g.setColour(juce::Colour(OCC::Design::kTextSecondary));
  g.drawText("Open a clip from the grid to edit its waveform, fades, group, and flags.",
             bounds.removeFromTop(48), juce::Justification::topLeft, true);
  bounds.removeFromTop(10);
  drawRow(g, bounds.removeFromTop(38), OCC::Console::groupColour(0), "Name / File", "Dialog");
  bounds.removeFromTop(8);
  drawRow(g, bounds.removeFromTop(38), OCC::Console::groupColour(1), "Trim / Fades", "Ready");
  bounds.removeFromTop(8);
  drawRow(g, bounds.removeFromTop(38), OCC::Console::groupColour(2), "Group / Routing", "Ready");
}

//==============================================================================
// Routing — 5-column table: GROUP | OUTPUT | GAIN | METER | M·S

void ConsoleInspectorPanel::drawRouting(juce::Graphics& g, juce::Rectangle<int> bounds) {
  drawSectionHeader(g, bounds, "Routing", "GROUP OUTPUTS");

  // Header row.
  {
    auto header = bounds.removeFromTop(20);
    g.setFont(OCC::Console::consoleFont(10.0f, juce::Font::bold));
    g.setColour(juce::Colour(OCC::Design::kTextSecondary));
    auto h = header;
    g.drawText("GROUP", h.removeFromLeft(80), juce::Justification::centredLeft, false);
    g.drawText("OUTPUT", h.removeFromLeft(90), juce::Justification::centredLeft, false);
    g.drawText("GAIN", h.removeFromLeft(56), juce::Justification::centredLeft, false);
    g.drawText("METER", h.removeFromLeft(96), juce::Justification::centredLeft, false);
    g.drawText("M·S", h, juce::Justification::centredLeft, false);
    bounds.removeFromTop(4);
    g.setColour(juce::Colour(OCC::Design::kBorderDefault));
    g.drawLine(static_cast<float>(bounds.getX()), static_cast<float>(bounds.getY()),
               static_cast<float>(bounds.getRight()), static_cast<float>(bounds.getY()), 1.0f);
    bounds.removeFromTop(6);
  }

  const juce::String outputs[4] = {"Out 1-2", "Out 3-4", "Out 5-6", "Out 7-8"};
  const float fakeGain[4] = {0.0f, -3.0f, -6.0f, -9.0f}; // placeholder until routing model wired

  for (int i = 0; i < 4; ++i) {
    auto row = bounds.removeFromTop(28);

    // GROUP column: 12x12 swatch + label.
    {
      auto col = row.removeFromLeft(80);
      const auto swatch = col.removeFromLeft(14).withSizeKeepingCentre(12, 12).toFloat();
      g.setColour(OCC::Console::groupColour(i));
      g.fillRoundedRectangle(swatch, 2.0f);
      g.setColour(juce::Colours::black.withAlpha(0.4f));
      g.drawRoundedRectangle(swatch, 2.0f, 1.0f);
      col.removeFromLeft(6);
      g.setFont(OCC::Console::consoleFont(12.0f, juce::Font::bold));
      g.setColour(juce::Colour(OCC::Design::kTextPrimary));
      g.drawText(juce::String("Group ") + OCC::Console::groupLabel(i), col,
                 juce::Justification::centredLeft, false);
    }

    // OUTPUT column.
    {
      auto col = row.removeFromLeft(90);
      g.setFont(OCC::Console::monoFont(11.0f));
      g.setColour(juce::Colour(OCC::Design::kTextSecondary));
      g.drawText(outputs[i], col, juce::Justification::centredLeft, false);
    }

    // GAIN column.
    {
      auto col = row.removeFromLeft(56);
      g.setFont(OCC::Console::monoFont(11.0f));
      g.setColour(juce::Colour(OCC::Design::kTextPrimary));
      g.drawText(juce::String(fakeGain[i], 1) + " dB", col, juce::Justification::centredLeft,
                 false);
    }

    // METER column: 90x10 bar with gradient fill clipped to level.
    {
      auto col = row.removeFromLeft(96);
      auto bar = col.removeFromLeft(90).withSizeKeepingCentre(90, 10).toFloat();
      g.setColour(juce::Colour(OCC::Design::kBgInset));
      g.fillRoundedRectangle(bar, 2.0f);
      g.setColour(juce::Colour(OCC::Design::kBorderDefault));
      g.drawRoundedRectangle(bar, 2.0f, 1.0f);

      const float level =
          juce::jlimit(0.0f, 1.0f, m_snapshot.audio.groupLevels[static_cast<size_t>(i)]);
      if (level > 0.0f) {
        auto fill = bar.reduced(1.0f);
        fill = fill.withWidth(fill.getWidth() * level);
        juce::ColourGradient grad{juce::Colour(OCC::Design::kMeterGreen),
                                  fill.getX(),
                                  fill.getY(),
                                  juce::Colour(OCC::Design::kMeterRed),
                                  fill.getRight(),
                                  fill.getY(),
                                  false};
        grad.addColour(0.60, juce::Colour(OCC::Design::kMeterGreen));
        grad.addColour(0.78, juce::Colour(OCC::Design::kMeterYellow));
        grad.addColour(0.92, juce::Colour(OCC::Design::kMeterOrange));
        g.setGradientFill(grad);
        g.fillRoundedRectangle(fill, 1.5f);
      }
    }

    // M·S column: two ghost buttons.
    {
      auto col = row;
      auto m = col.removeFromLeft(28);
      col.removeFromLeft(4);
      auto s = col.removeFromLeft(28);
      OCC::Console::drawActionButton(g, m.toFloat(), "M", OCC::Console::ActionVariant::Ghost);
      OCC::Console::drawActionButton(g, s.toFloat(), "S", OCC::Console::ActionVariant::Ghost);
    }

    bounds.removeFromTop(4);
  }
}

//==============================================================================
// Preferences — key/value list pulled from snapshot fields.

void ConsoleInspectorPanel::drawPreferences(juce::Graphics& g, juce::Rectangle<int> bounds) {
  drawSectionHeader(g, bounds, "Preferences", "DEVICE & I/O");

  struct KV {
    juce::String key;
    juce::String value;
  };

  const auto& dev = m_snapshot.audio.device;
  const auto& health = m_snapshot.audio.health;

  const juce::String deviceLabel =
      dev.deviceSummary.isNotEmpty()
          ? dev.deviceSummary
          : (dev.activeDeviceIdentifier.isNotEmpty() ? dev.activeDeviceIdentifier
                                                     : juce::String("(no device)"));
  const juce::String sampleRateLabel =
      health.sampleRate > 0 ? juce::String(health.sampleRate) + " Hz" : juce::String("--");
  const juce::String bufferSizeLabel =
      health.bufferSize > 0 ? juce::String(health.bufferSize) + " samples" : juce::String("--");
  const juce::String routeLabel =
      dev.playoutRouteLabel.isNotEmpty() ? dev.playoutRouteLabel : juce::String("(default)");
  const juce::String auditionLabel = m_snapshot.audio.audition.routeLabel.isNotEmpty()
                                         ? m_snapshot.audio.audition.routeLabel
                                         : juce::String("Shared with playout");
  const juce::String statusLabel =
      health.statusText.isNotEmpty() ? health.statusText : juce::String("OK");

  const KV rows[] = {
      {"Audio device", deviceLabel},     {"Sample rate", sampleRateLabel},
      {"Buffer size", bufferSizeLabel},  {"Playout route", routeLabel},
      {"Audition route", auditionLabel}, {"Status", statusLabel},
  };

  for (const auto& kv : rows) {
    auto row = bounds.removeFromTop(28);
    g.setFont(OCC::Console::consoleFont(12.0f, juce::Font::plain));
    g.setColour(juce::Colour(OCC::Design::kTextSecondary));
    g.drawText(kv.key, row.removeFromLeft(140), juce::Justification::centredLeft, false);
    g.setFont(OCC::Console::monoFont(11.0f));
    g.setColour(juce::Colour(OCC::Design::kTextPrimary));
    g.drawText(kv.value, row, juce::Justification::centredLeft, false);

    g.setColour(juce::Colour(OCC::Design::kBorderDefault).withAlpha(0.4f));
    g.drawLine(static_cast<float>(bounds.getX()), static_cast<float>(bounds.getY()),
               static_cast<float>(bounds.getRight()), static_cast<float>(bounds.getY()), 1.0f);
  }
}
