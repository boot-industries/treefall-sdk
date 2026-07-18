// SPDX-License-Identifier: MIT
#pragma once

// ORP134 G1: realtime clip playback sources.
//
// The transport's audio callback used to call IAudioFileReader::readSamples()
// / seek() directly — blocking libsndfile decode on the audio thread (the
// KNOWN_DEBT the realtime audit tracked since ORP121). These classes are the
// replacement contract: an IClipSource is an immutable, position-explicit,
// audio-thread-only VIEW of decoded engine-rate PCM. All decode, resample,
// and file I/O happens off the audio thread:
//
//  * PreparedClipSource — whole file decoded to memory during
//    prepareClipAudio()/startClip() (background/control thread). The common
//    short-clip case (soundboards, stingers, beds). Never misses.
//  * StreamingClipSource — fixed ring of PCM pages filled by a background
//    MediaStreamWorker for long files. The audio thread reads resident pages
//    (memcpy) and NEVER blocks: a cache miss returns false and the transport
//    emits silence + a BufferUnderrun event while the worker catches up.
//
// Threading model (strict ownership handoff — no locks on the audio thread,
// TSAN-clean by construction):
//  * A page is either FREE (start == -1, worker-owned, writable) or READY
//    (start >= 0, audio-owned, readable). The worker fills a FREE page then
//    publishes it with a release store of its aligned start frame; the audio
//    thread acquires it before reading. Only the AUDIO thread retires READY
//    pages (release store of -1) — it does so for any page outside its
//    current demand window, so the worker always regains pages to refill.
//  * The audio thread communicates its read position via a relaxed atomic
//    (setDemand); the worker polls it. After a large reposition the next
//    audio read retires stale pages and reports a miss (one silent buffer)
//    while the worker refills — streaming sources trade seek latency for
//    bounded memory; hosts needing instant random access use prepared
//    sources.
//
// Reads are position-explicit (no shared file cursor), which also makes
// multi-voice playback of one clip correct by construction — voices no
// longer fight over a single reader's seek position.

#include <orpheus/audio_file_reader.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace orpheus {

/// Audio-thread-safe random-access view of a clip's engine-rate PCM.
class IClipSource {
public:
  virtual ~IClipSource() = default;

  virtual uint16_t numChannels() const = 0;

  /// Total engine-rate frames available.
  virtual int64_t lengthFrames() const = 0;

  /// Copy up to `frames` interleaved frames starting at absolute frame `pos`
  /// into `dest`. AUDIO THREAD SAFE: no locks, no allocation, no I/O.
  ///
  /// @return true on success (framesRead = frames actually copied, capped at
  ///         EOF); false on a streaming cache miss (framesRead == 0, dest
  ///         untouched) — the caller renders silence and reports an underrun.
  virtual bool read(int64_t pos, float* dest, size_t frames, size_t& framesRead) = 0;

  /// Audio-thread hint: playback continues from `pos` (streaming prefetch
  /// target). Relaxed atomic store; no-op for prepared sources.
  virtual void setDemand(int64_t /*pos*/) {}

  /// True when the whole clip is memory-resident (read() can never miss).
  virtual bool isFullyResident() const = 0;
};

/// Whole-file PCM in memory. Immutable after construction.
class PreparedClipSource : public IClipSource {
public:
  /// Decode the reader's full content (engine-rate frames) into memory.
  /// BACKGROUND/CONTROL THREAD ONLY. Returns nullptr on decode failure.
  static std::shared_ptr<PreparedClipSource> decode(IAudioFileReader& reader, uint16_t numChannels,
                                                    int64_t lengthFrames);

  uint16_t numChannels() const override {
    return m_numChannels;
  }
  int64_t lengthFrames() const override {
    return m_lengthFrames;
  }
  bool read(int64_t pos, float* dest, size_t frames, size_t& framesRead) override;
  bool isFullyResident() const override {
    return true;
  }

private:
  PreparedClipSource(std::vector<float> pcm, uint16_t numChannels, int64_t lengthFrames);

  std::vector<float> m_pcm; // interleaved engine-rate PCM
  uint16_t m_numChannels;
  int64_t m_lengthFrames;
};

/// Fixed-page streaming source for long files.
///
/// The resident window is BIDIRECTIONAL (FTR025 T3b): one page behind the
/// demand position plus the demand page and two pages ahead. Forward playback
/// keeps ~2.7 s of lookahead; reverse/scrub playback keeps ~1.4 s of runway
/// behind the cursor, so position-explicit reads serve true backward playback
/// (descending positions) without a miss at every backward page crossing.
class StreamingClipSource : public IClipSource {
public:
  static constexpr size_t kPageFrames = 65536; ///< frames per page (~1.4s @ 48k)
  static constexpr size_t kWindowPages = 4;    ///< 1 behind + demand + 2 ahead (≈5.5s @ 48k)
  static constexpr size_t kNumPages = kWindowPages + 1; ///< reserved restart/refire page

  /// BACKGROUND/CONTROL THREAD. The reader is retained and used exclusively
  /// from worker/control threads (guarded by an internal mutex).
  StreamingClipSource(std::shared_ptr<IAudioFileReader> reader, uint16_t numChannels,
                      int64_t lengthFrames);

  uint16_t numChannels() const override {
    return m_numChannels;
  }
  int64_t lengthFrames() const override {
    return m_lengthFrames;
  }
  bool read(int64_t pos, float* dest, size_t frames, size_t& framesRead) override;
  void setDemand(int64_t pos) override {
    m_demand.store(pos, std::memory_order_relaxed);
  }
  bool isFullyResident() const override {
    return false;
  }

  /// Fill up to `max_pages` non-resident pages of the window around `pos`
  /// synchronously — the demand page first, then the forward pages, then the
  /// behind page. CONTROL/WORKER THREAD. One page is reserved outside the
  /// steady-state worker window so a control-thread start/refire can prime its
  /// first audible page before the realtime command resets the voice cursor.
  /// Hosts with latency-bounded transitions can pass max_pages = 1 to prime
  /// only the audible page and leave the rest to the worker.
  void prefill(int64_t pos, size_t max_pages = kWindowPages);

  /// One worker pass: refill FREE pages inside the current demand window.
  /// WORKER THREAD ONLY.
  void service();

private:
  struct Page {
    // -1 == FREE (worker-owned, writable); >= 0 == READY at that aligned
    // start frame (audio-owned, readable until the audio thread retires it).
    std::atomic<int64_t> start{-1};
    std::vector<float> data; // kPageFrames * numChannels, pre-allocated
  };

  static int64_t alignDown(int64_t pos) {
    return (pos / static_cast<int64_t>(kPageFrames)) * static_cast<int64_t>(kPageFrames);
  }

  /// Copy the subrange [pos, pos+frames) from a READY page covering it.
  /// Returns false when no resident page covers pos.
  bool copyFromResidentPage(int64_t pos, float* dest, size_t frames);

  /// Fill one FREE page with content starting at alignedStart and publish it.
  /// Worker/control thread. Returns false when no FREE page is available or
  /// the read fails.
  bool fillPage(int64_t alignedStart);

  std::shared_ptr<IAudioFileReader> m_reader; // worker/control threads only
  std::mutex m_readerMutex;                   // serializes prefill vs service
  std::atomic<int64_t> m_demand{0};
  uint16_t m_numChannels;
  int64_t m_lengthFrames;
  Page m_pages[kNumPages];
};

/// Background thread servicing every StreamingClipSource of a transport.
/// Owns no source (weak references); prunes dead sources automatically.
class MediaStreamWorker {
public:
  MediaStreamWorker();
  ~MediaStreamWorker();

  /// CONTROL THREAD: register a source for periodic servicing.
  void attach(const std::shared_ptr<StreamingClipSource>& source);

private:
  void run();

  std::mutex m_mutex;
  std::condition_variable m_wake;
  std::vector<std::weak_ptr<StreamingClipSource>> m_sources;
  bool m_stop = false;
  std::thread m_thread;
};

} // namespace orpheus
