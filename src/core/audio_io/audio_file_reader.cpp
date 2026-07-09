// SPDX-License-Identifier: MIT
//
// Default (base-interface) implementations for IAudioFileReader. Kept in a
// dedicated, always-built translation unit so the out-of-line definition of the
// defaulted `readSamplesEndingAt` virtual is available to every consumer,
// whether or not libsndfile is present.

#include <orpheus/audio_file_reader.h>

namespace orpheus {

// Portable backward/windowed read (ORP128, FTR018). Implemented purely on top of
// the existing forward primitives (seek + readSamples), so it works for any
// reader out of the box; concrete readers may override with a faster path.
//
// Reads the frame window [end_sample - num_frames + 1, end_sample] in forward
// order. When the window underflows the file start (index < 0), only the
// in-range trailing frames are read and the returned count is reduced. The read
// position observed on entry is restored before returning, so this call composes
// cleanly with ordinary forward streaming.
Result<size_t> IAudioFileReader::readSamplesEndingAt(int64_t end_sample, float* buffer,
                                                     size_t num_frames) {
  Result<size_t> result;
  result.value = 0;

  if (buffer == nullptr) {
    result.error = SessionGraphError::InvalidParameter;
    result.errorMessage = "readSamplesEndingAt: null buffer";
    return result;
  }
  if (!isOpen()) {
    result.error = SessionGraphError::NotReady;
    result.errorMessage = "readSamplesEndingAt: reader not open";
    return result;
  }
  if (num_frames == 0 || end_sample < 0) {
    return result; // nothing to read; value == 0, OK
  }

  // Window start, clamped so we never request negative indices. The number of
  // in-range frames shrinks by however much the window underflows below 0.
  const int64_t requested_start = end_sample - static_cast<int64_t>(num_frames) + 1;
  const int64_t start = requested_start < 0 ? 0 : requested_start;
  const size_t in_range = static_cast<size_t>(end_sample - start + 1);

  // Preserve the caller's forward-streaming position and restore it afterward.
  const int64_t prior_position = getCurrentPosition();

  const SessionGraphError seek_err = seek(start);
  if (seek_err != SessionGraphError::OK) {
    seek(prior_position); // best-effort restore
    result.error = seek_err;
    result.errorMessage = "readSamplesEndingAt: seek failed";
    return result;
  }

  Result<size_t> read = readSamples(buffer, in_range);

  // Restore the read cursor regardless of the read outcome.
  seek(prior_position);

  if (!read.isOk()) {
    result.error = read.error;
    result.errorMessage = read.errorMessage;
    return result;
  }

  result.value = read.value;
  return result;
}

} // namespace orpheus
