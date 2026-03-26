// SPDX-License-Identifier: MIT

#pragma once

#include <array>
#include <cstdint>
#include <orpheus/transport_controller.h>

namespace occ::ui {

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
  static constexpr int kButtonsPerTab = 48;

  int activeTab = 0;
  bool hasActivePlayback = false;
  std::array<ClipUiSnapshot, kButtonsPerTab> clips{};
};

struct AudioEngineUiSnapshot {
  float masterRmsLevel = 0.0f;
  std::array<float, 4> groupLevels{};
};

struct ClipComposerUiSnapshot {
  AudioEngineUiSnapshot audio;
  SessionUiSnapshot session;
};

struct PreviewPlaybackUiSnapshot {
  bool isPlaying = false;
  int64_t currentPositionSamples = 0;
};

} // namespace occ::ui
