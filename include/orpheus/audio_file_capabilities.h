// SPDX-License-Identifier: MIT
#pragma once

#include <orpheus/audio_file_reader.h>
#include <orpheus/audio_file_writer.h>
#include <orpheus/errors.h>
#include <orpheus/export.h>

#include <span>
#include <string>

namespace orpheus {

/// Immutable SDK codec policy registry. Spans reference static storage for the
/// lifetime of the SDK module; this query performs no file I/O or media setup.
struct AudioFileCodecCapabilities {
  AudioFileFormat format;
  bool documented_read_support;
  std::span<const AudioSampleFormat> write_sample_formats;
};

struct AudioFileCapabilities {
  bool file_io_available;
  std::span<const AudioFileCodecCapabilities> codecs;
};

/// Return codec policy and provider availability without allocating or probing.
/// Rows describe SDK policy, independently of provider availability. Only WAV,
/// AIFF and FLAC have documented decoding coverage; the backend may decode
/// other files successfully. Use probeAudioFile for actual-file acceptance.
ORPHEUS_API AudioFileCapabilities getAudioFileCapabilities() noexcept;
/// Validate a proposed writer tuple before creating a writer or destination.
/// No file I/O, media-object allocation, or fixed sample-rate menu. Positive
/// rates up to INT_MAX and positive channel counts are checked as an exact
/// backend tuple. Error precedence: invalid rate/channels -> InvalidParameter;
/// unsupported container -> NotSupported; invalid encoding (including float
/// FLAC) -> InvalidParameter; absent provider -> NotReady; backend refusal ->
/// InvalidParameter; acceptance -> OK. Opening a destination may still fail
/// with InternalError independently of successful preflight.
ORPHEUS_API SessionGraphError preflightAudioFileWrite(const AudioFileWriterConfig& config) noexcept;
/// Background/control-only header probe; does not decode, hash, or create media.
/// Successful metadata has an empty file_hash_sha256; unknown bit depth is zero.
/// This probe opens/closes a file and may allocate backend header state; it is
/// not allocation-free or realtime-safe. Empty path -> InvalidParameter;
/// absent provider -> NotReady; unreadable/missing/corrupt input -> InternalError
/// with a backend explanation; invalid header fields -> InvalidParameter.
ORPHEUS_API Result<AudioFileMetadata> probeAudioFile(const std::string& file_path);

} // namespace orpheus
