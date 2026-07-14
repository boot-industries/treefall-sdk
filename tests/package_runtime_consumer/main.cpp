// SPDX-License-Identifier: MIT
#include <orpheus/transport_controller.h>

#include <cstddef>
#include <vector>

namespace {

class RecordingCallback final : public orpheus::ITransportCallback {
public:
  void onClipStarted(orpheus::ClipHandle handle, orpheus::TransportPosition) override {
    ++started;
    lastHandle = handle;
  }

  void onClipStopped(orpheus::ClipHandle, orpheus::TransportPosition) override {}
  void onClipLooped(orpheus::ClipHandle, orpheus::TransportPosition) override {}
  void onBufferUnderrun(orpheus::TransportPosition) override {}

  int started = 0;
  orpheus::ClipHandle lastHandle = 0;
};

} // namespace

int main() {
  constexpr uint32_t kSampleRate = 48000;
  constexpr orpheus::ClipHandle kHandle = 17;
  constexpr size_t kFrames = 64;

  auto transport = orpheus::createTransportController(nullptr, kSampleRate);
  if (!transport) {
    return 1;
  }

  const orpheus::TransportRenderConfig config = transport->getRenderConfig();
  if (config.sampleRate != kSampleRate || config.outputChannels != 2 ||
      config.maxBlockFrames < kFrames) {
    return 2;
  }

  std::vector<std::vector<float>> storage(
      config.outputChannels, std::vector<float>(kFrames, 1.0f));
  std::vector<float*> outputs;
  outputs.reserve(config.outputChannels);
  for (auto& channel : storage) {
    outputs.push_back(channel.data());
  }

  RecordingCallback callback;
  transport->setCallback(&callback);
  if (transport->startClip(kHandle) != orpheus::SessionGraphError::OK) {
    return 3;
  }

  transport->processAudio(outputs.data(), outputs.size(), kFrames);
  if (callback.started != 0) {
    return 4;
  }

  transport->processCallbacks();
  if (callback.started != 1 || callback.lastHandle != kHandle) {
    return 5;
  }

  for (const auto& channel : storage) {
    for (float sample : channel) {
      if (sample != 0.0f) {
        return 6;
      }
    }
  }

  transport->setCallback(nullptr);

  return 0;
}
