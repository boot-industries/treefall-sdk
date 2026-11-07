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
//    when it may be claimed, positive while command-owned or transient read
//    pins protect it, and UINT32_MAX only while the worker fills or the audio
//    thread retires it. Worker fills and audio retirement claim zero guards
//    before mutating start; pins prevent mutation through render consumption.
//  * The audio thread publishes a relaxed fallback position and a bounded,
//    multi-buffered set of active-source voice cursors. The worker copies a
//    pinned coherent snapshot; a busy snapshot defers one bounded service pass.
//    Unprimed cache misses remain nonblocking false returns. Command
//    preparation owns reader access and page publication before consumption.
//  * While attached to a MediaStreamWorker, command priming and prefill request
//    worker service. The 2 s demand wait does not bound cancellation
//    acknowledgement, synchronous reader calls, or worker teardown. A blocked
//    reader blocks service of all sources; no hard cancellation is provided.
//    Reader access remains serialized during the acknowledged-timeout fallback.
//
// Reads are position-explicit (no shared file cursor). The transport publishes
// all active voice cursors for each source before rendering, so overlapping
// voices retain independent current/lookahead windows within fixed page budgets.

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
  ///         contents unspecified) — the caller renders silence and reports an
  ///         underrun. The transport's render path conceals the resident prefix of a
  ///         missed streaming block itself.
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
  /// Decode exactly `lengthFrames` engine-rate frames before publication.
  /// BACKGROUND/CONTROL THREAD ONLY. Returns nullptr on seek/read failure or
  /// any short read before the advertised length; never pads truncated media.
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
/// Six steady pages are shared across distinct page-aligned voice cursors;
/// voices whose cursors sit on the same page share one window. A single
/// cursor retains one page behind, its current page and four forward pages;
/// two distinct cursors receive behind/current/forward windows of three pages
/// each. More distinct cursors reduce runway and can exceed this fixed cache
/// budget.
/// Four separate command-prime pages retain first-render and loop-anchor leases.
class StreamingClipSource : public IClipSource {
public:
  static constexpr size_t kPageFrames = 65536; ///< frames per page (~1.4s @ 48k)
  static constexpr size_t kWindowPages = 6;    ///< total steady-page budget per source
  static constexpr size_t kCommandPrimePages = 4;
  static constexpr size_t kNumPages = kWindowPages + kCommandPrimePages;
  static_assert(kNumPages <= 16, "PrimeReservation page mask must cover every page");

  struct PrimeReservation {
    uint16_t pageMask{0};
  };

  struct LoopAnchorTransition {
    uint16_t previousPageMask{0};
    int64_t previousStart{-1}; ///< first anchored page start, -1 when inactive
    size_t previousPageCount{0};
    uint16_t replacementPageMask{0};
    int64_t replacementStart{-1};
    size_t replacementPageCount{0};
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
  /// Attached: waits for worker service. The 2 s demand wait is followed by
  /// cancellation acknowledgement before synchronous fallback; neither reader
  /// calls nor acknowledgement have a hard time bound. Unattached: direct decode.
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
  /// Attached: waits up to 2 s for the requested window; look-ahead remains best
  /// effort. This wait does not bound a blocked reader or worker teardown.
  SessionGraphError prefill(int64_t pos, size_t max_pages = kWindowPages,
                            PrimeReservation* commandReservation = nullptr);

  /// One worker pass: refill active-voice windows, current pages first, within
  /// the fixed six-page steady budget. WORKER THREAD ONLY.
  void service();

  /// One worker pass: fill every page demanded by a pending command prime,
  /// pin them as command-owned, and publish the completion mask. WORKER
  /// THREAD ONLY.
  void serviceCommandDemand();

  /// Prepare an exact loop-anchor ownership transition, retaining the previous
  /// anchor's pins until commit. CONTROL THREAD ONLY; stages the next control
  /// anchor and owns transition exclusively until command admission. Never
  /// prepare into or otherwise mutate an admitted command's transition.
  ///
  /// The transition pins every page covering [pos, pos + frames), clamped to
  /// the clip, as the restart working set. When the command pool cannot hold
  /// that set (NotReady) it pins only the page holding `pos`; a later prepare
  /// retries the full set. `enabled == false` stages release of the set.
  SessionGraphError prepareLoopAnchorTransition(int64_t pos, size_t frames, bool enabled,
                                                LoopAnchorTransition& transition);

  /// Commit a prepared loop-anchor transition under exclusive ownership.
  /// AUDIO THREAD SAFE after ownership transfers through an admitted command:
  /// bounded atomic-only work releases the previous pin at command execution,
  /// not enqueue, and clears only command-local transition state. Control-side
  /// source setup may commit before publication while it owns transition.
  void commitLoopAnchorTransition(LoopAnchorTransition& transition) noexcept;

  /// Roll back a prepared transition before ownership transfers to an admitted
  /// command. CONTROL THREAD ONLY; restores the prior anchor and releases the
  /// replacement pin. Discarded admitted commands may be drained on control
  /// only after audio quiescence has returned exclusive ownership.
  void rollbackLoopAnchorTransition(LoopAnchorTransition& transition) noexcept;

private:
  friend class MediaStreamWorker; // attach() sets m_attached; worker fills command primes
  friend class StreamingClipSourceTestAccess;
  friend class TransportController;

  // One voice's cursor and the program it will follow: where its current playback window ends and
  // the window start it jumps to next (-1 when playback ends there). Read-ahead and retention
  // follow this program, not the file order.
  struct VoiceDemand {
    int64_t position{0};
    int64_t windowEnd{0};   ///< exclusive end of the voice's current playback window
    int64_t jumpTarget{-1}; ///< next window start after windowEnd; -1 when playback ends there
  };

  // The worker receives one coherent bounded snapshot of every active voice
  // cursor for this source. The six steady pages are budgeted per distinct
  // page-aligned cursor, not per voice: they support two three-page windows,
  // three two-page windows, or up to six current pages. More distinct cursors
  // are a real cache-capacity limit, not a polyphony guarantee.
  static constexpr size_t kMaxVoiceDemands = 32;
  static constexpr uint32_t kVoiceDemandWriter = uint32_t{1} << 31;
  // One active view, possible worker/control readers, and an inactive writer slot.
  static constexpr uint32_t kVoiceDemandBufferCount = 4;
  void beginVoiceDemands() noexcept;
  size_t addVoiceDemand(int64_t position, int64_t windowEnd, int64_t jumpTarget) noexcept;
  void publishVoiceDemands() noexcept;
  void refreshVoicePagePins() noexcept;
  bool readForVoice(size_t demandIndex, int64_t pos, int64_t windowEnd, int64_t jumpTarget,
                    float* dest, size_t frames, size_t& framesRead);
  bool readCached(int64_t pos, float* dest, size_t frames, size_t& framesRead);
  bool keepsPage(int64_t pageStart, int64_t fallbackPosition) const noexcept;

  /// Budget the steady pages per distinct page-aligned cursor: voices on the same page share one
  /// window, and windows rank by first appearance. Audio-side retention and the worker refill both
  /// derive their windows here from the same published demands. Realtime-safe.
  static void computeVoiceWindowPages(const VoiceDemand* demands, size_t count,
                                      std::array<uint8_t, kMaxVoiceDemands>& pages) noexcept;

  /// The pages one voice reads, in order: `linear` consecutive pages from `current`, then, when
  /// the program jumps, consecutive pages from `hop`.
  struct ProgramShape {
    int64_t current{0};
    size_t linear{1};
    int64_t hop{-1}; ///< first page after the jump, -1 when playback ends at the window
  };
  ProgramShape programShape(const VoiceDemand& demand) const noexcept;

  /// Start of the `ahead`-th page a voice reads, or -1 past the program end or the clip.
  int64_t programPageStart(const VoiceDemand& demand, size_t ahead) const noexcept;

  struct VoiceDemandSnapshot {
    std::array<VoiceDemand, kMaxVoiceDemands> demands{};
    size_t count{0};
  };
  struct VoiceDemandBuffer {
    // The writer claims an inactive buffer; the worker increments this state
    // before copying. Payload stores precede the writer's release publication
    // of the active index, which the worker acquires before reading. The
    // reader's pin excludes overwrite until its release decrement; the next
    // writer's acquire claim therefore follows all prior plain-payload reads.
    // This is a bounded pin protocol, not a seqlock over concurrently accessed
    // non-atomic fields.
    std::atomic<uint32_t> access{0};
    std::array<VoiceDemand, kMaxVoiceDemands> demands{};
    size_t count{0};
  };
  bool readWorkerVoiceDemands(VoiceDemandSnapshot& snapshot) noexcept;

  static constexpr uint32_t kClaimed = UINT32_MAX;
  static constexpr std::chrono::milliseconds kFillWaitTimeout{2000};

  enum class CommandFillState : uint8_t {
    Idle,
    Pending,
    Filling,
    Succeeded,
    Failed,
    CancelRequested,
    Cancelled
  };

  struct CommandFillRequest {
    uint64_t generation{0};
    int64_t start{-1};
    int64_t frames{0};
    CommandFillState state{CommandFillState::Idle};
    uint16_t residentPinMask{0};
    uint16_t freshPageMask{0};
  };

  // Worker-local transaction state: not stored in the source, so its layout and
  // public service entry points remain unchanged. Fresh pages stay unpublished
  // across rounds until the complete command working set succeeds.
  struct CommandFillWork {
    uint64_t generation{0};
    int64_t firstPage{0};
    size_t pageCount{0};
    size_t pageOffset{0};
    uint16_t initialResidentMask{0};
    uint16_t workerResidentMask{0};
    uint16_t freshPageMask{0};
    std::array<int64_t, kNumPages> freshStarts{};
  };

  bool serviceCommandDemandPage(CommandFillWork& work);
  void finishCommandFillFailure(CommandFillWork& work, CommandFillState state);
  void serviceWindow(size_t pageOpportunities);

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
  bool acquireTransientReadPin(Page& page, int64_t expectedStart) noexcept;

  /// Decode into an exclusively claimed page without publishing it. The caller
  /// holds m_readerMutex and owns page.guard.
  SessionGraphError decodePage(Page& page, int64_t alignedStart);

  /// Fill one FREE steady-window page and publish it. Worker/control thread.
  SessionGraphError fillPage(int64_t alignedStart);

  /// Refill one source's active-voice windows in rotating current-page-first
  /// order. The normal worker round supplies one page opportunity.
  void serviceVoiceWindow(const VoiceDemandSnapshot& snapshot, size_t pageOpportunities);

  /// Fill the fallback single-cursor window. Audible demand is mandatory;
  /// look-ahead is best effort, within the caller's page opportunity budget.
  SessionGraphError fillWindow(int64_t base, size_t max_pages,
                               size_t pageOpportunities = kWindowPages);

  /// Decode the missing command pages into FREE command-pool pages (indices
  /// kWindowPages..kNumPages-1), pin them, and set the matching claimedMask
  /// bits. Caller holds m_readerMutex.
  SessionGraphError
  decodeMissingIntoCommandPool(const std::array<int64_t, kCommandPrimePages>& missingPages,
                               size_t missingCount, uint16_t& claimedMask);

  /// Release exactly one caller-owned READY-page pin with bounded atomic work.
  void releaseReadyPin(Page& page) noexcept;

  /// Release one pin on every page whose bit is set in `mask`.
  void releasePinMask(uint16_t mask) noexcept;

  std::shared_ptr<IAudioFileReader> m_reader; // worker/control threads only
  std::mutex m_readerMutex;                   // serializes prefill vs service
  std::mutex m_fillMutex;                     // protects the command-fill request transaction
  std::condition_variable m_fillCv;
  std::atomic<int64_t> m_demand{0};
  // Audio-thread-owned cursor list; worker threads consume only the pinned
  // immutable multi-buffer snapshot below.
  size_t m_voiceDemandCount{0};
  std::array<VoiceDemand, kMaxVoiceDemands> m_voiceDemands{};
  // Audio-thread-owned per-voice steady-page budget, derived at publication
  // from the same positions the worker snapshot carries, so retention and
  // refill follow one plan.
  std::array<uint8_t, kMaxVoiceDemands> m_voiceWindowPlan{};
  std::array<VoiceDemandBuffer, kVoiceDemandBufferCount> m_voiceDemandBuffers{};
  std::atomic<uint32_t> m_activeVoiceDemandBuffer{0};
  size_t m_voiceDemandServiceCursor{0}; // worker-thread only
  uint16_t m_voiceDemandPagePinMask{0}; // audio-owned command-pool demand leases
  std::atomic<uint32_t> m_pendingCommandPrimes{0};
  std::atomic<bool> m_attached{false}; // set by MediaStreamWorker::attach
  uint64_t m_nextCommandFillGeneration{1};
  CommandFillRequest m_commandFillRequest{};
  uint16_t m_loopAnchorPageMask{0};
  int64_t m_loopAnchorStart{-1};
  size_t m_loopAnchorPageCount{0};
  uint16_t m_numChannels;
  int64_t m_lengthFrames;
  Page m_pages[kNumPages];
};

/// Background thread servicing every StreamingClipSource of a transport.
/// Weak registrations are identity-deduplicated and expired entries are pruned.
/// Each round gives a source at most one page opportunity, with command demand
/// before look-ahead and a rotating first source. Synchronous reader calls can
/// block the whole worker and its destructor; this is not hard cancellation.
class MediaStreamWorker {
public:
  MediaStreamWorker();
  ~MediaStreamWorker();

  /// CONTROL THREAD: register a source once by identity; repeated attach is a no-op.
  void attach(const std::shared_ptr<StreamingClipSource>& source);

private:
  friend class StreamingClipSourceTestAccess;

  struct SourceService {
    std::shared_ptr<StreamingClipSource> source;
    StreamingClipSource::CommandFillWork command;
    bool commandServiced{false};
  };
  struct PreviousSource {
    const StreamingClipSource* identity;
    StreamingClipSource::CommandFillWork command;
  };

  // Caller holds m_mutex. Active transactions retain their source until their
  // claims/pins are published or rolled back; idle snapshots own no registration.
  void snapshotSources(std::vector<SourceService>& live, std::vector<SourceService>& next,
                       std::vector<PreviousSource>& previous);
  static void serviceRound(std::vector<SourceService>& live, size_t& cursor);
  void run();

  std::mutex m_mutex;
  std::condition_variable m_wake;
  std::vector<std::weak_ptr<StreamingClipSource>> m_sources;
  bool m_stop = false;
  std::thread m_thread;
};

} // namespace orpheus
