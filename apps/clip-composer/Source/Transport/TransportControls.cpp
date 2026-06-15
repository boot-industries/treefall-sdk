// SPDX-License-Identifier: MIT

#include "TransportControls.h"
#include "../UI/ConsoleTheme.h"
#include "../UI/DesignTokens.h"

namespace {

// Compare the transport-strip-visible subset of two snapshots. Returns true
// when nothing painted has changed and a repaint can be skipped. Continuous
// fields are quantized to render precision (~1% / per-dB).
bool transportSnapshotEqual(const occ::ui::ClipComposerUiSnapshot& a,
                            const occ::ui::ClipComposerUiSnapshot& b) {
  // Master meter — quantize to 1% so per-sample drift doesn't trip the gate.
  if (static_cast<int>(a.audio.masterRmsLevel * 100.0f) !=
      static_cast<int>(b.audio.masterRmsLevel * 100.0f))
    return false;
  // PFL availability — drives the Cue button enabled state + tooltip.
  if (a.audio.pfl.available != b.audio.pfl.available ||
      a.audio.pfl.unavailableReason != b.audio.pfl.unavailableReason)
    return false;
  // Playing count + first two display names — that's what the status zone shows.
  int aPlaying = 0;
  int bPlaying = 0;
  juce::StringArray aNames;
  juce::StringArray bNames;
  for (const auto& clip : a.session.clips) {
    if (clip.hasClip && clip.playbackState != orpheus::PlaybackState::Stopped) {
      ++aPlaying;
      if (aNames.size() < 2 && clip.displayName.isNotEmpty())
        aNames.add(clip.displayName);
    }
  }
  for (const auto& clip : b.session.clips) {
    if (clip.hasClip && clip.playbackState != orpheus::PlaybackState::Stopped) {
      ++bPlaying;
      if (bNames.size() < 2 && clip.displayName.isNotEmpty())
        bNames.add(clip.displayName);
    }
  }
  if (aPlaying != bPlaying || aNames != bNames)
    return false;
  return true;
}

} // namespace

//==============================================================================
TransportControls::TransportControls() {
  // paint() always covers the bounds with fillVerticalGradient, so promise
  // JUCE the strip is opaque. Suppresses parent recomposition on every
  // snapshot push from MainComponent.
  setOpaque(true);
  // Create Stop All button
  m_stopAllButton = std::make_unique<juce::TextButton>("Stop All");
  m_stopAllButton->setButtonText("STOP ALL");
  m_stopAllButton->setColour(juce::TextButton::buttonColourId,
                             juce::Colour(OCC::Design::kMeterRed));
  m_stopAllButton->setColour(juce::TextButton::textColourOffId,
                             juce::Colour(OCC::Design::kTextPrimary));
  m_stopAllButton->onClick = [this]() {
    if (onStopAll)
      onStopAll();
  };
  addAndMakeVisible(m_stopAllButton.get());

  // Create Panic button (red, emergency stop)
  m_panicButton = std::make_unique<juce::TextButton>("Panic");
  m_panicButton->setButtonText("PANIC");
  m_panicButton->onClick = [this]() {
    if (onPanic)
      onPanic();
  };
  m_panicButton->setColour(juce::TextButton::buttonColourId, juce::Colour(OCC::Design::kAmber));
  m_panicButton->setColour(juce::TextButton::textColourOffId, juce::Colours::black);
  addAndMakeVisible(m_panicButton.get());

  // Cue button (ghost variant — matte grey, far right).
  m_cueButton = std::make_unique<juce::TextButton>("Cue");
  m_cueButton->setButtonText("CUE");
  m_cueButton->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff383d40));
  m_cueButton->setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff45494c));
  m_cueButton->setColour(juce::TextButton::textColourOffId,
                         juce::Colour(OCC::Design::kTextSecondary));
  m_cueButton->onClick = [this]() {
    if (onCue)
      onCue();
  };
  addAndMakeVisible(m_cueButton.get());

  // Create latency label
  m_latencyLabel = std::make_unique<juce::Label>("Latency", "Latency: -- ms");
  m_latencyLabel->setFont(juce::FontOptions(12.0f, juce::Font::plain));
  m_latencyLabel->setColour(juce::Label::textColourId, juce::Colour(OCC::Design::kTextSecondary));
  m_latencyLabel->setJustificationType(juce::Justification::centredLeft);
  addAndMakeVisible(m_latencyLabel.get());

  // OCC109 v0.2.2: Create CPU usage label
  m_cpuLabel = std::make_unique<juce::Label>("CPU", "CPU: --");
  m_cpuLabel->setFont(juce::FontOptions(12.0f, juce::Font::plain));
  m_cpuLabel->setColour(juce::Label::textColourId, juce::Colour(OCC::Design::kTextSecondary));
  m_cpuLabel->setJustificationType(juce::Justification::centredLeft);
  addAndMakeVisible(m_cpuLabel.get());

  // OCC109 v0.2.2: Create memory usage label
  m_memoryLabel = std::make_unique<juce::Label>("Memory", "MEM: --");
  m_memoryLabel->setFont(juce::FontOptions(12.0f, juce::Font::plain));
  m_memoryLabel->setColour(juce::Label::textColourId, juce::Colour(OCC::Design::kTextSecondary));
  m_memoryLabel->setJustificationType(juce::Justification::centredLeft);
  addAndMakeVisible(m_memoryLabel.get());
}

//==============================================================================
// Shared layout — paint() and resized() both consume this so they can never
// disagree. Surfaces collapse in priority order when the window is too narrow:
//   1. diagnostics labels (latency / CPU / memory) collapse first — they're
//      reachable elsewhere in the app.
//   2. PLAYING status zone collapses next — the inspector also shows it.
//   3. Master cluster shrinks (meter narrows) but stays visible — it's the
//      operator's only on-strip level read.
//   4. The two destructive buttons (Stop All, Panic) and CUE always render.
TransportControls::Layout TransportControls::computeLayout() const {
  Layout L;
  auto bounds = getLocalBounds().reduced(10);
  if (bounds.isEmpty())
    return L;

  constexpr int kStopWidth = 136;
  constexpr int kPanicWidth = 116;
  constexpr int kButtonHeight = 36;
  constexpr int kGap = 10;
  constexpr int kClusterGap = 18;
  constexpr int kCueWidth = 60;
  constexpr int kCueHeight = 28;
  constexpr int kLatencyWidth = 160;
  constexpr int kCpuWidth = 90;
  constexpr int kMemoryWidth = 100;
  constexpr int kDiagnosticsTotal = kLatencyWidth + kCpuWidth + kMemoryWidth + kClusterGap;
  constexpr int kStatusMinWidth = 200; // PLAYING · N + 2 clip names need at least this.
  constexpr int kMasterIdealWidth = 320;
  constexpr int kMasterMinWidth = 200; // MASTER label + 90 px bar + readout collapses to this.

  // Fixed left cluster: Stop All + Panic.
  L.stopAll = bounds.removeFromLeft(kStopWidth).withSizeKeepingCentre(kStopWidth, kButtonHeight);
  bounds.removeFromLeft(kGap);
  L.panic = bounds.removeFromLeft(kPanicWidth).withSizeKeepingCentre(kPanicWidth, kButtonHeight);
  bounds.removeFromLeft(kClusterGap);

  // Fixed right anchor: CUE button.
  L.cueButton = bounds.removeFromRight(kCueWidth).withSizeKeepingCentre(kCueWidth, kCueHeight);
  bounds.removeFromRight(kGap);

  // Decide what fits in the middle. Available middle width:
  const int middleWidth = bounds.getWidth();

  // Always want master cluster — at least at min width.
  int masterWidth = juce::jmin(kMasterIdealWidth, juce::jmax(kMasterMinWidth, middleWidth));
  // Don't let the master cluster eat the whole middle if diagnostics can fit.
  if (middleWidth >= kDiagnosticsTotal + kMasterMinWidth + kStatusMinWidth) {
    // Comfortable: diagnostics + status + master all visible.
    L.diagnosticsVisible = true;
    L.statusVisible = true;
    L.masterVisible = true;
    masterWidth = kMasterIdealWidth;
  } else if (middleWidth >= kDiagnosticsTotal + kMasterMinWidth) {
    // Drop status — diagnostics + master.
    L.diagnosticsVisible = true;
    L.statusVisible = false;
    L.masterVisible = true;
  } else if (middleWidth >= kStatusMinWidth + kMasterMinWidth) {
    // Drop diagnostics — status + master.
    L.diagnosticsVisible = false;
    L.statusVisible = true;
    L.masterVisible = true;
  } else if (middleWidth >= kMasterMinWidth) {
    // Only master fits.
    L.diagnosticsVisible = false;
    L.statusVisible = false;
    L.masterVisible = true;
  } else {
    // Truly tight — collapse everything in the middle.
    L.diagnosticsVisible = false;
    L.statusVisible = false;
    L.masterVisible = false;
  }

  if (L.masterVisible)
    L.masterCluster = bounds.removeFromRight(masterWidth);

  if (L.diagnosticsVisible) {
    L.latencyLabel =
        bounds.removeFromLeft(kLatencyWidth).withSizeKeepingCentre(kLatencyWidth, kButtonHeight);
    L.cpuLabel = bounds.removeFromLeft(kCpuWidth).withSizeKeepingCentre(kCpuWidth, kButtonHeight);
    L.memoryLabel =
        bounds.removeFromLeft(kMemoryWidth).withSizeKeepingCentre(kMemoryWidth, kButtonHeight);
    bounds.removeFromLeft(kClusterGap);
  }
  if (L.statusVisible)
    L.statusZone = bounds;
  return L;
}

void TransportControls::paint(juce::Graphics& g) {
  auto bounds = getLocalBounds().toFloat();
  OCC::Console::fillVerticalGradient(g, bounds, juce::Colour(OCC::Design::kBgSecondary),
                                     juce::Colour(OCC::Design::kBgPrimary));
  g.setColour(juce::Colour(OCC::Design::kBorderDefault));
  g.drawLine(0.0f, 0.0f, static_cast<float>(getWidth()), 0.0f, 1.0f);

  const Layout L = computeLayout();

  // PLAYING status zone (count + clip names + green dot indicator).
  if (L.statusVisible && !L.statusZone.isEmpty()) {
    auto status = L.statusZone;
    // Separator hairline at the left edge of the status zone.
    g.setColour(juce::Colour(OCC::Design::kBorderDefault));
    g.drawLine(static_cast<float>(status.getX()), 12.0f, static_cast<float>(status.getX()),
               static_cast<float>(getHeight() - 12), 1.0f);
    status.removeFromLeft(12); // padding past the hairline

    int playing = 0;
    juce::StringArray names;
    for (const auto& clip : m_snapshot.session.clips) {
      if (clip.hasClip && clip.playbackState != orpheus::PlaybackState::Stopped) {
        ++playing;
        if (names.size() < 2 && clip.displayName.isNotEmpty())
          names.add(clip.displayName);
      }
    }

    g.setColour(playing > 0 ? juce::Colour(0xff5af070) : juce::Colour(OCC::Design::kTextMuted));
    g.fillEllipse(static_cast<float>(status.getX()), static_cast<float>(getHeight() / 2 - 4), 8.0f,
                  8.0f);
    g.setFont(OCC::Console::monoFont(10.5f));
    g.setColour(juce::Colour(OCC::Design::kTextSecondary));
    g.drawText("PLAYING  .  " + juce::String(playing), status.withTrimmedLeft(18),
               juce::Justification::centredLeft, false);
    if (!names.isEmpty()) {
      g.setFont(OCC::Console::consoleFont(14.0f, juce::Font::bold));
      g.setColour(juce::Colour(OCC::Design::kTextPrimary));
      g.drawText(names.joinIntoString("   |   "), status.withTrimmedLeft(128),
                 juce::Justification::centredLeft, true);
    }
  }

  // Master cluster: MASTER label + meter bar + dB readout (always painted when visible).
  if (L.masterVisible && !L.masterCluster.isEmpty()) {
    auto meterArea = L.masterCluster;
    g.setFont(OCC::Console::monoFont(11.0f, juce::Font::plain));
    g.setColour(juce::Colour(OCC::Design::kTextSecondary));
    g.drawText("MASTER", meterArea.removeFromLeft(64), juce::Justification::centredRight, false);
    meterArea.removeFromLeft(10);

    // Reserve dB readout (70 px) on the right.
    auto readoutArea = meterArea.removeFromRight(70);
    meterArea.removeFromRight(10);

    // Meter bar gets the leftover middle.
    auto meter = meterArea.withSizeKeepingCentre(meterArea.getWidth(), 12).toFloat();
    g.setColour(juce::Colour(OCC::Design::kBgInset));
    g.fillRoundedRectangle(meter, 2.0f);
    g.setColour(juce::Colour(OCC::Design::kBorderDefault));
    g.drawRoundedRectangle(meter, 2.0f, 1.0f);

    const float level = juce::jlimit(0.0f, 1.0f, m_snapshot.audio.masterRmsLevel);
    if (level > 0.0f) {
      auto fill = meter.reduced(1.0f);
      fill = fill.withWidth(fill.getWidth() * level);
      juce::ColourGradient grad(juce::Colour(OCC::Design::kMeterGreen), fill.getX(), fill.getY(),
                                juce::Colour(OCC::Design::kMeterRed), meter.getRight(), fill.getY(),
                                false);
      grad.addColour(0.60, juce::Colour(OCC::Design::kMeterGreen));
      grad.addColour(0.78, juce::Colour(OCC::Design::kMeterYellow));
      grad.addColour(0.92, juce::Colour(OCC::Design::kMeterOrange));
      g.setGradientFill(grad);
      g.fillRoundedRectangle(fill, 2.0f);
    }

    juce::String dbText;
    if (level > 0.0001f) {
      const float db = juce::jlimit(-60.0f, 6.0f, 20.0f * std::log10(level));
      dbText = juce::String::formatted("%+.1f dB", static_cast<double>(db));
    } else {
      dbText = "-inf";
    }
    g.setFont(OCC::Console::monoFont(11.0f, juce::Font::plain));
    g.setColour(level > 0.0001f ? juce::Colour(OCC::Design::kAmber)
                                : juce::Colour(OCC::Design::kTextMuted));
    g.drawText(dbText, readoutArea, juce::Justification::centredLeft, false);
  }
}

void TransportControls::resized() {
  const Layout L = computeLayout();
  if (m_stopAllButton)
    m_stopAllButton->setBounds(L.stopAll);
  if (m_panicButton)
    m_panicButton->setBounds(L.panic);
  if (m_cueButton)
    m_cueButton->setBounds(L.cueButton);

  // Diagnostic labels: position only when layout decided they fit. When collapsed,
  // setVisible(false) hides them outright so no stray paint lands on top of the master.
  if (m_latencyLabel) {
    m_latencyLabel->setVisible(L.diagnosticsVisible);
    if (L.diagnosticsVisible)
      m_latencyLabel->setBounds(L.latencyLabel);
  }
  if (m_cpuLabel) {
    m_cpuLabel->setVisible(L.diagnosticsVisible);
    if (L.diagnosticsVisible)
      m_cpuLabel->setBounds(L.cpuLabel);
  }
  if (m_memoryLabel) {
    m_memoryLabel->setVisible(L.diagnosticsVisible);
    if (L.diagnosticsVisible)
      m_memoryLabel->setBounds(L.memoryLabel);
  }
}

void TransportControls::setLatencyInfo(double latencyMs, int bufferSize, int sampleRate) {
  juce::String text =
      juce::String::formatted("Latency: %.1f ms (%d @ %dHz)", latencyMs, bufferSize, sampleRate);

  // Color-code for user feedback (green < 10ms, orange < 20ms, red >= 20ms)
  juce::Colour color;
  if (latencyMs < 10.0) {
    color = juce::Colour(OCC::Design::kMeterGreen);
  } else if (latencyMs < 20.0) {
    color = juce::Colour(OCC::Design::kMeterYellow);
  } else {
    color = juce::Colour(OCC::Design::kMeterRed);
  }

  m_latencyLabel->setText(text, juce::dontSendNotification);
  m_latencyLabel->setColour(juce::Label::textColourId, color);
}

void TransportControls::setTransportSnapshot(const occ::ui::ClipComposerUiSnapshot& snapshot) {
  // Gate the strip repaint on whether anything painted actually changed. The
  // Cue button handles its own invalidation when setEnabled / setTooltip
  // flip, so we still apply those updates unconditionally.
  const bool needsRepaint = !transportSnapshotEqual(m_snapshot, snapshot);
  m_snapshot = snapshot;
  // OCC149c: PFL gate — Cue is an opt-in feature. Disable the transport-strip
  // Cue button + show the reason as a tooltip when the device/routing
  // prerequisites aren't met.
  if (m_cueButton) {
    const auto& pfl = m_snapshot.audio.pfl;
    m_cueButton->setEnabled(pfl.available);
    m_cueButton->setTooltip(pfl.available ? juce::String() : pfl.unavailableReason);
  }
  if (needsRepaint)
    repaint();
}

void TransportControls::setPerformanceInfo(float cpuPercent, int memoryMB) {
  // OCC109 v0.2.2: Update CPU usage display
  juce::String cpuText = juce::String::formatted("CPU: %.0f%%", cpuPercent);
  m_cpuLabel->setText(cpuText, juce::dontSendNotification);

  // Color-code CPU usage (green < 50%, orange < 80%, red >= 80%)
  juce::Colour cpuColor;
  if (cpuPercent < 50.0f) {
    cpuColor = juce::Colour(OCC::Design::kMeterGreen);
  } else if (cpuPercent < 80.0f) {
    cpuColor = juce::Colour(OCC::Design::kMeterYellow);
  } else {
    cpuColor = juce::Colour(OCC::Design::kMeterRed);
  }
  m_cpuLabel->setColour(juce::Label::textColourId, cpuColor);

  // OCC109 v0.2.2: Update memory usage display
  juce::String memoryText = juce::String::formatted("MEM: %d MB", memoryMB);
  m_memoryLabel->setText(memoryText, juce::dontSendNotification);

  // Color-code memory usage (green < 200MB, orange < 500MB, red >= 500MB)
  juce::Colour memColor;
  if (memoryMB < 200) {
    memColor = juce::Colour(OCC::Design::kTextSecondary);
  } else if (memoryMB < 500) {
    memColor = juce::Colour(OCC::Design::kMeterYellow);
  } else {
    memColor = juce::Colour(OCC::Design::kMeterRed);
  }
  m_memoryLabel->setColour(juce::Label::textColourId, memColor);
}
