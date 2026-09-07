// SPDX-License-Identifier: MIT
// examples/multi_clip_trigger/multi_clip_trigger.cpp
//
// Multi-Clip Trigger Example
// Demonstrates soundboard-style playback with multiple clips

#include <algorithm>
#include <iostream>
#include <string>
#include <treefall/audio_driver.h>
#include <treefall/audio_file_reader.h>
#include <treefall/transport_controller.h>
#include <vector>

class MultiClipAudioCallback : public treefall::IAudioCallback {
public:
  explicit MultiClipAudioCallback(treefall::ITransportController* transport)
      : transport_(transport) {}

  void processAudio(const treefall::AudioProcessBlock& block) noexcept override {
    transport_->processAudio(block.output_buffers, block.num_output_channels, block.num_frames);
  }

private:
  treefall::ITransportController* transport_;
};

struct ClipInfo {
  treefall::ClipHandle handle = 0;
  std::string name;
  std::string file_path;
  double duration_seconds = 0.0;
};

void printUsage(const char* program_name) {
  std::cout << "\nMulti-Clip Trigger Example\n"
            << "===========================\n\n"
            << "Usage: " << program_name << " <audio_file1.wav> <audio_file2.wav> ...\n\n"
            << "Controls:\n  1-9  : Trigger clips 1-9\n  s    : Stop all clips\n"
            << "  q    : Quit application\n  h    : Show this help\n\n";
}

void printClipStatus(const std::vector<ClipInfo>& clips) {
  std::cout << "\nLoaded " << clips.size() << " clips:\n---------------------\n";
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
  std::cout << "\nLoading audio files...\n";
  for (int i = 1; i < argc && i <= 9; ++i) {
    auto reader = treefall::createAudioFileReader();
    if (!reader) {
      std::cerr << "Error: Audio file reader is unavailable\n";
      return 1;
    }
    auto result = reader->open(argv[i]);
    if (!result.isOk()) {
      std::cerr << "Warning: Failed to load " << argv[i] << ": " << result.errorMessage << "\n";
      continue;
    }

    const auto& metadata = *result;
    max_sample_rate = std::max(max_sample_rate, metadata.sample_rate);
    std::string file_path = argv[i];
    const size_t last_slash = file_path.find_last_of("/\\");
    const std::string filename =
        last_slash != std::string::npos ? file_path.substr(last_slash + 1) : file_path;
    clips.push_back(ClipInfo{0, filename, std::move(file_path), metadata.durationSeconds()});
    std::cout << "  [" << clips.size() << "] Loaded: " << filename << " ("
              << metadata.durationSeconds() << "s, " << metadata.sample_rate << "Hz, "
              << metadata.num_channels << "ch)\n";
  }

  if (clips.empty()) {
    std::cerr << "Error: No valid audio files loaded\n";
    return 1;
  }

  treefall::TransportConfig config{
      .sampleRate = max_sample_rate, .outputChannels = 2, .maxBlockFrames = 512};
  auto transport = treefall::createTransportController(nullptr, config);
  if (!transport) {
    std::cerr << "Failed to create transport\n";
    return 1;
  }

  std::cout << "\nRegistering clips with transport...\n";
  for (size_t i = 0; i < clips.size(); ++i) {
    auto& clip = clips[i];
    clip.handle = static_cast<treefall::ClipHandle>(i + 1);
    if (transport->registerClipAudio(clip.handle, clip.file_path) !=
            treefall::SessionGraphError::OK ||
        transport->prepareClipAudio(clip.handle) != treefall::SessionGraphError::OK) {
      std::cerr << "Warning: Failed to register or prepare " << clip.name << "\n";
      clip.handle = 0;
    }
  }

#ifdef ORPHEUS_ENABLE_COREAUDIO
  auto driver = treefall::createCoreAudioDriver();
#else
  auto driver = treefall::createDummyAudioDriver();
  std::cout << "\nNote: Using dummy driver (no audio output on this platform)\n";
#endif
  if (!driver) {
    std::cerr << "Audio driver is unavailable\n";
    return 1;
  }

  treefall::AudioDriverConfig driver_config;
  driver_config.sample_rate = config.sampleRate;
  driver_config.buffer_size = static_cast<uint16_t>(config.maxBlockFrames);
  driver_config.num_inputs = 0;
  driver_config.num_outputs = static_cast<uint16_t>(config.outputChannels);
  if (driver->initialize(driver_config) != treefall::SessionGraphError::OK) {
    std::cerr << "Failed to initialize audio driver\n";
    return 1;
  }

  MultiClipAudioCallback callback(transport.get());
  if (driver->start(&callback) != treefall::SessionGraphError::OK) {
    std::cerr << "Failed to start audio driver\n";
    return 1;
  }

  printClipStatus(clips);
  bool running = true;
  while (running) {
    transport->processCallbacks();
    std::cout << "> " << std::flush;
    std::string input;
    if (!std::getline(std::cin, input)) {
      break;
    }
    transport->processCallbacks();
    if (input.empty()) {
      continue;
    }

    const char cmd = input[0];
    if (cmd == 'q' || cmd == 'Q') {
      running = false;
      std::cout << "Quitting...\n";
    } else if (cmd == 's' || cmd == 'S') {
      const auto result = transport->stopAllClips();
      if (result == treefall::SessionGraphError::OK) {
        std::cout << "Stopped all clips\n";
      } else {
        std::cout << "Failed to stop all clips\n";
      }
    } else if (cmd == 'h' || cmd == 'H' || cmd == '?') {
      printUsage(argv[0]);
      printClipStatus(clips);
    } else if (cmd >= '1' && cmd <= '9') {
      const int clip_index = cmd - '1';
      if (clip_index >= 0 && clip_index < static_cast<int>(clips.size())) {
        const auto& clip = clips[clip_index];
        if (clip.handle != 0) {
          const auto result = transport->startClip(clip.handle, 0);
          std::cout << (result == treefall::SessionGraphError::OK ? "Triggered: "
                                                                  : "Failed to start clip: ")
                    << clip.name << "\n";
        } else {
          std::cout << "Clip is not registered: " << clip.name << "\n";
        }
      } else {
        std::cout << "Invalid clip number (1-" << clips.size() << ")\n";
      }
    } else {
      std::cout << "Unknown command. Press 'h' for help.\n";
    }
  }

  const auto stop_result = transport->stopAllClips();
  if (stop_result != treefall::SessionGraphError::OK) {
    std::cerr << "Warning: failed to admit shutdown stop command\n";
  }
  transport->processCallbacks();
  driver->stop();
  std::cout << "\nShutdown complete.\n";
  return 0;
}
