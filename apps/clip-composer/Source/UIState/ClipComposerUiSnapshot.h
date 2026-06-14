// SPDX-License-Identifier: MIT

#pragma once

#include <array>
#include <cstdint>
#include <juce_gui_basics/juce_gui_basics.h>
#include <orpheus/transport_controller.h>

#include "../Core/GridConstants.h"

namespace occ::ui {

enum class OperatorViewMode {
  Playout = 0,
  Edit,
  Routing,
  Preferences,
};

struct ClipUiSnapshot {
  int tabIndex = 0;
  int buttonIndex = -1;
  int globalClipIndex = -1;
  bool hasClip = false;
  orpheus::PlaybackState playbackState = orpheus::PlaybackState::Stopped;
  bool loopEnabled = false;
  bool fadeInEnabled = false;
  bool fadeOutEnabled = false;
  bool stopOthersEnabled = false;
  float playbackProgress = 0.0f;
  juce::String displayName;
  juce::Colour color{juce::Colours::transparentBlack};
  int clipGroup = 0;

  /// Compare snapshots for repaint gating (progress quantized to 1%)
  bool visuallyEquals(const ClipUiSnapshot& other) const {
    return hasClip == other.hasClip && playbackState == other.playbackState &&
           loopEnabled == other.loopEnabled && fadeInEnabled == other.fadeInEnabled &&
           fadeOutEnabled == other.fadeOutEnabled && stopOthersEnabled == other.stopOthersEnabled &&
           static_cast<int>(playbackProgress * 100.0f) ==
               static_cast<int>(other.playbackProgress * 100.0f);
  }
};

struct SessionUiSnapshot {
  static constexpr int kButtonsPerTab = occ::BUTTONS_PER_TAB;

  int activeTab = 0;
  bool hasActivePlayback = false;
  std::array<ClipUiSnapshot, kButtonsPerTab> clips{};
};

struct AudioEngineUiSnapshot {
  struct AuditionRouteConfig {
    bool usesDedicatedBus = false;
    juce::String routeLabel;
    juce::String validationMessage;
  };

  // OCC149c: PFL (pre-fader-listen) availability gating for the Cue / Cue Buss
  // operator buttons. PFL is a feature, not a default — it needs a >2ch
  // interface AND explicit cue-bus routing in Preferences. When unavailable,
  // the buttons render disabled with the reason text as a tooltip so the
  // operator sees why the affordance is dimmed instead of guessing.
  struct PflAvailability {
    bool available = false;
    juce::String unavailableReason;
  };

  struct DeviceRouteStatus {
    bool initialized = false;
    bool running = false;
    bool usingFallbackDriver = false;
    bool routesAvailable = true;
    juce::String activeDeviceIdentifier;
    juce::String playoutRouteLabel;
    juce::String deviceSummary;
  };

  struct HealthStripSnapshot {
    float cpuPercent = 0.0f;
    int memoryMB = 0;
    int bufferSize = 0;
    int sampleRate = 0;
    int dropoutCount = 0;
    juce::String statusText;
  };

  // OCC149c: routing inspector row data. Per-group output label, gain readout,
  // and M·S indicator state. Populated by MainComponent each poll from
  // AudioEngine getters; consumed by ConsoleInspectorPanel::drawRouting().
  struct GroupRoutingSnapshot {
    juce::String outputLabel;
    float gainDb = 0.0f;
    bool muted = false;
    bool soloed = false;
  };

  float masterRmsLevel = 0.0f;
  std::array<float, 4> groupLevels{};
  std::array<GroupRoutingSnapshot, 4> groupRouting{};
  bool routeAssignmentsAdjusted = false;
  juce::String activeDeviceIdentifier;
  DeviceRouteStatus device;
  AuditionRouteConfig audition;
  PflAvailability pfl;
  HealthStripSnapshot health;
};

struct ClipComposerUiSnapshot {
  OperatorViewMode activeViewMode = OperatorViewMode::Playout;
  AudioEngineUiSnapshot audio;
  SessionUiSnapshot session;
};

struct PreviewPlaybackUiSnapshot {
  bool isPlaying = false;
  int64_t currentPositionSamples = 0;
};

} // namespace occ::ui
