// SPDX-License-Identifier: MIT
// examples/simple_player/simple_player.cpp
//
// Simple Clip Player Example
// Demonstrates basic audio file playback using the Treefall SDK

#include <chrono>
#include <iostream>
#include <thread>
#include <treefall/audio_driver.h>
#include <treefall/audio_file_reader.h>
#include <treefall/transport_controller.h>

// Audio callback to connect driver to transport. This is the only audio-thread
// entry point; control work, including callback dispatch, stays on main.
class SimpleAudioCallback : public treefall::IAudioCallback {
public:
  explicit SimpleAudioCallback(treefall::ITransportController* transport) : transport_(transport) {}

  void processAudio(const treefall::AudioProcessBlock& block) noexcept override {
    transport_->processAudio(block.output_buffers, block.num_output_channels, block.num_frames);
  }

private:
  treefall::ITransportController* transport_;
};

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <audio_file.wav>" << std::endl;
    return 1;
  }

  auto reader = treefall::createAudioFileReader();
  if (!reader) {
    std::cerr << "Audio file reader is unavailable" << std::endl;
    return 1;
  }
  auto open_result = reader->open(argv[1]);
  if (!open_result.isOk()) {
    std::cerr << "Failed to open audio file: " << open_result.errorMessage << std::endl;
    return 1;
  }

  const auto& metadata = *open_result;
  std::cout << "\nLoaded: " << argv[1] << "\n"
            << "Duration: " << metadata.durationSeconds() << " seconds\n"
            << "Sample rate: " << metadata.sample_rate << " Hz\n"
            << "Channels: " << metadata.num_channels << "\n\n";

  treefall::TransportConfig config{
      .sampleRate = metadata.sample_rate, .outputChannels = 2, .maxBlockFrames = 512};
  auto transport = treefall::createTransportController(nullptr, config);
  if (!transport) {
    std::cerr << "Failed to create transport" << std::endl;
    return 1;
  }

  constexpr treefall::ClipHandle clip_handle = 1;
  if (transport->registerClipAudio(clip_handle, argv[1]) != treefall::SessionGraphError::OK ||
      transport->prepareClipAudio(clip_handle) != treefall::SessionGraphError::OK) {
    std::cerr << "Failed to register or prepare audio clip" << std::endl;
    return 1;
  }

#ifdef ORPHEUS_ENABLE_COREAUDIO
  auto driver = treefall::createCoreAudioDriver();
#else
  auto driver = treefall::createDummyAudioDriver();
  std::cout << "Note: Using dummy driver (no audio output on this platform)\n\n";
#endif
  if (!driver) {
    std::cerr << "Audio driver is unavailable" << std::endl;
    return 1;
  }

  treefall::AudioDriverConfig driver_config;
  driver_config.sample_rate = config.sampleRate;
  driver_config.buffer_size = static_cast<uint16_t>(config.maxBlockFrames);
  driver_config.num_inputs = 0;
  driver_config.num_outputs = static_cast<uint16_t>(config.outputChannels);
  if (driver->initialize(driver_config) != treefall::SessionGraphError::OK) {
    std::cerr << "Failed to initialize audio driver" << std::endl;
    return 1;
  }

  SimpleAudioCallback callback(transport.get());
  if (driver->start(&callback) != treefall::SessionGraphError::OK) {
    std::cerr << "Failed to start audio driver" << std::endl;
    return 1;
  }

  const auto start_result = transport->startClip(clip_handle, 0);
  if (start_result != treefall::SessionGraphError::OK) {
    std::cerr << "Failed to start clip playback" << std::endl;
    driver->stop();
    return 1;
  }
  std::cout << "Playing..." << std::endl;

  // Keep callback delivery on this control thread while audio runs. The
  // bounded pump interval also lets the clip complete without busy waiting.
  const auto deadline =
      std::chrono::steady_clock::now() +
      std::chrono::milliseconds(static_cast<int>(metadata.durationSeconds() * 1000 + 500));
  while (std::chrono::steady_clock::now() < deadline) {
    transport->processCallbacks();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  transport->processCallbacks();

  driver->stop();
  std::cout << "Playback complete!\n" << std::endl;
  return 0;
}
