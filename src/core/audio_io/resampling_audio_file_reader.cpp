// SPDX-License-Identifier: MIT
#include "resampling_audio_file_reader.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace orpheus {

namespace {
constexpr size_t kInputBlockFrames = 4096; // frames read from inner per pump
// Passed explicitly to PolyphaseResampler so seek() derives the same FIR span.
constexpr uint32_t kTapsPerPhase = 16;
} // namespace

ResamplingAudioFileReader::ResamplingAudioFileReader(std::shared_ptr<IAudioFileReader> inner,
                                                     uint32_t targetRate)
    : m_inner(std::move(inner)), m_targetRate(targetRate == 0 ? 48000 : targetRate) {}

void ResamplingAudioFileReader::rebuildResampler() {
  m_resampler = std::make_unique<PolyphaseResampler>(m_sourceRate, m_targetRate, m_numChannels,
                                                     kTapsPerPhase);
  m_outBuffer.clear();
  m_outReadFrame = 0;
  m_sourceEof = false;

  // Reserve steady-state capacity so readSamples() does not reallocate in the
  // common case. One input block converts to ~block * targetRate/sourceRate
  // output frames; reserve a few blocks' worth plus headroom. Callers read from
  // streaming worker or control threads, never from processAudio.
  const uint16_t ch = m_numChannels == 0 ? 1 : m_numChannels;
  const double ratio =
      static_cast<double>(m_targetRate) / static_cast<double>(m_sourceRate == 0 ? 1 : m_sourceRate);
  const size_t perBlockOut = static_cast<size_t>(kInputBlockFrames * ratio) + 64;
  m_outBuffer.reserve(perBlockOut * 4 * ch);
  m_inScratch.reserve(kInputBlockFrames * ch);
}

Result<AudioFileMetadata> ResamplingAudioFileReader::open(const std::string& file_path) {
  auto res = m_inner->open(file_path);
  if (!res.isOk()) {
    return res;
  }

  AudioFileMetadata meta = res.value;
  m_sourceRate = meta.sample_rate;
  m_numChannels = meta.num_channels;
  m_sourceDurationSamples = meta.duration_samples;

  // Target-rate duration (rounded). Positions/trims operate on this timeline.
  m_targetDurationSamples = static_cast<int64_t>(
      std::llround(static_cast<double>(m_sourceDurationSamples) *
                   static_cast<double>(m_targetRate) / static_cast<double>(m_sourceRate)));

  rebuildResampler();
  m_targetPos = 0;
  m_open = true;

  // Present the file to the rest of the SDK at the TARGET rate.
  meta.sample_rate = m_targetRate;
  meta.duration_samples = m_targetDurationSamples;
  Result<AudioFileMetadata> out;
  out.value = meta;
  out.error = SessionGraphError::OK;
  return out;
}

SessionGraphError ResamplingAudioFileReader::produceUntil(size_t frames) {
  const uint16_t ch = m_numChannels;
  // Frames already available (unread) in m_outBuffer.
  auto availableFrames = [&]() { return (m_outBuffer.size() / ch) - m_outReadFrame; };

  std::vector<float> converted;
  while (availableFrames() < frames && !m_sourceEof) {
    // Read a block from the inner (source-rate) reader.
    m_inScratch.resize(kInputBlockFrames * ch);
    auto rd = m_inner->readSamples(m_inScratch.data(), kInputBlockFrames);
    if (!rd.isOk()) {
      return rd.error;
    }
    const size_t inFrames = rd.value;
    if (inFrames == 0) {
      m_sourceEof = true;
      // A successful zero-frame read is the sole EOF signal. Feed one silence
      // block so the FIR history flushes its final real samples.
      std::vector<float> tail(kInputBlockFrames * ch, 0.0f);
      m_resampler->process(tail.data(), kInputBlockFrames, converted);
      if (!converted.empty()) {
        m_outBuffer.insert(m_outBuffer.end(), converted.begin(), converted.end());
      }
      break;
    }
    m_resampler->process(m_inScratch.data(), inFrames, converted);
    if (!converted.empty()) {
      m_outBuffer.insert(m_outBuffer.end(), converted.begin(), converted.end());
    }
  }

  // Compact m_outBuffer occasionally to bound memory (drop consumed frames).
  if (m_outReadFrame > 0 && m_outReadFrame >= (m_outBuffer.size() / ch)) {
    m_outBuffer.clear();
    m_outReadFrame = 0;
  }
  return SessionGraphError::OK;
}

Result<size_t> ResamplingAudioFileReader::readSamples(float* buffer, size_t num_samples) {
  Result<size_t> out;
  if (!m_open || m_numChannels == 0) {
    out.value = 0;
    out.error = SessionGraphError::NotReady;
    return out;
  }
  const uint16_t ch = m_numChannels;

  const SessionGraphError produceResult = produceUntil(num_samples);
  if (produceResult != SessionGraphError::OK) {
    out.value = 0;
    out.error = produceResult;
    return out;
  }

  size_t availableFrames = (m_outBuffer.size() / ch) - m_outReadFrame;
  size_t give = std::min(num_samples, availableFrames);

  const float* src = m_outBuffer.data() + m_outReadFrame * ch;
  std::copy(src, src + give * ch, buffer);
  m_outReadFrame += give;
  m_targetPos += static_cast<int64_t>(give);

  // Drop fully-consumed leading data to keep the buffer bounded.
  if (m_outReadFrame > kInputBlockFrames * 4) {
    m_outBuffer.erase(m_outBuffer.begin(),
                      m_outBuffer.begin() + static_cast<long>(m_outReadFrame) * ch);
    m_outReadFrame = 0;
  }

  out.value = give;
  out.error = SessionGraphError::OK;
  return out;
}

SessionGraphError ResamplingAudioFileReader::seek(int64_t sample_position) {
  if (!m_open || m_numChannels == 0) {
    return SessionGraphError::NotReady;
  }
  // sample_position is in TARGET-rate frames.
  const int64_t target = std::clamp<int64_t>(sample_position, 0, m_targetDurationSamples);

  // L, M and the FIR half-width, with the exact expressions of the
  // PolyphaseResampler constructor (polyphase_resampler.cpp).
  const uint32_t inputRate = m_sourceRate == 0 ? 1 : m_sourceRate;
  const uint32_t outputRate = m_targetRate == 0 ? 1 : m_targetRate;
  const int64_t g = std::gcd<int64_t>(outputRate, inputRate);
  const int64_t L = outputRate / g;
  const int64_t M = inputRate / g;
  const double ratio = static_cast<double>(outputRate) / static_cast<double>(inputRate);
  const double cutoff = 0.5 * std::min(1.0, ratio);
  const double halfWidthInput = static_cast<double>(kTapsPerPhase) * (0.5 / cutoff);
  uint32_t firHalf = static_cast<uint32_t>(std::ceil(halfWidthInput));
  if (firHalf < 1) {
    firHalf = 1;
  }

  // Output frame n is centred on source frame n*M/L. Restart the converter at a
  // source frame s0 that is a multiple of M, so output n0 = (s0/M)*L of the
  // fresh converter has the same FIR phase as the whole-file decode, and that
  // lies at least firHalf frames before the target's centre, so every tap of
  // every output from the target onward reads real source history.
  const int64_t centre = target * M / L;
  const int64_t primedStart = std::max<int64_t>(0, centre - static_cast<int64_t>(firHalf));
  const int64_t sourceStart = M * (primedStart / M);
  const int64_t outputStart = (sourceStart / M) * L;

  const SessionGraphError err = m_inner->seek(sourceStart);
  if (err != SessionGraphError::OK) {
    return err;
  }
  // Reset the streaming converter only after the wrapped reader accepted the
  // seek, so a failed seek leaves its prior phase, buffers, and position intact.
  rebuildResampler();
  m_targetPos = outputStart;

  // Discard the priming outputs that precede the target.
  const auto discard = static_cast<size_t>(target - outputStart);
  const SessionGraphError primeResult = produceUntil(discard);
  if (primeResult != SessionGraphError::OK) {
    return primeResult;
  }
  const size_t available = (m_outBuffer.size() / m_numChannels) - m_outReadFrame;
  m_outReadFrame += std::min(discard, available);
  m_targetPos = target;
  return SessionGraphError::OK;
}

void ResamplingAudioFileReader::close() {
  m_inner->close();
  m_open = false;
  m_outBuffer.clear();
  m_outReadFrame = 0;
  m_resampler.reset();
}

int64_t ResamplingAudioFileReader::getCurrentPosition() const {
  return m_targetPos;
}

bool ResamplingAudioFileReader::isOpen() const {
  return m_open && m_inner && m_inner->isOpen();
}

} // namespace orpheus
