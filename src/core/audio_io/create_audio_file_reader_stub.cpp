// SPDX-License-Identifier: MIT
#include <orpheus/audio_file_reader.h>

namespace orpheus {

// Fallback factory implementation when libsndfile is not available
// This ensures the symbol exists for linking even when SNDFILE_FOUND is false
// The actual implementation in audio_file_reader_libsndfile.cpp will override this
// when libsndfile is present
std::unique_ptr<IAudioFileReader> createAudioFileReader() {
  // Return nullptr to indicate audio file reading is not available
  // Callers should check for nullptr before using
  return nullptr;
}

} // namespace orpheus
