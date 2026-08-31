// SPDX-License-Identifier: MIT
#include <orpheus/clip_source.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstring>

namespace orpheus {

// ============================================================================
// PreparedClipSource
// ============================================================================

PreparedClipSource::PreparedClipSource(std::vector<float> pcm, uint16_t numChannels,
                                       int64_t lengthFrames)
    : m_pcm(std::move(pcm)), m_numChannels(numChannels), m_lengthFrames(lengthFrames) {}

std::shared_ptr<PreparedClipSource>
PreparedClipSource::decode(IAudioFileReader& reader, uint16_t numChannels, int64_t lengthFrames) {
  if (numChannels == 0 || lengthFrames < 0) {
    return nullptr;
  }

  std::vector<float> pcm(static_cast<size_t>(lengthFrames) * numChannels, 0.0f);

  if (reader.seek(0) != SessionGraphError::OK) {
    return nullptr;
  }

  // Chunked sequential decode (background thread; blocking I/O is fine here).
  constexpr size_t kChunkFrames = 65536;
  int64_t framesDecoded = 0;
  while (framesDecoded < lengthFrames) {
    const size_t want = static_cast<size_t>(
        std::min<int64_t>(static_cast<int64_t>(kChunkFrames), lengthFrames - framesDecoded));
    float* dest = pcm.data() + static_cast<size_t>(framesDecoded) * numChannels;
    auto result = reader.readSamples(dest, want);
    if (!result.isOk()) {
      return nullptr;
    }
    if (result.value == 0) {
      break; // EOF earlier than metadata promised — keep zero padding
    }
    framesDecoded += static_cast<int64_t>(result.value);
  }

  return std::shared_ptr<PreparedClipSource>(
      new PreparedClipSource(std::move(pcm), numChannels, lengthFrames));
}

bool PreparedClipSource::read(int64_t pos, float* dest, size_t frames, size_t& framesRead) {
  framesRead = 0;
  if (pos < 0 || pos >= m_lengthFrames) {
    return true; // valid read of zero frames (EOF / out of range)
  }
  const int64_t available = m_lengthFrames - pos;
  const size_t toCopy =
      static_cast<size_t>(std::min<int64_t>(static_cast<int64_t>(frames), available));
  std::memcpy(dest, m_pcm.data() + static_cast<size_t>(pos) * m_numChannels,
              toCopy * m_numChannels * sizeof(float));
  framesRead = toCopy;
  return true;
}

// ============================================================================
// StreamingClipSource
// ============================================================================

StreamingClipSource::StreamingClipSource(std::shared_ptr<IAudioFileReader> reader,
                                         uint16_t numChannels, int64_t lengthFrames)
    : m_reader(std::move(reader)), m_numChannels(numChannels), m_lengthFrames(lengthFrames) {
  for (auto& page : m_pages) {
    page.data.assign(kPageFrames * m_numChannels, 0.0f);
  }
}

bool StreamingClipSource::copyFromResidentPage(int64_t pos, float* dest, size_t frames) {
  const int64_t pageStart = alignDown(pos);
  for (auto& page : m_pages) {
    if (page.start.load(std::memory_order_acquire) == pageStart) {
      const size_t offset = static_cast<size_t>(pos - pageStart);
      std::memcpy(dest, page.data.data() + offset * m_numChannels,
                  frames * m_numChannels * sizeof(float));
      return true;
    }
  }
  return false;
}

void StreamingClipSource::releaseReadyPin(Page& page) noexcept {
  uint32_t observed = page.guard.load(std::memory_order_acquire);
  while (true) {
    assert(observed > 0 && observed < kClaimed);
    if (observed == 0 || observed >= kClaimed) {
      return;
    }
    if (page.guard.compare_exchange_weak(observed, observed - 1, std::memory_order_acq_rel,
                                         std::memory_order_acquire)) {
      return;
    }
  }
}

bool StreamingClipSource::read(int64_t pos, float* dest, size_t frames, size_t& framesRead) {
  framesRead = 0;
  if (pos < 0 || pos >= m_lengthFrames) {
    return true; // valid read of zero frames
  }
  const int64_t available = m_lengthFrames - pos;
  const size_t toCopy =
      static_cast<size_t>(std::min<int64_t>(static_cast<int64_t>(frames), available));

  // Publish the demand position and retire every unpinned READY page outside
  // the demand window. Pinned command pages stay immutable until the command
  // drain's end-of-block release path hands their guard back to zero.
  m_demand.store(pos, std::memory_order_relaxed);
  const int64_t windowBase = alignDown(pos) - static_cast<int64_t>(kPageFrames);
  const int64_t windowEnd =
      alignDown(pos) + static_cast<int64_t>(kWindowPages - 1) * static_cast<int64_t>(kPageFrames);
  for (auto& page : m_pages) {
    const int64_t start = page.start.load(std::memory_order_acquire);
    if (start >= 0 && (start < windowBase || start >= windowEnd)) {
      uint32_t expected = 0;
      if (page.guard.compare_exchange_strong(expected, kClaimed, std::memory_order_acq_rel,
                                             std::memory_order_acquire)) {
        page.start.store(-1, std::memory_order_release);
        page.guard.store(0, std::memory_order_release);
      }
    }
  }

  // A transport buffer (<= 2048 frames) spans at most two 64k pages.
  size_t copied = 0;
  while (copied < toCopy) {
    const int64_t subPos = pos + static_cast<int64_t>(copied);
    const int64_t pageEnd = alignDown(subPos) + static_cast<int64_t>(kPageFrames);
    const size_t subFrames = static_cast<size_t>(
        std::min<int64_t>(static_cast<int64_t>(toCopy - copied), pageEnd - subPos));
    if (!copyFromResidentPage(subPos, dest + copied * m_numChannels, subFrames)) {
      return false; // genuine unprimed/failed cache miss
    }
    copied += subFrames;
  }

  framesRead = toCopy;
  return true;
}

SessionGraphError StreamingClipSource::decodePage(Page& page, int64_t alignedStart) {
  if (!m_reader || !m_reader->isOpen()) {
    return SessionGraphError::NotReady;
  }
  if (alignedStart < 0 || alignedStart >= m_lengthFrames) {
    return SessionGraphError::InvalidParameter;
  }

  const size_t want = static_cast<size_t>(
      std::min<int64_t>(static_cast<int64_t>(kPageFrames), m_lengthFrames - alignedStart));
  if (want == 0) {
    return SessionGraphError::InvalidParameter;
  }

  const SessionGraphError seekResult = m_reader->seek(alignedStart);
  if (seekResult != SessionGraphError::OK) {
    return seekResult;
  }
  const auto readResult = m_reader->readSamples(page.data.data(), want);
  if (!readResult.isOk()) {
    return readResult.error;
  }
  if (readResult.value != want) {
    return SessionGraphError::InternalError;
  }

  if (want < kPageFrames) {
    std::fill(page.data.begin() + static_cast<ptrdiff_t>(want * m_numChannels), page.data.end(),
              0.0f);
  }
  return SessionGraphError::OK;
}

SessionGraphError StreamingClipSource::fillPage(int64_t alignedStart) {
  std::lock_guard<std::mutex> lock(m_readerMutex);
  if (alignedStart < 0 || alignedStart >= m_lengthFrames) {
    return SessionGraphError::InvalidParameter;
  }

  for (auto& page : m_pages) {
    if (page.start.load(std::memory_order_acquire) == alignedStart) {
      return SessionGraphError::OK;
    }
  }

  Page* target = nullptr;
  for (size_t index = 0; index < kWindowPages; ++index) {
    Page& page = m_pages[index];
    if (page.start.load(std::memory_order_acquire) != -1) {
      continue;
    }
    uint32_t expected = 0;
    if (page.guard.compare_exchange_strong(expected, kClaimed, std::memory_order_acq_rel,
                                           std::memory_order_acquire)) {
      target = &page;
      break;
    }
  }
  if (target == nullptr) {
    return SessionGraphError::NotReady;
  }

  const SessionGraphError result = decodePage(*target, alignedStart);
  if (result == SessionGraphError::OK) {
    target->start.store(alignedStart, std::memory_order_release);
  }
  target->guard.store(0, std::memory_order_release);
  return result;
}

SessionGraphError StreamingClipSource::primeForCommand(int64_t pos, size_t frames,
                                                       PrimeReservation& reservation) {
  std::lock_guard<std::mutex> lock(m_readerMutex);
  if (m_lengthFrames <= 0 || frames == 0) {
    return SessionGraphError::OK;
  }

  const int64_t rangeStart = std::clamp<int64_t>(pos, 0, m_lengthFrames);
  if (rangeStart == m_lengthFrames) {
    return SessionGraphError::OK;
  }
  const int64_t available = m_lengthFrames - rangeStart;
  const int64_t rangeFrames =
      frames > static_cast<size_t>(available) ? available : static_cast<int64_t>(frames);
  const int64_t firstPage = alignDown(rangeStart);
  const int64_t lastPage = alignDown(rangeStart + rangeFrames - 1);
  const size_t pageCount =
      static_cast<size_t>((lastPage - firstPage) / static_cast<int64_t>(kPageFrames) + 1);
  if (pageCount > kCommandPrimePages) {
    return SessionGraphError::InvalidParameter;
  }

  std::array<int64_t, kCommandPrimePages> requestedPages{};
  for (size_t index = 0; index < pageCount; ++index) {
    requestedPages[index] =
        firstPage + static_cast<int64_t>(index * static_cast<size_t>(kPageFrames));
  }

  struct ClaimedPage {
    Page* page{nullptr};
    int64_t start{-1};
  };
  std::array<ClaimedPage, kCommandPrimePages> claimedPages{};
  size_t claimedCount = 0;
  const uint8_t originalMask = reservation.pageMask;
  uint8_t readyPinMask = 0;
  uint8_t claimedMask = 0;

  const auto reservationSize = [](uint8_t mask) noexcept {
    size_t count = 0;
    for (size_t index = 0; index < kNumPages; ++index) {
      count += (mask & static_cast<uint8_t>(uint8_t{1} << index)) != 0 ? 1u : 0u;
    }
    return count;
  };

  const auto rollback = [&]() noexcept {
    for (size_t index = 0; index < claimedCount; ++index) {
      claimedPages[index].page->guard.store(0, std::memory_order_release);
    }
    for (size_t index = 0; index < kNumPages; ++index) {
      const uint8_t bit = static_cast<uint8_t>(uint8_t{1} << index);
      if ((readyPinMask & bit) != 0) {
        releaseReadyPin(m_pages[index]);
      }
    }
  };

  for (size_t requestIndex = 0; requestIndex < pageCount; ++requestIndex) {
    const int64_t requestedStart = requestedPages[requestIndex];
    bool satisfied = false;
    bool exhaustedByContention = true;
    for (size_t scan = 0; scan < kNumPages; ++scan) {
      bool retry = false;
      bool foundReady = false;
      for (size_t pageIndex = 0; pageIndex < kNumPages; ++pageIndex) {
        Page& page = m_pages[pageIndex];
        const uint8_t bit = static_cast<uint8_t>(uint8_t{1} << pageIndex);
        if (page.start.load(std::memory_order_acquire) != requestedStart) {
          continue;
        }
        foundReady = true;
        if ((originalMask & bit) != 0) {
          satisfied = true;
          break;
        }

        if (reservationSize(static_cast<uint8_t>(originalMask | readyPinMask | claimedMask)) >=
            kCommandPrimePages) {
          rollback();
          return SessionGraphError::NotReady;
        }

        uint32_t expected = page.guard.load(std::memory_order_acquire);
        if (expected >= kClaimed - 1) {
          retry = true;
          break;
        }
        if (!page.guard.compare_exchange_strong(expected, expected + 1, std::memory_order_acq_rel,
                                                std::memory_order_acquire)) {
          retry = true;
          break;
        }
        if (page.start.load(std::memory_order_acquire) == requestedStart) {
          readyPinMask |= bit;
          satisfied = true;
          break;
        }

        // The audio thread retired the page between the initial load and the
        // pin CAS. Undo only this acquired pin, then restart the fixed scan.
        releaseReadyPin(page);
        retry = true;
        break;
      }
      if (satisfied) {
        break;
      }
      if (!foundReady) {
        exhaustedByContention = false;
        break;
      }
      if (!retry) {
        exhaustedByContention = false;
        break;
      }
    }
    if (satisfied) {
      continue;
    }
    if (exhaustedByContention) {
      rollback();
      return SessionGraphError::NotReady;
    }

    if (reservationSize(static_cast<uint8_t>(originalMask | readyPinMask | claimedMask)) >=
        kCommandPrimePages) {
      rollback();
      return SessionGraphError::NotReady;
    }

    Page* target = nullptr;
    for (size_t scan = 0; scan < kNumPages && target == nullptr; ++scan) {
      bool observedContention = false;
      for (size_t pageIndex = kWindowPages; pageIndex < kNumPages; ++pageIndex) {
        const uint8_t bit = static_cast<uint8_t>(uint8_t{1} << pageIndex);
        if ((claimedMask & bit) != 0) {
          continue;
        }
        Page& page = m_pages[pageIndex];
        if (page.start.load(std::memory_order_acquire) != -1) {
          continue;
        }
        uint32_t expected = 0;
        if (page.guard.compare_exchange_strong(expected, 1, std::memory_order_acq_rel,
                                               std::memory_order_acquire)) {
          target = &page;
          claimedMask |= bit;
          claimedPages[claimedCount++] = {target, requestedStart};
          break;
        }
        observedContention = true;
      }
      if (!observedContention) {
        break;
      }
    }
    if (target == nullptr) {
      rollback();
      return SessionGraphError::NotReady;
    }
  }

  for (size_t index = 0; index < claimedCount; ++index) {
    const SessionGraphError result =
        decodePage(*claimedPages[index].page, claimedPages[index].start);
    if (result != SessionGraphError::OK) {
      rollback();
      return result;
    }
  }
  for (size_t index = 0; index < claimedCount; ++index) {
    claimedPages[index].page->start.store(claimedPages[index].start, std::memory_order_release);
  }

  reservation.pageMask = static_cast<uint8_t>(originalMask | readyPinMask | claimedMask);
  if (originalMask == 0 && reservation.pageMask != 0) {
    m_pendingCommandPrimes.fetch_add(1, std::memory_order_release);
  }
  return SessionGraphError::OK;
}

void StreamingClipSource::releaseCommandPrime(PrimeReservation reservation) noexcept {
  if (reservation.pageMask == 0) {
    return;
  }
  for (size_t index = 0; index < kNumPages; ++index) {
    const uint8_t bit = static_cast<uint8_t>(uint8_t{1} << index);
    if ((reservation.pageMask & bit) == 0) {
      continue;
    }
    const uint32_t observed = m_pages[index].guard.load(std::memory_order_acquire);
    assert(observed > 0 && observed < kClaimed);
    if (observed == 0 || observed >= kClaimed) {
      continue;
    }
    const uint32_t previous = m_pages[index].guard.fetch_sub(1, std::memory_order_acq_rel);
    assert(previous > 0 && previous < kClaimed);
    static_cast<void>(previous);
  }
  const uint32_t pending = m_pendingCommandPrimes.load(std::memory_order_acquire);
  assert(pending > 0);
  if (pending != 0) {
    m_pendingCommandPrimes.fetch_sub(1, std::memory_order_release);
  }
}

SessionGraphError StreamingClipSource::prefill(int64_t pos, size_t max_pages,
                                               PrimeReservation* commandReservation) {
  if (m_lengthFrames <= 0 || pos >= m_lengthFrames) {
    return SessionGraphError::OK;
  }
  if (max_pages == 0) {
    return SessionGraphError::NotReady;
  }
  const int64_t base = alignDown(std::max<int64_t>(0, pos));

  // A command publisher must pin the page the first render will consume. This
  // path deliberately uses the transactional command-prime lease rather than
  // allowing a full steady window to turn preparation into NotReady.
  size_t filled = 0;
  if (commandReservation != nullptr) {
    const SessionGraphError result = primeForCommand(base, 1, *commandReservation);
    if (result != SessionGraphError::OK) {
      return result;
    }
    filled = 1;
  }

  // Fill order: audible demand, forward look-ahead, then reverse runway.
  std::array<int64_t, kWindowPages> wanted{};
  size_t wantedCount = 0;
  for (size_t index = 0; index + 1 < kWindowPages; ++index) {
    wanted[wantedCount++] = base + static_cast<int64_t>(index * static_cast<size_t>(kPageFrames));
  }
  wanted[wantedCount++] = base - static_cast<int64_t>(kPageFrames);

  for (size_t index = 0; index < wantedCount && filled < max_pages; ++index) {
    const int64_t start = wanted[index];
    if (start < 0 || start >= m_lengthFrames) {
      continue;
    }
    if (commandReservation != nullptr && index == 0) {
      continue;
    }
    bool resident = false;
    for (auto& page : m_pages) {
      if (page.start.load(std::memory_order_acquire) == start) {
        resident = true;
        break;
      }
    }
    if (resident) {
      continue;
    }

    const SessionGraphError result = fillPage(start);
    if (result != SessionGraphError::OK) {
      if (index == 0 && commandReservation == nullptr) {
        return result;
      }
      continue; // look-ahead is deliberately best effort
    }
    ++filled;
  }
  return SessionGraphError::OK;
}

void StreamingClipSource::service() {
  (void)prefill(m_demand.load(std::memory_order_relaxed));
}

// ============================================================================
// MediaStreamWorker
// ============================================================================

MediaStreamWorker::MediaStreamWorker() : m_thread([this]() { run(); }) {}

MediaStreamWorker::~MediaStreamWorker() {
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stop = true;
  }
  m_wake.notify_all();
  if (m_thread.joinable()) {
    m_thread.join();
  }
}

void MediaStreamWorker::attach(const std::shared_ptr<StreamingClipSource>& source) {
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sources.push_back(source);
  }
  m_wake.notify_all();
}

void MediaStreamWorker::run() {
  std::unique_lock<std::mutex> lock(m_mutex);
  while (!m_stop) {
    // Snapshot live sources, then service them without holding the lock so
    // attach() never blocks behind file I/O.
    std::vector<std::shared_ptr<StreamingClipSource>> live;
    live.reserve(m_sources.size());
    for (auto it = m_sources.begin(); it != m_sources.end();) {
      if (auto strong = it->lock()) {
        live.push_back(std::move(strong));
        ++it;
      } else {
        it = m_sources.erase(it);
      }
    }

    lock.unlock();
    for (auto& source : live) {
      source->service();
    }
    live.clear();
    lock.lock();

    // The audio thread cannot notify (no locks there); poll. 10ms against a
    // multi-second resident window keeps refill latency negligible.
    m_wake.wait_for(lock, std::chrono::milliseconds(10), [this]() { return m_stop; });
  }
}

} // namespace orpheus
