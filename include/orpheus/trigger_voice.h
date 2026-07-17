// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace orpheus {

/// Voice allocation policy for a pre-loaded one-shot sample.
enum class VoicePolicy : uint8_t {
  /// A trigger immediately replaces the currently sounding instance.
  RetriggerCut = 0,
  /// Triggers layer up to maxVoices, then replace the oldest instance.
  Polyphonic = 1,
};

/// Real-time sample-trigger contract for a bounded, pre-allocated voice pool.
class ITriggerVoice {
public:
  virtual ~ITriggerVoice() = default;

  /// Copy interleaved PCM and prepare the complete voice pool off the audio thread.
  ///
  /// Loading must not race trigger() or render(). Invalid input unloads the
  /// current sample. RetriggerCut always prepares one voice; Polyphonic prepares
  /// maxVoices voices (at least one).
  virtual void loadSample(const float* pcm, size_t numFrames, int channels, VoicePolicy policy,
                          int maxVoices) = 0;

  /// Schedule a trigger in the current callback buffer without allocation or locking.
  ///
  /// offsetInBuffer must be less than the numFrames supplied to the following
  /// render() call. pitchRatio must be finite and greater than zero.
  virtual void trigger(size_t offsetInBuffer, float gain, float pitchRatio = 1.0F) noexcept = 0;

  /// Mix active voices into interleaved output without clearing it.
  ///
  /// The output contains numFrames times the loaded sample's channel count.
  /// Its interleaved layout must match that count; wider hosts must up-mix
  /// separately. Triggers for this callback must be submitted before render().
  virtual void render(float* out, size_t numFrames) noexcept = 0;
};

/// Dependency-free, allocation-free-on-render implementation of ITriggerVoice.
/// Each instance owns one interleaved PCM sample, an independent voice pool,
/// and a fixed queue for up to kMaxPendingTriggers events per render call.
/// Excess triggers are ignored. Sample playback uses linear interpolation for
/// arbitrary positive pitch ratios.
class TriggerVoice final : public ITriggerVoice {
public:
  static constexpr size_t kMaxPendingTriggers = 64;
  TriggerVoice() = default;

  void loadSample(const float* pcm, size_t numFrames, int channels, VoicePolicy policy,
                  int maxVoices) override;
  void trigger(size_t offsetInBuffer, float gain, float pitchRatio = 1.0F) noexcept override;
  void render(float* out, size_t numFrames) noexcept override;

  [[nodiscard]] size_t loadedFrames() const noexcept {
    return m_numFrames;
  }
  [[nodiscard]] int loadedChannels() const noexcept {
    return m_channels;
  }
  [[nodiscard]] size_t voiceCapacity() const noexcept {
    return m_voices.size();
  }

private:
  struct Voice {
    double position{0.0};
    float gain{0.0F};
    float pitchRatio{1.0F};
    uint64_t sequence{0};
    bool active{false};
  };

  struct TriggerEvent {
    size_t offset{0};
    float gain{0.0F};
    float pitchRatio{1.0F};
  };

  void applyTrigger(const TriggerEvent& event) noexcept;

  std::vector<float> m_sample;
  std::vector<Voice> m_voices;
  std::array<TriggerEvent, kMaxPendingTriggers> m_pendingTriggers{};
  size_t m_pendingTriggerCount{0};
  size_t m_numFrames{0};
  int m_channels{0};
  VoicePolicy m_policy{VoicePolicy::RetriggerCut};
  uint64_t m_nextSequence{0};
};

} // namespace orpheus
