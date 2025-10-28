// SPDX-License-Identifier: MIT
// examples/simple_player/simple_player.cpp
//
// Simple Clip Player Example
// Demonstrates basic audio file playback using the Orpheus SDK

#include <chrono>
#include <iostream>
#include <orpheus/audio_driver.h>
#include <orpheus/audio_file_reader.h>
#include <orpheus/transport_controller.h>
#include <thread>

// Audio callback to connect driver to transport
class SimpleAudioCallback : public orpheus::IAudioCallback {
public:
  explicit SimpleAudioCallback(orpheus::ITransportController* transport) : transport_(transport) {}

  void processAudio(const float** /*inputs*/, float** outputs, size_t num_channels,
                    size_t num_frames) override {
    // Let transport fill the output buffers
    transport_->processAudio(outputs, static_cast<uint32_t>(num_channels),
                             static_cast<uint32_t>(num_frames));
  }

private:
  orpheus::ITransportController* transport_;
};

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <audio_file.wav>" << std::endl;
    return 1;
  }

  // 1. Open audio file
  auto reader = orpheus::createAudioFileReader();
  auto open_result = reader->open(argv[1]);

  if (!open_result.isOk()) {
    std::cerr << "Failed to open audio file: " << open_result.errorMessage << std::endl;
    return 1;
  }

  const auto& metadata = *open_result;
  std::cout << "\nLoaded: " << argv[1] << "\n";
  std::cout << "Duration: " << metadata.durationSeconds() << " seconds\n";
  std::cout << "Sample rate: " << metadata.sample_rate << " Hz\n";
  std::cout << "Channels: " << metadata.num_channels << "\n\n";

  // 2. Create transport controller
  orpheus::TransportConfig config;
  config.sample_rate = metadata.sample_rate;
  config.buffer_size = 512;
  config.num_outputs = 2;

  auto transport = orpheus::createTransportController(nullptr, config.sample_rate);

  if (transport->initialize(config) != orpheus::SessionGraphError::OK) {
    std::cerr << "Failed to initialize transport" << std::endl;
    return 1;
  }

  // 3. Register audio clip
  orpheus::ClipRegistration clip_reg;
  clip_reg.audio_file_path = argv[1];
  clip_reg.trim_in_samples = 0;
  clip_reg.trim_out_samples = metadata.duration_samples;

  auto clip_handle = transport->registerClipAudio(clip_reg);

  if (!clip_handle.isValid()) {
    std::cerr << "Failed to register audio clip" << std::endl;
    return 1;
  }

  // 4. Create and initialize audio driver
#ifdef __APPLE__
  auto driver = orpheus::createCoreAudioDriver();
#else
  auto driver = orpheus::createDummyAudioDriver();
  std::cout << "Note: Using dummy driver (no audio output on this platform)\n\n";
#endif

  orpheus::AudioDriverConfig driver_config;
  driver_config.sample_rate = config.sample_rate;
  driver_config.buffer_size = config.buffer_size;
  driver_config.num_outputs = 2;

  if (driver->initialize(driver_config) != orpheus::SessionGraphError::OK) {
    std::cerr << "Failed to initialize audio driver" << std::endl;
    return 1;
  }

  // 5. Set up audio callback
  SimpleAudioCallback callback(transport.get());

  if (driver->start(&callback) != orpheus::SessionGraphError::OK) {
    std::cerr << "Failed to start audio driver" << std::endl;
    return 1;
  }

  // 6. Start playback
  std::cout << "Playing..." << std::endl;

  if (transport->startClip(clip_handle, 0) != orpheus::SessionGraphError::OK) {
    std::cerr << "Failed to start clip playback" << std::endl;
    driver->stop();
    return 1;
  }

  // Wait for playback to finish (duration + 500ms buffer)
  std::this_thread::sleep_for(
      std::chrono::milliseconds(static_cast<int>(metadata.durationSeconds() * 1000 + 500)));

  // 7. Clean up
  driver->stop();
  std::cout << "Playback complete!\n" << std::endl;

  return 0;
}
