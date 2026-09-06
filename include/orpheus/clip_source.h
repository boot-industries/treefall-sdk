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
//    Accepted transport seeks synchronously pin their first-render working
//    pages on the control thread before their command reaches the audio thread.
//
// Threading model (strict ownership handoff — no locks on the audio thread,
// TSAN-clean by construction):
//  * A page is FREE (start == -1) or READY (start >= 0). Its guard is zero
//    when it may be claimed, positive while a command reservation pins it, and
//    UINT32_MAX only while the worker fills or the audio thread retires it.
//    Worker fills and audio retirement claim zero guards before mutating start;
//    READY command pins prevent retirement between control preparation and
//    render consumption.
//  * The audio thread communicates its read position via a relaxed atomic
//    (setDemand); the worker polls it. Unprimed reposition/cache misses remain
//    non-blocking false returns. Command preparation owns reader access and
//    page publication before the command can be consumed.
//  * While attached to a MediaStreamWorker, the worker is the SOLE decoder:
//    command priming (primeForCommand) and prefill publish a demand request
//    and wait for the worker to fill it (bounded by kFillWaitTimeout) instead
//    of decoding directly. m_readerMutex serializes decode only in the
//    unattached direct-drive path (unit tests) and the wait-timeout fallback.
//
// Reads are position-explicit (no shared file cursor), which also makes
// multi-voice playback of one clip correct by construction — voices no
// longer fight over a single reader's seek position.

#include <orpheus/audio_file_reader.h>

#include <array>
#include <atomic>
#include <chrono>
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
/// demand position plus the demand page and four pages ahead. Forward playback
/// keeps ~8.2 s of lookahead; reverse/scrub playback keeps ~1.4 s of runway
/// behind the cursor, so position-explicit reads serve true backward playback
/// (descending positions) without a miss at every backward page crossing.
class StreamingClipSource : public IClipSource {
public:
  static constexpr size_t kPageFrames = 65536; ///< frames per page (~1.4s @ 48k)
  static constexpr size_t kWindowPages = 6;    ///< 1 behind + demand + 4 ahead (≈8.2s @ 48k)
  static constexpr size_t kCommandPrimePages = 4;
  static constexpr size_t kNumPages = kWindowPages + kCommandPrimePages;
  static_assert(kNumPages <= 16, "PrimeReservation page mask must cover every page");

  struct PrimeReservation {
    uint16_t pageMask{0};
  };

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

  /// Pin every page covering [pos, pos + frames) before publishing a command.
  /// CONTROL THREAD ONLY. The operation is transactional: failure leaves no
  /// newly published page or pin in reservation. An existing reservation may
  /// be extended, but a request spanning more than kCommandPrimePages pages is
  /// rejected before mutation.
  /// Attached: waits for the worker to decode the missing pages (bounded by
  /// kFillWaitTimeout, then falls back to a synchronous decode). Unattached:
  /// synchronous decode.
  SessionGraphError primeForCommand(int64_t pos, size_t frames, PrimeReservation& reservation);

  /// CONTROL THREAD for rejected/unread-command cleanup, or AUDIO THREAD after
  /// the consuming render block. Releases one command-owned pin per set bit;
  /// fixed bounded atomic work with no allocation, locking, or I/O.
  void releaseCommandPrime(PrimeReservation reservation) noexcept;

  bool hasPendingCommandPrimes() const noexcept {
    return m_pendingCommandPrimes.load(std::memory_order_acquire) != 0;
  }

  /// Fill up to `max_pages` non-resident pages of the steady worker window —
  /// the demand page first, then forward pages, then behind. The audible page
  /// is mandatory; later look-ahead pages are best effort. When
  /// `commandReservation` is supplied, the mandatory page is pinned as a
  /// command-owned page so a caller publishing a command can use command-prime
  /// capacity when the steady window is full.
  /// Attached: waits for the worker to fill the window (bounded by
  /// kFillWaitTimeout, then falls back to a synchronous fill). Unattached:
  /// synchronous fill. CONTROL/WORKER THREAD.
  SessionGraphError prefill(int64_t pos, size_t max_pages = kWindowPages,
                            PrimeReservation* commandReservation = nullptr);

  /// One worker pass: refill FREE pages inside the current demand window.
  /// WORKER THREAD ONLY.
  void service();

  /// One worker pass: fill every page demanded by a pending command prime,
  /// pin them as command-owned, and publish the completion mask. WORKER
  /// THREAD ONLY.
  void serviceCommandDemand();

private:
  friend class MediaStreamWorker; // attach() sets m_attached; serviceCommandDemand() fills command primes

  static constexpr uint32_t kClaimed = UINT32_MAX;
  static constexpr std::chrono::milliseconds kFillWaitTimeout{2000};

  struct Page {
    // -1 == FREE, >= 0 == READY at that aligned frame. A nonzero guard makes
    // a page immutable: positive values are command pins; kClaimed is the
    // transient worker-fill/audio-retirement claim.
    std::atomic<int64_t> start{-1};
    std::atomic<uint32_t> guard{0};
    std::vector<float> data; // kPageFrames * numChannels, pre-allocated
  };

  static int64_t alignDown(int64_t pos) {
    return (pos / static_cast<int64_t>(kPageFrames)) * static_cast<int64_t>(kPageFrames);
  }

  /// Copy the subrange [pos, pos+frames) from a READY page covering it.
  /// Returns false when no resident page covers pos.
  bool copyFromResidentPage(int64_t pos, float* dest, size_t frames);

  /// Decode into an exclusively claimed page without publishing it. The caller
  /// holds m_readerMutex and owns page.guard.
  SessionGraphError decodePage(Page& page, int64_t alignedStart);

  /// Fill one FREE steady-window page and publish it. Worker/control thread.
  SessionGraphError fillPage(int64_t alignedStart);

  /// Fill up to `max_pages` pages of the steady window at `base` — demand page
  /// first, then forward pages, then behind. Caller holds m_readerMutex.
  SessionGraphError fillWindow(int64_t base, size_t max_pages);

  /// Decode the missing command pages into FREE command-pool pages (indices
  /// kWindowPages..kNumPages-1), pin them, and set the matching claimedMask
  /// bits. Caller holds m_readerMutex.
  SessionGraphError decodeMissingIntoCommandPool(
      const std::array<int64_t, kCommandPrimePages>& missingPages, size_t missingCount,
      uint16_t& claimedMask);

  /// Undo exactly one READY-page pin acquired by a failed command-prime scan.
  void releaseReadyPin(Page& page) noexcept;

  std::shared_ptr<IAudioFileReader> m_reader; // worker/control threads only
  std::mutex m_readerMutex;                   // serializes prefill vs service
  std::mutex m_fillMutex;                     // control-thread wait handshake
  std::condition_variable m_fillCv;           // worker notifies after each pass
  std::atomic<int64_t> m_demand{0};
  std::atomic<uint32_t> m_pendingCommandPrimes{0};
  std::atomic<bool> m_attached{false};                  // set by MediaStreamWorker::attach
  std::atomic<int64_t> m_commandDemandStart{-1};        // pending command fill request
  std::atomic<int64_t> m_commandDemandFrames{0};
  std::atomic<uint32_t> m_commandFillState{0};          // 0 idle, 1 done-ok, 2 done-failed
  std::atomic<uint16_t> m_commandFillMask{0};           // worker-pinned pages (state==1)
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
