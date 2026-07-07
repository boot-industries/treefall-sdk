// SPDX-License-Identifier: MIT
//
// ORP127 T10: submodule-consumption smoke program. Exercises the ORP127 public
// transport API surface (voice modes, choke, voice caps, SRC-neutral calls)
// enough to prove the headers compile and link cleanly from an external
// consumer that pulled the SDK in via add_subdirectory. No JUCE, no host glue.

#include <orpheus/polyphase_resampler.h>
#include <orpheus/transport_controller.h>

#include <cstdint>
#include <memory>
#include <vector>

using namespace orpheus;

int main() {
  // Transport is host-neutral: a null SessionGraph is fine for a link/compile
  // smoke test (we never register real audio here).
  auto transport = createTransportController(nullptr, 48000);
  if (!transport) {
    return 1;
  }

  // ORP127 G5: voice-mode API is reachable and typed as expected.
  VoiceMode modes[] = {VoiceMode::MonoWithFadeOverlap, VoiceMode::Polyphonic,
                       VoiceMode::MonoStrict};
  (void)modes;

  // ORP127 G7: choke + voice-cap primitives are reachable.
  transport->setMaxVoicesPerClip(4);
  if (transport->getMaxVoicesPerClip() != 4) {
    return 2;
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
