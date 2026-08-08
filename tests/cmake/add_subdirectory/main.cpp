// SPDX-License-Identifier: MIT
//
// ORP127 T10: submodule-consumption smoke program. Exercises the ORP127 public
// transport API surface (voice modes, choke, voice caps, SRC-neutral calls)
// enough to prove the headers compile and link cleanly from an external
// consumer that pulled the SDK in via add_subdirectory. No JUCE, no host glue.

#include <orpheus/audio_driver.h>

#include <orpheus/polyphase_resampler.h>
#include <orpheus/transport_controller.h>

#include <cstdint>
#include <memory>
#include <vector>

using namespace orpheus;

int main() {
  orpheus::AudioDriverConfig legacy{48000, 512, 0, {}, 2, {}, {}, {{}, {0, 1}}};
  if (legacy.sample_rate != 48000 || legacy.buffer_size != 512 || legacy.num_inputs != 0 ||
      !legacy.input_device_id.empty() || legacy.num_outputs != 2 ||
      !legacy.output_device_id.empty() || !legacy.device_name.empty() ||
      !legacy.channel_map.input_channels.empty() ||
      legacy.channel_map.output_channels != std::vector<uint16_t>{0, 1} ||
      legacy.sample_rate_policy != orpheus::AudioSampleRatePolicy::PreserveDeviceRate) {
    return 1;
  }

  // Transport is host-neutral: a null SessionGraph is fine for a link/compile
  // smoke test (we never register real audio here).
  auto transport = createTransportController(
      nullptr, TransportConfig{.sampleRate = static_cast<uint32_t>(48000)});
  if (!transport) {
    return 2;
  }

  // ORP127 G5: voice-mode API is reachable and typed as expected.
  VoiceMode modes[] = {VoiceMode::MonoWithFadeOverlap, VoiceMode::Polyphonic,
                       VoiceMode::MonoStrict};
  (void)modes;

  // ORP127 G7: choke + voice-cap primitives are reachable.
  transport->setMaxVoicesPerClip(4);
  if (transport->getMaxVoicesPerClip() != 4) {
    return 3;
  }
  transport->stopOtherClips(0);

  // ORP127 G6: the resampler is a standalone, dependency-free type.
  PolyphaseResampler rs(44100, 48000, 2);
  std::vector<float> in(256 * 2, 0.0f);
  std::vector<float> out;
  rs.process(in.data(), 256, out);

  // ClipMetadata carries the new voiceMode field.
  ClipMetadata meta;
  meta.voiceMode = VoiceMode::MonoStrict;
  (void)meta;

  return 0;
}
