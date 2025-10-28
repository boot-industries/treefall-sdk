// SPDX-License-Identifier: MIT
// examples/offline_renderer/offline_renderer.cpp
//
// Offline Renderer Example
// Demonstrates non-real-time rendering to WAV files

#include <cmath>
#include <iostream>
#include <orpheus/audio_file_reader.h>
#include <orpheus/audio_file_writer.h>
#include <orpheus/offline_render.h>
#include <orpheus/transport_controller.h>
#include <string>
#include <vector>

// Print usage information
void printUsage(const char* program_name) {
  std::cout << "\nOffline Renderer Example\n";
  std::cout << "========================\n\n";
  std::cout << "Usage: " << program_name
            << " --output <output.wav> [options] <input1.wav> <input2.wav> ...\n\n";
  std::cout << "Options:\n";
  std::cout << "  --output FILE    Output WAV file path (required)\n";
  std::cout << "  --duration SEC   Duration in seconds (default: longest input)\n";
  std::cout << "  --sample-rate HZ Sample rate in Hz (default: 48000)\n";
  std::cout << "  --bit-depth N    Bit depth: 16, 24, or 32 (default: 24)\n";
  std::cout << "  --help           Show this help message\n\n";
  std::cout << "Examples:\n";
  std::cout << "  # Render single file\n";
  std::cout << "  " << program_name << " --output out.wav input.wav\n\n";
  std::cout << "  # Mix multiple files\n";
  std::cout << "  " << program_name << " --output mix.wav drums.wav bass.wav vocals.wav\n\n";
  std::cout << "  # Render with specific settings\n";
  std::cout << "  " << program_name
            << " --output out.wav --duration 10 --sample-rate 96000 --bit-depth 32 input.wav\n\n";
}

// Parse command-line arguments
struct RenderConfig {
  std::string output_file;
  std::vector<std::string> input_files;
  uint32_t sample_rate = 48000;
  uint32_t bit_depth = 24;
  double duration_seconds = 0.0; // 0 = auto-detect from longest input
};

bool parseArgs(int argc, char** argv, RenderConfig& config) {
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];

    if (arg == "--help" || arg == "-h") {
      return false;
    } else if (arg == "--output" || arg == "-o") {
      if (i + 1 < argc) {
        config.output_file = argv[++i];
      } else {
        std::cerr << "Error: --output requires a filename\n";
        return false;
      }
    } else if (arg == "--duration" || arg == "-d") {
      if (i + 1 < argc) {
        config.duration_seconds = std::stod(argv[++i]);
      } else {
        std::cerr << "Error: --duration requires a value in seconds\n";
        return false;
      }
    } else if (arg == "--sample-rate" || arg == "-r") {
      if (i + 1 < argc) {
        config.sample_rate = std::stoi(argv[++i]);
      } else {
        std::cerr << "Error: --sample-rate requires a value in Hz\n";
        return false;
      }
    } else if (arg == "--bit-depth" || arg == "-b") {
      if (i + 1 < argc) {
        config.bit_depth = std::stoi(argv[++i]);
        if (config.bit_depth != 16 && config.bit_depth != 24 && config.bit_depth != 32) {
          std::cerr << "Error: --bit-depth must be 16, 24, or 32\n";
          return false;
        }
      } else {
        std::cerr << "Error: --bit-depth requires a value\n";
        return false;
      }
    } else if (arg[0] != '-') {
      // Input file
      config.input_files.push_back(arg);
    } else {
      std::cerr << "Error: Unknown option: " << arg << "\n";
      return false;
    }
  }

  if (config.output_file.empty()) {
    std::cerr << "Error: --output is required\n";
    return false;
  }

  if (config.input_files.empty()) {
    std::cerr << "Error: At least one input file is required\n";
    return false;
  }

  return true;
}

// Progress callback for render
class RenderProgressCallback : public orpheus::IOfflineRenderCallback {
public:
  void onProgress(double progress_0_to_1, uint64_t /*frames_rendered*/,
                  uint64_t /*total_frames*/) override {
    int percent = static_cast<int>(progress_0_to_1 * 100.0);

    // Simple progress bar (update every 5%)
    if (percent >= last_percent_ + 5) {
      std::cout << "Progress: " << percent << "%\n";
      last_percent_ = percent;
    }
  }

private:
  int last_percent_ = 0;
};

int main(int argc, char** argv) {
  // Parse command-line arguments
  RenderConfig config;
  if (!parseArgs(argc, argv, config)) {
    printUsage(argv[0]);
    return 1;
  }

  // 1. Load all input files and determine render duration
  std::cout << "\nLoading input files...\n";

  std::vector<orpheus::ClipHandle> clip_handles;
  double max_duration = 0.0;

  // Create transport controller
  auto transport = orpheus::createTransportController(nullptr, config.sample_rate);

  orpheus::TransportConfig transport_config;
  transport_config.sample_rate = config.sample_rate;
  transport_config.buffer_size = 512;
  transport_config.num_outputs = 2;

  if (transport->initialize(transport_config) != orpheus::SessionGraphError::OK) {
    std::cerr << "Failed to initialize transport\n";
    return 1;
  }

  // Register all input clips
  for (const auto& input_file : config.input_files) {
    auto reader = orpheus::createAudioFileReader();
    auto result = reader->open(input_file);

    if (!result.isOk()) {
      std::cerr << "Warning: Failed to load " << input_file << ": " << result.errorMessage << "\n";
      continue;
    }

    const auto& metadata = *result;

    std::cout << "  Loaded: " << input_file << " (" << metadata.durationSeconds() << "s, "
              << metadata.sample_rate << "Hz, " << metadata.num_channels << "ch)\n";

    // Track longest clip
    if (metadata.durationSeconds() > max_duration) {
      max_duration = metadata.durationSeconds();
    }

    // Register clip with transport
    orpheus::ClipRegistration clip_reg;
    clip_reg.audio_file_path = input_file;
    clip_reg.trim_in_samples = 0;
    clip_reg.trim_out_samples = metadata.duration_samples;

    auto clip_handle = transport->registerClipAudio(clip_reg);

    if (!clip_handle.isValid()) {
      std::cerr << "Warning: Failed to register " << input_file << "\n";
      continue;
    }

    clip_handles.push_back(clip_handle);
  }

  if (clip_handles.empty()) {
    std::cerr << "Error: No valid input files loaded\n";
    return 1;
  }

  // Determine render duration
  double render_duration = (config.duration_seconds > 0.0) ? config.duration_seconds : max_duration;

  std::cout << "\nRender configuration:\n";
  std::cout << "  Output:      " << config.output_file << "\n";
  std::cout << "  Duration:    " << render_duration << " seconds\n";
  std::cout << "  Sample rate: " << config.sample_rate << " Hz\n";
  std::cout << "  Bit depth:   " << config.bit_depth << " bit\n";
  std::cout << "  Input clips: " << clip_handles.size() << "\n\n";

  // 2. Start all clips at time 0
  for (const auto& handle : clip_handles) {
    if (transport->startClip(handle, 0) != orpheus::SessionGraphError::OK) {
      std::cerr << "Warning: Failed to start clip\n";
    }
  }

  // 3. Render offline
  std::cout << "Rendering...\n";

  orpheus::OfflineRenderConfig render_config;
  render_config.sample_rate = config.sample_rate;
  render_config.bit_depth = config.bit_depth;
  render_config.num_channels = 2;
  render_config.duration_samples = static_cast<uint64_t>(render_duration * config.sample_rate);
  render_config.output_file_path = config.output_file;

  RenderProgressCallback progress_callback;

  auto renderer = orpheus::createOfflineRenderer(transport.get());
  auto render_result = renderer->render(render_config, &progress_callback);

  if (!render_result.isOk()) {
    std::cerr << "\nError: Render failed: " << render_result.errorMessage << "\n";
    return 1;
  }

  // 4. Report success
  std::cout << "\nRender complete!\n";
  std::cout << "  Output file: " << config.output_file << "\n";
  std::cout << "  Duration:    " << render_duration << " seconds\n";

  // Calculate file size
  uint64_t samples = render_config.duration_samples;
  uint64_t bytes_per_sample = config.bit_depth / 8;
  uint64_t file_size_bytes = samples * 2 * bytes_per_sample; // 2 channels
  double file_size_mb = file_size_bytes / (1024.0 * 1024.0);

  std::cout << "  File size:   " << file_size_mb << " MB\n\n";

  return 0;
}
