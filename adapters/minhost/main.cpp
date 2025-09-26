#include "click_renderer.h"

#include "orpheus/abi.h"
#include "orpheus/session.h"

#include <exception>
#include <iostream>
#include <optional>
#include <string>

namespace {

struct Options {
  bool play = false;
  bool stop = false;
  std::optional<std::string> outputPath;
  double bpm = 120.0;
  int bars = 1;
};

void PrintUsage() {
  std::cout << "Usage: orpheus_minhost [--play] [--stop] [--out <file>]"
            << " [--bpm <value>] [--bars <count>]" << std::endl;
}

std::optional<Options> ParseOptions(int argc, char **argv) {
  Options options;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--play") {
      options.play = true;
    } else if (arg == "--stop") {
      options.stop = true;
    } else if (arg == "--out") {
      if (i + 1 >= argc) {
        std::cerr << "--out requires a path" << std::endl;
        return std::nullopt;
      }
      options.outputPath = argv[++i];
    } else if (arg == "--bpm") {
      if (i + 1 >= argc) {
        std::cerr << "--bpm requires a value" << std::endl;
        return std::nullopt;
      }
      options.bpm = std::stod(argv[++i]);
    } else if (arg == "--bars") {
      if (i + 1 >= argc) {
        std::cerr << "--bars requires a value" << std::endl;
        return std::nullopt;
      }
      options.bars = std::stoi(argv[++i]);
    } else {
      std::cerr << "Unknown argument: " << arg << std::endl;
      return std::nullopt;
    }
  }
  return options;
}

void PrintSessionPreview(bool play, bool stop) {
  orpheus::SessionState state;
  if (play) {
    state.events.push_back({"transport", "play"});
  }
  if (stop) {
    state.events.push_back({"transport", "stop"});
  }

  const std::string serialized = orpheus::SerializeSession(state);
  if (!serialized.empty()) {
    std::cout << "Session events:" << std::endl;
    std::cout << serialized << std::endl;
  }
}

}  // namespace

int main(int argc, char **argv) {
  std::cout << "Orpheus Minhost (ABI " << orpheus::ToString(orpheus::kCurrentAbi)
            << ")" << std::endl;

  auto parsed = ParseOptions(argc, argv);
  if (!parsed) {
    PrintUsage();
    return 1;
  }

  const Options &options = *parsed;
  if (options.play) {
    std::cout << "Transport: play" << std::endl;
  }
  if (options.stop) {
    std::cout << "Transport: stop" << std::endl;
  }

  PrintSessionPreview(options.play, options.stop);

  if (options.outputPath) {
    try {
      orpheus::minhost::ClickRenderer renderer;
      renderer.RenderClick(*options.outputPath, 44100, options.bpm,
                           options.bars);
      std::cout << "Rendered click track to " << *options.outputPath
                << std::endl;
    } catch (const std::exception &ex) {
      std::cerr << "Rendering failed: " << ex.what() << std::endl;
      return 1;
    }
  }

  return 0;
}
