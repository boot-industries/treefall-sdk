// SPDX-License-Identifier: MIT

#include "ConsoleInspectorPanel.h"
#include "ConsoleTheme.h"

ConsoleInspectorPanel::ConsoleInspectorPanel() {
  // Playout footer — Stop All (danger coral) + Cue Buss (matte default). Real juce::Buttons
  // so the live operator can mash them with the mouse or the keyboard. Wired through
  // onStopAll / onCueBuss callbacks installed by MainComponent.
  m_playoutStopAllButton = std::make_unique<ConsoleActionButton>(
      "inspector-stop-all", ConsoleActionButton::Variant::Danger);
  m_playoutStopAllButton->setButtonText("Stop All");
  m_playoutStopAllButton->onClick = [this]() {
    if (onStopAll)
      onStopAll();
  };
  addChildComponent(*m_playoutStopAllButton);

  m_playoutCueBussButton = std::make_unique<ConsoleActionButton>(
      "inspector-cue-buss", ConsoleActionButton::Variant::Default);
  m_playoutCueBussButton->setButtonText("Cue Buss");
  m_playoutCueBussButton->onClick = [this]() {
    if (onCueBuss)
      onCueBuss();
  };
  addChildComponent(*m_playoutCueBussButton);

  // Routing matrix mute/solo — one pair per group (A/B/C/D). Real buttons, no-op
  // dispatch until the routing model exposes mute/solo state.
  // TODO(occ149b-routing): wire onMutePressed / onSoloPressed to real routing model.
  for (int i = 0; i < 4; ++i) {
    auto mute = std::make_unique<ConsoleActionButton>("routing-mute-" + juce::String(i),
                                                      ConsoleActionButton::Variant::Ghost);
    mute->setButtonText("M");
    mute->onClick = [this, i]() {
      if (onMutePressed)
        onMutePressed(i);
    };
    addChildComponent(*mute);
    m_routingMuteButtons[i] = std::move(mute);

    auto solo = std::make_unique<ConsoleActionButton>("routing-solo-" + juce::String(i),
                                                      ConsoleActionButton::Variant::Ghost);
    solo->setButtonText("S");
    solo->onClick = [this, i]() {
      if (onSoloPressed)
        onSoloPressed(i);
    };
    addChildComponent(*solo);
    m_routingSoloButtons[i] = std::move(solo);
  }

  updateChildVisibility();
}

void ConsoleInspectorPanel::setSnapshot(const occ::ui::ClipComposerUiSnapshot& snapshot) {
  m_snapshot = snapshot;
  // OCC149c: reflect each group's mute/solo state on its M·S buttons. Engaged
  // mute reads as Danger (coral) to mirror Stop All; engaged solo reads as
  // Amber, matching the broadcast convention. Inactive = Ghost.
  for (int i = 0; i < 4; ++i) {
    const auto& routing = m_snapshot.audio.groupRouting[static_cast<size_t>(i)];
    if (auto& mute = m_routingMuteButtons[i])
      mute->setVariant(routing.muted ? ConsoleActionButton::Variant::Danger
                                     : ConsoleActionButton::Variant::Ghost);
    if (auto& solo = m_routingSoloButtons[i])
      solo->setVariant(routing.soloed ? ConsoleActionButton::Variant::Amber
                                      : ConsoleActionButton::Variant::Ghost);
  }
  repaint();
}

void ConsoleInspectorPanel::setOperatorViewMode(occ::ui::OperatorViewMode mode) {
  if (m_mode == mode)
    return;
  m_mode = mode;
  updateChildVisibility();
  resized();
  repaint();
}

void ConsoleInspectorPanel::updateChildVisibility() {
  const bool playoutMode = m_mode == occ::ui::OperatorViewMode::Playout;
  const bool routingMode = m_mode == occ::ui::OperatorViewMode::Routing;

  if (m_playoutStopAllButton)
    m_playoutStopAllButton->setVisible(playoutMode);
  if (m_playoutCueBussButton)
    m_playoutCueBussButton->setVisible(playoutMode);
  for (auto& b : m_routingMuteButtons)
    if (b)
      b->setVisible(routingMode);
  for (auto& b : m_routingSoloButtons)
    if (b)
      b->setVisible(routingMode);
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

void ConsoleInspectorPanel::resized() {
  // Children only need positioning when their mode is active. Other modes paint
  // statically so layout is a no-op.
  auto bounds = getLocalBounds().reduced(18, 16);
  switch (m_mode) {
  case occ::ui::OperatorViewMode::Playout: {
    constexpr int kFooterHeight = 36;
    auto footer = bounds.removeFromBottom(kFooterHeight);
    layoutPlayoutFooter(footer);
    break;
  }
  case occ::ui::OperatorViewMode::Routing: {
    layoutRoutingButtons(bounds);
    break;
  }
  case occ::ui::OperatorViewMode::Edit:
  case occ::ui::OperatorViewMode::Preferences:
  default:
    break;
  }
}

void ConsoleInspectorPanel::layoutPlayoutFooter(juce::Rectangle<int> footer) {
  if (!m_playoutStopAllButton || !m_playoutCueBussButton)
    return;
  const int half = (footer.getWidth() - 8) / 2;
  auto stopAll = footer.removeFromLeft(half);
  footer.removeFromLeft(8);
  m_playoutStopAllButton->setBounds(stopAll);
  m_playoutCueBussButton->setBounds(footer);
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
// Playout — live operator's "now playing" summary + Stop All / Cue Buss footer.

void ConsoleInspectorPanel::drawPlayout(juce::Graphics& g, juce::Rectangle<int> bounds) {
  // Reserve the footer (the real-button area, positioned in resized()).
  constexpr int kFooterHeight = 36;
  constexpr int kFooterGap = 12;
  bounds.removeFromBottom(kFooterHeight);
  bounds.removeFromBottom(kFooterGap);

  drawSectionHeader(g, bounds, "Playout", "NOW PLAYING");

  int rows = 0;
  for (const auto& clip : m_snapshot.session.clips) {
    if (!clip.hasClip || clip.playbackState == orpheus::PlaybackState::Stopped)
      continue;
    const auto row = bounds.removeFromTop(38);
    // Stripe is the group routing colour (per OCC149 contract: stripe = group).
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
}

//==============================================================================
// Edit — entry point to the modal dialog. The dialog itself carries the editor anatomy.

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
// Operator: engineer mapping the four groups onto outputs and trimming the bus.

namespace {

constexpr int kRoutingGroupColWidth = 80;
constexpr int kRoutingOutputColWidth = 90;
constexpr int kRoutingGainColWidth = 56;
constexpr int kRoutingMeterColWidth = 96;
constexpr int kRoutingHeaderHeight = 20;
constexpr int kRoutingHeaderGap = 10; // 4 px separator gap + 6 px before rows
constexpr int kRoutingRowHeight = 28;
constexpr int kRoutingRowGap = 4;
constexpr int kRoutingHeaderTotal =
    kRoutingHeaderHeight + kRoutingHeaderGap; // total height consumed by the header band

} // namespace

void ConsoleInspectorPanel::layoutRoutingButtons(const juce::Rectangle<int>& tableArea) {
  // Mirror the exact arithmetic used by drawRouting() so M/S buttons land on top of
  // the M·S column for each row. paint() and resized() must agree — visual collisions
  // are not acceptable.
  auto bounds = tableArea;
  // Section header in drawSectionHeader consumes: 18 (eyebrow) + 30 (title) + 10 (gap) = 58.
  bounds.removeFromTop(58);
  // Routing header band.
  bounds.removeFromTop(kRoutingHeaderTotal);

  for (int i = 0; i < 4; ++i) {
    auto row = bounds.removeFromTop(kRoutingRowHeight);
    // Walk through the columns the same way drawRouting does.
    row.removeFromLeft(kRoutingGroupColWidth);
    row.removeFromLeft(kRoutingOutputColWidth);
    row.removeFromLeft(kRoutingGainColWidth);
    row.removeFromLeft(kRoutingMeterColWidth);
    // What remains is the M·S column.
    constexpr int kMSButtonWidth = 28;
    constexpr int kMSButtonGap = 4;
    auto m = row.removeFromLeft(kMSButtonWidth);
    row.removeFromLeft(kMSButtonGap);
    auto s = row.removeFromLeft(kMSButtonWidth);
    if (m_routingMuteButtons[i])
      m_routingMuteButtons[i]->setBounds(m);
    if (m_routingSoloButtons[i])
      m_routingSoloButtons[i]->setBounds(s);
    bounds.removeFromTop(kRoutingRowGap);
  }
}

void ConsoleInspectorPanel::drawRouting(juce::Graphics& g, juce::Rectangle<int> bounds) {
  drawSectionHeader(g, bounds, "Routing", "GROUP OUTPUTS");

  // Header row.
  {
    auto header = bounds.removeFromTop(kRoutingHeaderHeight);
    g.setFont(OCC::Console::consoleFont(10.0f, juce::Font::bold));
    g.setColour(juce::Colour(OCC::Design::kTextSecondary));
    auto h = header;
    g.drawText("GROUP", h.removeFromLeft(kRoutingGroupColWidth), juce::Justification::centredLeft,
               false);
    g.drawText("OUTPUT", h.removeFromLeft(kRoutingOutputColWidth), juce::Justification::centredLeft,
               false);
    g.drawText("GAIN", h.removeFromLeft(kRoutingGainColWidth), juce::Justification::centredLeft,
               false);
    g.drawText("METER", h.removeFromLeft(kRoutingMeterColWidth), juce::Justification::centredLeft,
               false);
    g.drawText("M·S", h, juce::Justification::centredLeft, false);
    bounds.removeFromTop(4);
    g.setColour(juce::Colour(OCC::Design::kBorderDefault));
    g.drawLine(static_cast<float>(bounds.getX()), static_cast<float>(bounds.getY()),
               static_cast<float>(bounds.getRight()), static_cast<float>(bounds.getY()), 1.0f);
    bounds.removeFromTop(6);
  }

  // OCC149c: routing rows now read OUTPUT / GAIN / mute / solo from the audio
  // engine via the UI snapshot — see ClipComposerUiSnapshot::AudioEngineUiSnapshot.
  // The "—" placeholder only surfaces while the engine is still initializing.
  const juce::String kPlaceholder("—");

  for (int i = 0; i < 4; ++i) {
    auto row = bounds.removeFromTop(kRoutingRowHeight);
    const auto& routing = m_snapshot.audio.groupRouting[static_cast<size_t>(i)];

    // GROUP column: 12x12 swatch + label.
    {
      auto col = row.removeFromLeft(kRoutingGroupColWidth);
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
      auto col = row.removeFromLeft(kRoutingOutputColWidth);
      g.setFont(OCC::Console::monoFont(11.0f));
      g.setColour(juce::Colour(OCC::Design::kTextSecondary));
      const juce::String label =
          routing.outputLabel.isNotEmpty() ? routing.outputLabel : kPlaceholder;
      g.drawText(label, col, juce::Justification::centredLeft, false);
    }

    // GAIN column.
    {
      auto col = row.removeFromLeft(kRoutingGainColWidth);
      g.setFont(OCC::Console::monoFont(11.0f));
      g.setColour(juce::Colour(OCC::Design::kTextSecondary));
      const juce::String gainText =
          routing.gainDb == 0.0f ? juce::String("0.0 dB") : juce::String(routing.gainDb, 1) + " dB";
      g.drawText(gainText, col, juce::Justification::centredLeft, false);
    }

    // METER column: 90x10 bar with gradient fill clipped to live level.
    {
      auto col = row.removeFromLeft(kRoutingMeterColWidth);
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

    // M·S column: the real ConsoleActionButton instances live here. resized()
    // positions them; we just consume the same width so the row arithmetic
    // matches.
    {
      constexpr int kMSButtonWidth = 28;
      constexpr int kMSButtonGap = 4;
      row.removeFromLeft(kMSButtonWidth);
      row.removeFromLeft(kMSButtonGap);
      row.removeFromLeft(kMSButtonWidth);
    }

    bounds.removeFromTop(kRoutingRowGap);
  }
}

//==============================================================================
// Preferences — key/value summary pulled from the live audio snapshot.

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
      health.sampleRate > 0 ? juce::String(health.sampleRate) + " Hz" : juce::String("—");
  const juce::String bufferSizeLabel =
      health.bufferSize > 0 ? juce::String(health.bufferSize) + " samples" : juce::String("—");
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
