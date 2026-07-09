// SPDX-License-Identifier: MIT
#pragma once

// ORP134 G7: host-neutral recorder plumbing.
//
// FourTrack (and future recording hosts) currently hook the raw driver input
// callback and hand-roll a ring buffer + async disk writer. The SDK now
// provides the reusable, non-opinionated pieces:
//
//  * AudioInputRing — a lock-free SPSC ring of interleaved float frames.
//    The AUDIO thread produces (write: memcpy + atomic index, never blocks,
//    drops whole buffers on overflow and counts them); ONE background thread
//    consumes (read) and feeds e.g. IAudioFileWriter (ORP134 G5) — making
//    "capture → disk" an SDK-supported path.
//  * IAudioInputStream — the capture contract hosts consume instead of the
//    raw driver callback: the driver-facing side pushes captured frames on
//    the audio thread; the host-facing side drains them on a background
//    thread. createAudioInputStream() returns the SDK's ring-backed
//    implementation.
//
// Deliberately NOT here (host policy, per ORP134 G7): take management,
// punch in/out, latency compensation, arming semantics, file naming.

#include <orpheus/errors.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace orpheus {

/// Lock-free single-producer/single-consumer ring of interleaved audio
/// frames.
///
/// Threading contract:
/// - write(): exactly ONE producer thread (the audio/driver callback).
///   Realtime-safe: memcpy + relaxed/acquire/release atomics; no locks, no
///   allocation, never blocks. When the ring lacks space for the WHOLE
///   buffer, the write is dropped atomically (no partial frames) and
///   overflowCount() increments — matching the transport's event-ring
///   drop-don't-block policy.
/// - read(): exactly ONE consumer thread (a background writer/analysis
///   thread). Returns however many frames are available, up to maxFrames.
/// - framesAvailable()/overflowCount(): safe from any thread (diagnostics).
class AudioInputRing {
public:
  /// @param numChannels Interleaved channel count (>= 1)
  /// @param capacityFrames Ring capacity; rounded UP to a power of two
  ///        (allocation happens here, never on the audio thread)
  AudioInputRing(uint16_t numChannels, size_t capacityFrames);

  uint16_t numChannels() const {
    return m_numChannels;
  }

  /// Usable capacity in frames (power of two minus one slot semantics are
  /// handled internally; this is the real writable capacity).
  size_t capacityFrames() const {
    return m_capacityFrames;
  }

  /// AUDIO THREAD (single producer): append `frames` interleaved frames.
  /// @return frames written — either `frames` (all) or 0 (dropped: ring full)
  size_t write(const float* interleaved, size_t frames);

  /// BACKGROUND THREAD (single consumer): pop up to `maxFrames` frames.
  /// @return frames actually copied into `dest`
  size_t read(float* dest, size_t maxFrames);

  /// Frames currently queued (approximate under concurrency; exact when one
  /// side is idle).
  size_t framesAvailable() const;

  /// Number of write() calls dropped because the ring was full.
  uint64_t overflowCount() const {
    return m_overflows.load(std::memory_order_relaxed);
  }

private:
  const uint16_t m_numChannels;
  size_t m_capacityFrames; // power of two
  std::vector<float> m_data;
  std::atomic<size_t> m_writeIndex{0}; // in frames, monotonically wrapping
  std::atomic<size_t> m_readIndex{0};
  std::atomic<uint64_t> m_overflows{0};
};

/// Capture-stream configuration.
struct AudioInputStreamConfig {
  uint16_t num_channels = 2;    ///< Interleaved input channel count
  uint32_t sample_rate = 48000; ///< Capture sample rate (informational)
  /// Ring depth. Default 65536 frames ≈ 1.37 s @ 48 kHz — generous headroom
  /// for a background writer that flushes every few hundred ms.
  size_t ring_capacity_frames = 65536;
};

/// The capture contract between a driver input callback and a host's
/// background consumer (ORP134 G7).
///
/// Threading:
/// - capture(): AUDIO THREAD only (single producer). Realtime-safe;
///   never blocks; drops + counts on overflow.
/// - drain(): ONE background thread (single consumer).
/// - Queries: any thread.
class IAudioInputStream {
public:
  virtual ~IAudioInputStream() = default;

  virtual uint16_t numChannels() const = 0;
  virtual uint32_t sampleRate() const = 0;

  /// AUDIO THREAD: push captured interleaved frames into the stream.
  /// @return frames accepted (all or nothing; 0 == dropped, overflow counted)
  virtual size_t capture(const float* interleaved, size_t frames) = 0;

  /// BACKGROUND THREAD: pop up to maxFrames captured frames.
  /// @return frames copied into dest
  virtual size_t drain(float* dest, size_t maxFrames) = 0;

  /// Frames waiting to be drained.
  virtual size_t framesPending() const = 0;

  /// Capture buffers dropped because the consumer fell behind.
  virtual uint64_t overflowCount() const = 0;
};

/// Create the SDK's ring-backed input stream.
std::unique_ptr<IAudioInputStream> createAudioInputStream(const AudioInputStreamConfig& config);

} // namespace orpheus
