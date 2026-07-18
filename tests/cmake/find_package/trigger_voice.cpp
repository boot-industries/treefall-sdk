// SPDX-License-Identifier: MIT
#include <orpheus/trigger_voice.h>

#include <array>

int main() {
  orpheus::TriggerVoice voice;
  constexpr std::array<float, 3> sample{1.0F, 0.5F, 0.25F};
  std::array<float, 4> output{};

  voice.loadSample(sample.data(), sample.size(), 1, orpheus::VoicePolicy::Polyphonic, 4);
  voice.trigger(1, 0.5F, 1.0F);
  voice.render(output.data(), output.size());

  return output[0] == 0.0F && output[1] == 0.5F && output[2] == 0.25F ? 0 : 1;
}
