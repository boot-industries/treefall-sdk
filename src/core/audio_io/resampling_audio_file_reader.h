// SPDX-License-Identifier: MIT
#pragma once

#include <orpheus/audio_file_reader.h>
#include <orpheus/polyphase_resampler.h>

#include <memory>
#include <vector>

namespace orpheus {

/// ORP127 G6: Decorator that presents a wrapped IAudioFileReader as if it were
/// sampled at a target rate, resampling on the fly with a deterministic
/// polyphase converter. Implements the full IAudioFileReader interface, so it is
/// a drop-in replacement — the transport does not need to know SRC happened.
///
/// The reported metadata (sample_rate, duration_samples) and all sample
/// positions (seek / getCurrentPosition) are expressed in the TARGET-rate
/// timeline, so the rest of the SDK operates in a single rate. This keeps trim
/// points, fades, and cue points sample-accurate against the engine rate.
///
/// Host-neutral, no third-party dependency. Not real-time safe on open()
/// (allocates), but readSamples() only allocates a small scratch buffer that
/// grows monotonically and then stabilizes.
class ResamplingAudioFileReader : public IAudioFileReader {
public:
  /// @param inner The source reader (already opened or to be opened).
  /// @param targetRate The engine/target sample rate.
  ResamplingAudioFileReader(std::shared_ptr<IAudioFileReader> inner, uint32_t targetRate);

  Result<AudioFileMetadata> open(const std::string& file_path) override;
  Result<size_t> readSamples(float* buffer, size_t num_samples) override;
  SessionGraphError seek(int64_t sample_position) override;
  void close() override;
  int64_t getCurrentPosition() const override;
  bool isOpen() const override;

private:
  void rebuildResampler();
  // Fill m_outBuffer with at least `frames` target-rate frames (or until EOF).
  void produceUntil(size_t frames);

  std::shared_ptr<IAudioFileReader> m_inner;
  uint32_t m_targetRate;
  uint32_t m_sourceRate{0};
  uint16_t m_numChannels{0};
  int64_t m_sourceDurationSamples{0};
  int64_t m_targetDurationSamples{0};
  bool m_open{false};

  std::unique_ptr<PolyphaseResampler> m_resampler;

  // Target-rate output that has been produced but not yet handed out.
  std::vector<float> m_outBuffer; // interleaved
  size_t m_outReadFrame{0};       // next unread frame index in m_outBuffer
  int64_t m_targetPos{0};         // current position in target-rate frames
  bool m_sourceEof{false};

  // Scratch input block read from the inner reader.
  std::vector<float> m_inScratch;
};

} // namespace orpheus
