// SPDX-License-Identifier: MIT
#include <orpheus/audio_file_writer.h>

namespace orpheus {

// Fallback factory implementation when libsndfile is not available.
// Mirrors create_audio_file_reader_stub.cpp: the symbol must exist for
// linking; callers must check for nullptr.
std::unique_ptr<IAudioFileWriter> createAudioFileWriter() {
  return nullptr;
}

} // namespace orpheus
