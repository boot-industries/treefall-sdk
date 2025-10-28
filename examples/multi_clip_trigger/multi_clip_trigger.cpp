// SPDX-License-Identifier: MIT
// examples/multi_clip_trigger/multi_clip_trigger.cpp
//
// Multi-Clip Trigger Example
// Demonstrates soundboard-style playback with multiple clips

#include <atomic>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <orpheus/audio_driver.h>
#include <orpheus/audio_file_reader.h>
#include <orpheus/transport_controller.h>
#include <string>
#include <thread>
#include <vector>

// Audio callback to connect driver to transport
class MultiClipAudioCallback : public orpheus::IAudioCallback {
public:
  explicit MultiClipAudioCallback(orpheus::ITransportController* transport)
      : transport_(transport) {}

  void processAudio(const float** /*inputs*/, float** outputs, size_t num_channels,
                    size_t num_frames) override {
    transport_->processAudio(outputs, static_cast<uint32_t>(num_channels),
                             static_cast<uint32_t>(num_frames));
  }

private:
  orpheus::ITransportController* transport_;
};

// Clip metadata for user interaction
struct ClipInfo {
  orpheus::ClipHandle handle;
  std::string name;
  std::string file_path;
  double duration_seconds;
  int key; // Keyboard shortcut (1-9)
};

// Print usage instructions
void printUsage(const char* program_name) {
  std::cout << "\nMulti-Clip Trigger Example\n";
  std::cout << "===========================\n\n";
  std::cout << "Usage: " << program_name << " <audio_file1.wav> <audio_file2.wav> ...\n\n";
  std::cout << "Controls:\n";
  std::cout << "  1-9  : Trigger clips 1-9\n";
  std::cout << "  s    : Stop all clips\n";
  std::cout << "  q    : Quit application\n";
  std::cout << "  h    : Show this help\n\n";
}

// Print clip status
void printClipStatus(const std::vector<ClipInfo>& clips) {
  std::cout << "\nLoaded " << clips.size() << " clips:\n";
  std::cout << "---------------------\n";
  for (size_t i = 0; i < clips.size(); ++i) {
    std::cout << "  [" << (i + 1) << "] " << clips[i].name << " (" << clips[i].duration_seconds
              << "s)\n";
  }
  std::cout << "\nPress 1-" << clips.size()
            << " to trigger clips, 's' to stop all, 'q' to quit\n\n";
}

int main(int argc, char** argv) {
  if (argc < 2) {
    printUsage(argv[0]);
    return 1;
  }

  std::vector<ClipInfo> clips;
  uint32_t max_sample_rate = 48000;

  // 1. Load all audio files and find maximum sample rate
  std::cout << "\nLoading audio files...\n";
  for (int i = 1; i < argc && i <= 9; ++i) {
    auto reader = orpheus::createAudioFileReader();
    auto result = reader->open(argv[i]);

    if (!result.isOk()) {
      std::cerr << "Warning: Failed to load " << argv[i] << ": " << result.errorMessage << "\n";
      continue;
    }

    const auto& metadata = *result;

    // Track maximum sample rate for transport config
    if (metadata.sample_rate > max_sample_rate) {
      max_sample_rate = metadata.sample_rate;
    }

    // Extract filename from path
    std::string file_path = argv[i];
    size_t last_slash = file_path.find_last_of("/\\");
    std::string filename =
        (last_slash != std::string::npos) ? file_path.substr(last_slash + 1) : file_path;

    ClipInfo info;
    info.name = filename;
    info.file_path = file_path;
    info.duration_seconds = metadata.durationSeconds();
    info.key = i;

    clips.push_back(info);

    std::cout << "  [" << i << "] Loaded: " << filename << " (" << metadata.durationSeconds()
              << "s, " << metadata.sample_rate << "Hz, " << metadata.num_channels << "ch)\n";
  }

  if (clips.empty()) {
    std::cerr << "Error: No valid audio files loaded\n";
    return 1;
  }

  // 2. Create transport controller
  orpheus::TransportConfig config;
  config.sample_rate = max_sample_rate;
  config.buffer_size = 512;
  config.num_outputs = 2;

  auto transport = orpheus::createTransportController(nullptr, config.sample_rate);

  if (transport->initialize(config) != orpheus::SessionGraphError::OK) {
    std::cerr << "Failed to initialize transport" << std::endl;
    return 1;
  }

  // 3. Register all clips with transport
  std::cout << "\nRegistering clips with transport...\n";
  for (auto& clip : clips) {
    auto reader = orpheus::createAudioFileReader();
    auto result = reader->open(clip.file_path);

    if (!result.isOk()) {
      continue; // Already validated in loading phase
    }

    const auto& metadata = *result;

    orpheus::ClipRegistration clip_reg;
    clip_reg.audio_file_path = clip.file_path;
    clip_reg.trim_in_samples = 0;
    clip_reg.trim_out_samples = metadata.duration_samples;

    clip.handle = transport->registerClipAudio(clip_reg);

    if (!clip.handle.isValid()) {
      std::cerr << "Warning: Failed to register " << clip.name << "\n";
    }
  }

  // 4. Create and initialize audio driver
#ifdef __APPLE__
  auto driver = orpheus::createCoreAudioDriver();
#else
  auto driver = orpheus::createDummyAudioDriver();
  std::cout << "\nNote: Using dummy driver (no audio output on this platform)\n";
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
  MultiClipAudioCallback callback(transport.get());

  if (driver->start(&callback) != orpheus::SessionGraphError::OK) {
    std::cerr << "Failed to start audio driver" << std::endl;
    return 1;
  }

  // 6. Interactive control loop
  printClipStatus(clips);

  std::atomic<bool> running{true};

  while (running) {
    // Read single character input (non-blocking would be better, but this is simple)
    std::cout << "> ";
    std::string input;
    std::getline(std::cin, input);

    if (input.empty()) {
      continue;
    }

    char cmd = input[0];

    // Handle commands
    if (cmd == 'q' || cmd == 'Q') {
      running = false;
      std::cout << "Quitting...\n";
    } else if (cmd == 's' || cmd == 'S') {
      transport->stopAllClips();
      std::cout << "Stopped all clips\n";
    } else if (cmd == 'h' || cmd == 'H' || cmd == '?') {
      printUsage(argv[0]);
      printClipStatus(clips);
    } else if (cmd >= '1' && cmd <= '9') {
      int clip_index = cmd - '1';
      if (clip_index < static_cast<int>(clips.size())) {
        const auto& clip = clips[clip_index];
        if (clip.handle.isValid()) {
          auto result = transport->startClip(clip.handle, 0);
          if (result == orpheus::SessionGraphError::OK) {
            std::cout << "Triggered: " << clip.name << "\n";
          } else {
            std::cout << "Failed to start clip: " << clip.name << "\n";
          }
        }
      } else {
        std::cout << "Invalid clip number (1-" << clips.size() << ")\n";
      }
    } else {
      std::cout << "Unknown command. Press 'h' for help.\n";
    }
  }

  // 7. Clean up
  transport->stopAllClips();
  driver->stop();

  std::cout << "\nShutdown complete.\n";

  return 0;
}
