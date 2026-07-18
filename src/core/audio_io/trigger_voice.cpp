// SPDX-License-Identifier: MIT
#include <orpheus/trigger_voice.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace orpheus {

void TriggerVoice::loadSample(const float* pcm, size_t numFrames, int channels, VoicePolicy policy,
                              int maxVoices) {
  if (pcm == nullptr || numFrames == 0 || channels <= 0 ||
      numFrames > std::numeric_limits<size_t>::max() / static_cast<size_t>(channels)) {
    m_sample.clear();
    m_voices.clear();
    m_numFrames = 0;
    m_channels = 0;
    m_policy = policy;
    m_nextSequence = 0;
    m_pendingTriggerCount = 0;
    return;
  }

  const size_t sampleCount = numFrames * static_cast<size_t>(channels);
  std::vector<float> sample(pcm, pcm + sampleCount);
  const size_t voiceCount =
      policy == VoicePolicy::RetriggerCut ? 1u : static_cast<size_t>(std::max(maxVoices, 1));
  std::vector<Voice> voices(voiceCount);

  m_sample = std::move(sample);
  m_voices = std::move(voices);
  m_numFrames = numFrames;
  m_channels = channels;
  m_policy = policy;
  m_nextSequence = 0;
  m_pendingTriggerCount = 0;
}

void TriggerVoice::trigger(size_t offsetInBuffer, float gain, float pitchRatio) noexcept {
  if (m_sample.empty() || m_voices.empty() || m_pendingTriggerCount == kMaxPendingTriggers ||
      !std::isfinite(gain) || !std::isfinite(pitchRatio) || pitchRatio <= 0.0F) {
    return;
  }

  TriggerEvent event{offsetInBuffer, gain, pitchRatio};
  size_t insertion = m_pendingTriggerCount;
  while (insertion > 0 && m_pendingTriggers[insertion - 1].offset > event.offset) {
    m_pendingTriggers[insertion] = m_pendingTriggers[insertion - 1];
    --insertion;
  }
  m_pendingTriggers[insertion] = event;
  ++m_pendingTriggerCount;
}

void TriggerVoice::applyTrigger(const TriggerEvent& event) noexcept {
  Voice* selected = nullptr;
  if (m_policy == VoicePolicy::RetriggerCut) {
    selected = &m_voices.front();
  } else {
    const auto freeVoice = std::find_if(m_voices.begin(), m_voices.end(),
                                        [](const Voice& voice) { return !voice.active; });
    if (freeVoice != m_voices.end()) {
      selected = &*freeVoice;
    } else {
      selected = &*std::min_element(
          m_voices.begin(), m_voices.end(),
          [](const Voice& lhs, const Voice& rhs) { return lhs.sequence < rhs.sequence; });
    }
  }

  selected->position = 0.0;
  selected->gain = event.gain;
  selected->pitchRatio = event.pitchRatio;
  selected->sequence = m_nextSequence++;
  selected->active = true;
}

void TriggerVoice::render(float* out, size_t numFrames) noexcept {
  if (out == nullptr || numFrames == 0 || m_sample.empty()) {
    return;
  }

  const size_t channels = static_cast<size_t>(m_channels);
  size_t nextTrigger = 0;
  for (size_t outputFrame = 0; outputFrame < numFrames; ++outputFrame) {
    while (nextTrigger < m_pendingTriggerCount &&
           m_pendingTriggers[nextTrigger].offset == outputFrame) {
      applyTrigger(m_pendingTriggers[nextTrigger]);
      ++nextTrigger;
    }

    for (Voice& voice : m_voices) {
      if (!voice.active) {
        continue;
      }

      const size_t sourceFrame = static_cast<size_t>(voice.position);
      if (sourceFrame >= m_numFrames) {
        voice.active = false;
        continue;
      }

      const double fraction = voice.position - static_cast<double>(sourceFrame);
      const size_t sourceBase = sourceFrame * channels;
      const bool hasNextFrame = sourceFrame + 1 < m_numFrames;
      const size_t nextBase = hasNextFrame ? sourceBase + channels : sourceBase;
      const size_t outputBase = outputFrame * channels;

      for (size_t channel = 0; channel < channels; ++channel) {
        const float first = m_sample[sourceBase + channel];
        const float second = hasNextFrame ? m_sample[nextBase + channel] : 0.0F;
        const float interpolated =
            first + static_cast<float>((static_cast<double>(second) - first) * fraction);
        out[outputBase + channel] += voice.gain * interpolated;
      }

      voice.position += static_cast<double>(voice.pitchRatio);
      if (voice.position >= static_cast<double>(m_numFrames)) {
        voice.active = false;
      }
    }
  }

  m_pendingTriggerCount = 0;
}

} // namespace orpheus
