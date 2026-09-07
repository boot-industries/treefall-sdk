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

SessionGraphError StreamingClipSource::decodeMissingIntoCommandPool(
    const std::array<int64_t, kCommandPrimePages>& missingPages, size_t missingCount,
    uint16_t& claimedMask) {
  claimedMask = 0;
  struct ClaimedPage {
    Page* page{nullptr};
    int64_t start{-1};
  };
  std::array<ClaimedPage, kCommandPrimePages> claimedPages{};
  size_t claimedCount = 0;

  const auto releaseClaims = [&]() noexcept {
    for (size_t index = 0; index < claimedCount; ++index) {
      claimedPages[index].page->start.store(-1, std::memory_order_release);
      claimedPages[index].page->guard.store(0, std::memory_order_release);
    }
  };

  for (size_t missingIndex = 0; missingIndex < missingCount; ++missingIndex) {
    const int64_t requestedStart = missingPages[missingIndex];
    Page* target = nullptr;
    size_t targetIndex = kNumPages;
    for (size_t pageIndex = kWindowPages; pageIndex < kNumPages; ++pageIndex) {
      Page& page = m_pages[pageIndex];
      const int64_t existingStart = page.start.load(std::memory_order_acquire);
      if (existingStart != -1) {
        const int64_t demand =
            alignDown(std::max<int64_t>(0, m_demand.load(std::memory_order_relaxed)));
        const int64_t windowBase = demand - static_cast<int64_t>(kPageFrames);
        const int64_t windowEnd =
            demand + static_cast<int64_t>(kWindowPages - 1) * static_cast<int64_t>(kPageFrames);
        if (existingStart >= windowBase && existingStart < windowEnd) {
          continue;
        }
      }
      uint32_t expected = 0;
      if (page.guard.compare_exchange_strong(expected, kClaimed, std::memory_order_acq_rel,
                                             std::memory_order_acquire)) {
        page.start.store(-1, std::memory_order_release);
        target = &page;
        targetIndex = pageIndex;
        claimedPages[claimedCount++] = {target, requestedStart};
        claimedMask |= static_cast<uint16_t>(uint16_t{1} << targetIndex);
        break;
      }
    }
    if (target == nullptr) {
      releaseClaims();
      claimedMask = 0;
      return SessionGraphError::NotReady;
    }
  }

  for (size_t index = 0; index < claimedCount; ++index) {
    const SessionGraphError result =
        decodePage(*claimedPages[index].page, claimedPages[index].start);
    if (result != SessionGraphError::OK) {
      releaseClaims();
      claimedMask = 0;
      return result;
    }
  }
  for (size_t index = 0; index < claimedCount; ++index) {
    claimedPages[index].page->start.store(claimedPages[index].start, std::memory_order_release);
    claimedPages[index].page->guard.store(1, std::memory_order_release);
  }
  return SessionGraphError::OK;
}

SessionGraphError StreamingClipSource::primeForCommand(int64_t pos, size_t frames,
                                                       PrimeReservation& reservation) {
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

  const uint16_t originalMask = reservation.pageMask;
  uint16_t readyPinMask = 0;
  uint16_t claimedMask = 0;

  const auto reservationSize = [](uint16_t mask) noexcept {
    size_t count = 0;
    for (size_t index = 0; index < kNumPages; ++index) {
      count += (mask & static_cast<uint16_t>(uint16_t{1} << index)) != 0 ? 1u : 0u;
    }
    return count;
  };

  const auto rollback = [&]() noexcept {
    for (size_t index = 0; index < kNumPages; ++index) {
      const uint16_t bit = static_cast<uint16_t>(uint16_t{1} << index);
      if ((readyPinMask & bit) != 0) {
        releaseReadyPin(m_pages[index]);
      }
    }
  };

  const auto commit = [&](uint16_t mask) -> SessionGraphError {
    reservation.pageMask = mask;
    if (originalMask == 0 && reservation.pageMask != 0) {
      m_pendingCommandPrimes.fetch_add(1, std::memory_order_release);
    }
    return SessionGraphError::OK;
  };

  // Phase one only acquires pins for pages that are already resident. The
  // worker receives these bits so it can distinguish them from late residents
  // it pins itself and preserve their page contents during rollback.
  std::array<int64_t, kCommandPrimePages> missingPages{};
  size_t missingCount = 0;
  for (size_t requestIndex = 0; requestIndex < pageCount; ++requestIndex) {
    const int64_t requestedStart = requestedPages[requestIndex];
    bool satisfied = false;
    bool exhaustedByContention = true;
    for (size_t scan = 0; scan < kNumPages; ++scan) {
      bool retry = false;
      bool foundReady = false;
      for (size_t pageIndex = 0; pageIndex < kNumPages; ++pageIndex) {
        Page& page = m_pages[pageIndex];
        const uint16_t bit = static_cast<uint16_t>(uint16_t{1} << pageIndex);
        if (page.start.load(std::memory_order_acquire) != requestedStart) {
          continue;
        }
        foundReady = true;
        if ((originalMask & bit) != 0) {
          satisfied = true;
          break;
        }

        if (reservationSize(static_cast<uint16_t>(originalMask | readyPinMask | claimedMask)) >=
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
    missingPages[missingCount++] = requestedStart;
  }

  if (missingCount == 0) {
    return commit(static_cast<uint16_t>(originalMask | readyPinMask));
  }
  if (reservationSize(static_cast<uint16_t>(originalMask | readyPinMask)) + missingCount >
      kCommandPrimePages) {
    rollback();
    return SessionGraphError::NotReady;
  }

  if (!m_attached.load(std::memory_order_acquire)) {
    std::lock_guard<std::mutex> lock(m_readerMutex);
    const SessionGraphError result =
        decodeMissingIntoCommandPool(missingPages, missingCount, claimedMask);
    if (result != SessionGraphError::OK) {
      rollback();
      return result;
    }
    return commit(static_cast<uint16_t>(originalMask | readyPinMask | claimedMask));
  }

  const int64_t firstMissing = missingPages[0];
  const int64_t lastMissing = missingPages[missingCount - 1];
  std::unique_lock<std::mutex> lock(m_fillMutex);
  if (m_commandFillRequest.state != CommandFillState::Idle) {
    lock.unlock();
    rollback();
    return SessionGraphError::NotReady;
  }

  const uint64_t generation = m_nextCommandFillGeneration++;
  m_commandFillRequest = {};
  m_commandFillRequest.generation = generation;
  m_commandFillRequest.start = firstMissing;
  m_commandFillRequest.frames = lastMissing - firstMissing + static_cast<int64_t>(kPageFrames);
  m_commandFillRequest.state = CommandFillState::Pending;
  m_commandFillRequest.residentPinMask = readyPinMask;
  m_commandFillRequest.freshPageMask = 0;
  m_fillCv.notify_all();

  const auto deadline = std::chrono::steady_clock::now() + kFillWaitTimeout;
  const bool completed = m_fillCv.wait_until(lock, deadline, [&]() {
    return m_commandFillRequest.generation != generation ||
           m_commandFillRequest.state == CommandFillState::Succeeded ||
           m_commandFillRequest.state == CommandFillState::Failed ||
           m_commandFillRequest.state == CommandFillState::Cancelled;
  });
  static_cast<void>(completed);

  if (m_commandFillRequest.generation != generation) {
    lock.unlock();
    rollback();
    return SessionGraphError::NotReady;
  }

  const auto adoptSucceeded = [&]() -> SessionGraphError {
    const uint16_t workerMask = static_cast<uint16_t>(m_commandFillRequest.residentPinMask |
                                                      m_commandFillRequest.freshPageMask);
    m_commandFillRequest = {};
    lock.unlock();
    return commit(static_cast<uint16_t>(originalMask | workerMask));
  };

  if (m_commandFillRequest.state == CommandFillState::Succeeded) {
    return adoptSucceeded();
  }
  if (m_commandFillRequest.state == CommandFillState::Failed ||
      m_commandFillRequest.state == CommandFillState::Cancelled) {
    m_commandFillRequest = {};
    lock.unlock();
    rollback();
    return SessionGraphError::NotReady;
  }

  // Timeout is an ownership fence. A worker that has started decoding must
  // acknowledge cancellation before the control thread can touch the reader.
  if (m_commandFillRequest.state == CommandFillState::Pending) {
    m_commandFillRequest.state = CommandFillState::Cancelled;
    m_fillCv.notify_all();
  } else if (m_commandFillRequest.state == CommandFillState::Filling) {
    m_commandFillRequest.state = CommandFillState::CancelRequested;
    m_fillCv.notify_all();
    m_fillCv.wait(lock, [&]() {
      return m_commandFillRequest.generation != generation ||
             m_commandFillRequest.state == CommandFillState::Succeeded ||
             m_commandFillRequest.state == CommandFillState::Failed ||
             m_commandFillRequest.state == CommandFillState::Cancelled;
    });
    if (m_commandFillRequest.generation != generation) {
      lock.unlock();
      rollback();
      return SessionGraphError::NotReady;
    }
    if (m_commandFillRequest.state == CommandFillState::Succeeded) {
      return adoptSucceeded();
    }
    if (m_commandFillRequest.state == CommandFillState::Failed) {
      m_commandFillRequest = {};
      lock.unlock();
      rollback();
      return SessionGraphError::NotReady;
    }
  }

  // The worker has acknowledged cancellation (or never claimed the pending
  // request). Reset the transaction before taking the reader mutex for the
  // bounded synchronous fallback.
  m_commandFillRequest = {};
  lock.unlock();

  std::lock_guard<std::mutex> readerLock(m_readerMutex);
  const SessionGraphError result =
      decodeMissingIntoCommandPool(missingPages, missingCount, claimedMask);
  if (result != SessionGraphError::OK) {
    rollback();
    return result;
  }
  return commit(static_cast<uint16_t>(originalMask | readyPinMask | claimedMask));
}

void StreamingClipSource::releaseCommandPrime(PrimeReservation reservation) noexcept {
  if (reservation.pageMask == 0) {
    return;
  }
  for (size_t index = 0; index < kNumPages; ++index) {
    const uint16_t bit = static_cast<uint16_t>(uint16_t{1} << index);
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

SessionGraphError StreamingClipSource::fillWindow(int64_t base, size_t max_pages) {
  // Fill order: audible demand, forward look-ahead, then reverse runway.
  std::array<int64_t, kWindowPages> wanted{};
  size_t wantedCount = 0;
  for (size_t index = 0; index + 1 < kWindowPages; ++index) {
    wanted[wantedCount++] = base + static_cast<int64_t>(index * static_cast<size_t>(kPageFrames));
  }
  wanted[wantedCount++] = base - static_cast<int64_t>(kPageFrames);

  size_t filled = 0;
  for (size_t index = 0; index < wantedCount && filled < max_pages; ++index) {
    const int64_t start = wanted[index];
    if (start < 0 || start >= m_lengthFrames) {
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
      if (index == 0) {
        return result; // the audible demand page is mandatory
      }
      continue; // look-ahead is deliberately best effort
    }
    ++filled;
  }
  return SessionGraphError::OK;
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
  const auto primeDemand = [&]() -> SessionGraphError {
    if (commandReservation == nullptr) {
      return SessionGraphError::OK;
    }
    return primeForCommand(base, 1, *commandReservation);
  };

  // Unattached direct-drive path: synchronous fill, exactly as before (with
  // the demand pin acquired first when a reservation is supplied).
  if (!m_attached.load(std::memory_order_acquire)) {
    const SessionGraphError result = primeDemand();
    if (result != SessionGraphError::OK) {
      return result;
    }
    return fillWindow(base, max_pages);
  }

  // Attached: steer the worker, then wait for it to fill the wanted window.
  // With a reservation the pinned demand page is skipped by the resident check
  // instead of consuming `max_pages`, so one extra look-ahead page may fill —
  // harmless, and no test depends on it.
  m_demand.store(pos, std::memory_order_relaxed);
  const SessionGraphError result = primeDemand();
  if (result != SessionGraphError::OK) {
    return result;
  }

  std::array<int64_t, kWindowPages> wanted{};
  size_t wantedCount = 0;
  for (size_t index = 0; index + 1 < kWindowPages; ++index) {
    wanted[wantedCount++] = base + static_cast<int64_t>(index * static_cast<size_t>(kPageFrames));
  }
  wanted[wantedCount++] = base - static_cast<int64_t>(kPageFrames);
  if (wantedCount > max_pages) {
    wantedCount = max_pages;
  }

  const auto allWantedResident = [&]() {
    for (size_t index = 0; index < wantedCount; ++index) {
      const int64_t start = wanted[index];
      if (start < 0 || start >= m_lengthFrames) {
        continue;
      }
      bool resident = false;
      for (auto& page : m_pages) {
        if (page.start.load(std::memory_order_acquire) == start) {
          resident = true;
          break;
        }
      }
      if (!resident) {
        return false;
      }
    }
    return true;
  };

  std::unique_lock<std::mutex> lock(m_fillMutex);
  m_fillCv.wait_for(lock, kFillWaitTimeout, allWantedResident);
  lock.unlock();

  bool demandResident = false;
  for (auto& page : m_pages) {
    if (page.start.load(std::memory_order_acquire) == base) {
      demandResident = true;
      break;
    }
  }
  return demandResident ? SessionGraphError::OK : SessionGraphError::NotReady;
}

void StreamingClipSource::service() {
  const int64_t pos = m_demand.load(std::memory_order_relaxed);
  if (m_lengthFrames > 0 && pos < m_lengthFrames) {
    (void)fillWindow(alignDown(std::max<int64_t>(0, pos)), kWindowPages);
  }
  m_fillCv.notify_all();
}

void StreamingClipSource::serviceCommandDemand() {
  if (!m_attached.load(std::memory_order_acquire)) {
    return;
  }

  uint64_t generation = 0;
  int64_t demandStart = -1;
  int64_t demandFrames = 0;
  uint16_t initialResidentMask = 0;
  {
    std::lock_guard<std::mutex> lock(m_fillMutex);
    if (m_commandFillRequest.state != CommandFillState::Pending ||
        m_commandFillRequest.generation == 0) {
      return;
    }
    generation = m_commandFillRequest.generation;
    demandStart = m_commandFillRequest.start;
    demandFrames = std::max<int64_t>(1, m_commandFillRequest.frames);
    initialResidentMask = m_commandFillRequest.residentPinMask;
    m_commandFillRequest.state = CommandFillState::Filling;
  }

  const int64_t firstPage = alignDown(demandStart);
  const int64_t lastPage = alignDown(demandStart + demandFrames - 1);
  const size_t pageCount =
      static_cast<size_t>((lastPage - firstPage) / static_cast<int64_t>(kPageFrames) + 1);
  uint16_t workerResidentMask = 0;
  uint16_t freshPageMask = 0;
  std::array<int64_t, kNumPages> freshStarts{};
  freshStarts.fill(-1);

  const auto requestIsFilling = [&]() {
    std::lock_guard<std::mutex> lock(m_fillMutex);
    return m_commandFillRequest.generation == generation &&
           m_commandFillRequest.state == CommandFillState::Filling;
  };

  const auto releaseWorkerOwnership = [&]() noexcept {
    for (size_t index = 0; index < kNumPages; ++index) {
      const uint16_t bit = static_cast<uint16_t>(uint16_t{1} << index);
      if ((workerResidentMask & bit) != 0) {
        releaseReadyPin(m_pages[index]);
      }
      if ((freshPageMask & bit) != 0) {
        m_pages[index].start.store(-1, std::memory_order_release);
        m_pages[index].guard.store(0, std::memory_order_release);
      }
    }
  };

  const auto finishWithoutSuccess = [&](CommandFillState failureState) noexcept {
    releaseWorkerOwnership();
    std::lock_guard<std::mutex> lock(m_fillMutex);
    if (m_commandFillRequest.generation != generation) {
      return;
    }
    m_commandFillRequest.residentPinMask = initialResidentMask;
    m_commandFillRequest.freshPageMask = 0;
    m_commandFillRequest.state = m_commandFillRequest.state == CommandFillState::CancelRequested
                                     ? CommandFillState::Cancelled
                                     : failureState;
    m_fillCv.notify_all();
  };

  for (size_t pageOffset = 0; pageOffset < pageCount; ++pageOffset) {
    if (!requestIsFilling()) {
      finishWithoutSuccess(CommandFillState::Cancelled);
      return;
    }
    const int64_t pageStart =
        firstPage + static_cast<int64_t>(pageOffset * static_cast<size_t>(kPageFrames));

    size_t residentIndex = kNumPages;
    for (size_t index = 0; index < kNumPages; ++index) {
      if (m_pages[index].start.load(std::memory_order_acquire) == pageStart) {
        residentIndex = index;
        break;
      }
    }
    if (residentIndex != kNumPages) {
      const uint16_t bit = static_cast<uint16_t>(uint16_t{1} << residentIndex);
      if ((initialResidentMask & bit) != 0) {
        continue;
      }
      Page& page = m_pages[residentIndex];
      uint32_t expected = page.guard.load(std::memory_order_acquire);
      bool pinned = false;
      while (expected < kClaimed - 1) {
        if (page.guard.compare_exchange_weak(expected, expected + 1, std::memory_order_acq_rel,
                                             std::memory_order_acquire)) {
          if (page.start.load(std::memory_order_acquire) == pageStart) {
            workerResidentMask |= bit;
            pinned = true;
          } else {
            releaseReadyPin(page);
          }
          break;
        }
      }
      if (pinned) {
        continue;
      }
    }

    if (!requestIsFilling()) {
      finishWithoutSuccess(CommandFillState::Cancelled);
      return;
    }

    Page* target = nullptr;
    size_t targetIndex = kNumPages;
    SessionGraphError decodeResult = SessionGraphError::OK;
    {
      std::lock_guard<std::mutex> readerLock(m_readerMutex);
      for (size_t index = kWindowPages; index < kNumPages; ++index) {
        Page& page = m_pages[index];
        const int64_t existingStart = page.start.load(std::memory_order_acquire);
        if (existingStart != -1) {
          const int64_t demand =
              alignDown(std::max<int64_t>(0, m_demand.load(std::memory_order_relaxed)));
          const int64_t windowBase = demand - static_cast<int64_t>(kPageFrames);
          const int64_t windowEnd =
              demand + static_cast<int64_t>(kWindowPages - 1) * static_cast<int64_t>(kPageFrames);
          if (existingStart >= windowBase && existingStart < windowEnd) {
            continue;
          }
        }
        uint32_t expected = 0;
        if (page.guard.compare_exchange_strong(expected, kClaimed, std::memory_order_acq_rel,
                                               std::memory_order_acquire)) {
          page.start.store(-1, std::memory_order_release);
          target = &page;
          targetIndex = index;
          freshPageMask |= static_cast<uint16_t>(uint16_t{1} << index);
          break;
        }
      }
      if (target == nullptr) {
        decodeResult = SessionGraphError::NotReady;
      } else {
        decodeResult = decodePage(*target, pageStart);
      }
    }
    if (decodeResult != SessionGraphError::OK) {
      finishWithoutSuccess(CommandFillState::Failed);
      return;
    }
    freshStarts[targetIndex] = pageStart;
    if (!requestIsFilling()) {
      finishWithoutSuccess(CommandFillState::Cancelled);
      return;
    }
  }

  bool published = false;
  {
    std::lock_guard<std::mutex> lock(m_fillMutex);
    if (m_commandFillRequest.generation == generation &&
        m_commandFillRequest.state == CommandFillState::Filling) {
      for (size_t index = kWindowPages; index < kNumPages; ++index) {
        const uint16_t bit = static_cast<uint16_t>(uint16_t{1} << index);
        if ((freshPageMask & bit) == 0) {
          continue;
        }
        m_pages[index].start.store(freshStarts[index], std::memory_order_release);
        m_pages[index].guard.store(1, std::memory_order_release);
      }
      m_commandFillRequest.residentPinMask =
          static_cast<uint16_t>(initialResidentMask | workerResidentMask);
      m_commandFillRequest.freshPageMask = freshPageMask;
      m_commandFillRequest.state = CommandFillState::Succeeded;
      published = true;
    }
  }
  if (published) {
    m_fillCv.notify_all();
    return;
  }

  finishWithoutSuccess(CommandFillState::Cancelled);
}

SessionGraphError
StreamingClipSource::prepareLoopAnchorTransition(int64_t pos, bool enabled,
                                                 LoopAnchorTransition& transition) {
  transition = {};
  transition.previousPageIndex = m_loopAnchorPageIndex;
  transition.previousStart = m_loopAnchorStart;
  transition.replacementPageIndex = m_loopAnchorPageIndex;
  transition.replacementStart = m_loopAnchorStart;

  if (!enabled) {
    transition.replacementPageIndex = LoopAnchorTransition::kInactivePageIndex;
    transition.replacementStart = -1;
    m_loopAnchorPageIndex = LoopAnchorTransition::kInactivePageIndex;
    m_loopAnchorStart = -1;
    return SessionGraphError::OK;
  }
  if (m_lengthFrames <= 0) {
    transition = {};
    return SessionGraphError::NotReady;
  }

  const int64_t anchor = alignDown(std::clamp<int64_t>(pos, 0, m_lengthFrames - 1));
  if (m_loopAnchorStart == anchor && m_loopAnchorPageIndex < kNumPages) {
    return SessionGraphError::OK;
  }

  PrimeReservation temporaryReservation{};
  const SessionGraphError primeResult = primeForCommand(anchor, 1, temporaryReservation);
  if (primeResult != SessionGraphError::OK) {
    transition = {};
    return primeResult;
  }

  size_t replacementIndex = kNumPages;
  for (size_t index = 0; index < kNumPages; ++index) {
    const uint16_t bit = static_cast<uint16_t>(uint16_t{1} << index);
    if ((temporaryReservation.pageMask & bit) == 0) {
      continue;
    }
    if (replacementIndex != kNumPages) {
      releaseCommandPrime(temporaryReservation);
      transition = {};
      return SessionGraphError::InternalError;
    }
    replacementIndex = index;
  }
  if (replacementIndex == kNumPages ||
      m_pages[replacementIndex].start.load(std::memory_order_acquire) != anchor) {
    releaseCommandPrime(temporaryReservation);
    transition = {};
    return SessionGraphError::NotReady;
  }

  const uint32_t pending = m_pendingCommandPrimes.load(std::memory_order_acquire);
  assert(pending > 0);
  if (pending == 0) {
    releaseCommandPrime(temporaryReservation);
    transition = {};
    return SessionGraphError::InternalError;
  }
  m_pendingCommandPrimes.fetch_sub(1, std::memory_order_release);

  transition.replacementPageIndex = replacementIndex;
  transition.replacementStart = anchor;
  m_loopAnchorPageIndex = replacementIndex;
  m_loopAnchorStart = anchor;
  return SessionGraphError::OK;
}

void StreamingClipSource::commitLoopAnchorTransition(LoopAnchorTransition& transition) noexcept {
  if (transition.previousPageIndex != transition.replacementPageIndex ||
      transition.previousStart != transition.replacementStart) {
    if (transition.previousPageIndex < kNumPages) {
      releaseReadyPin(m_pages[transition.previousPageIndex]);
    }
  }
  transition = {};
}

void StreamingClipSource::rollbackLoopAnchorTransition(LoopAnchorTransition& transition) noexcept {
  if (transition.previousPageIndex != transition.replacementPageIndex ||
      transition.previousStart != transition.replacementStart) {
    if (transition.replacementPageIndex < kNumPages) {
      releaseReadyPin(m_pages[transition.replacementPageIndex]);
    }
  }
  m_loopAnchorPageIndex = transition.previousPageIndex;
  m_loopAnchorStart = transition.previousStart;
  transition = {};
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
  // Publish attachment before waking the worker so its very first pass sees
  // the attached source as worker-owned (command priming requests+waits).
  source->m_attached.store(true, std::memory_order_release);
  m_wake.notify_all();
}

void MediaStreamWorker::run() {
  std::unique_lock<std::mutex> lock(m_mutex);
  std::vector<std::shared_ptr<StreamingClipSource>> live;
  while (!m_stop) {
    // Snapshot live sources, then service them without holding the lock so
    // attach() never blocks behind file I/O.
    live.clear();
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
    // Two passes: command-prime demand first (bounds command-prime latency for
    // accepted seeks), then the steady window refill.
    for (auto& source : live) {
      source->serviceCommandDemand();
    }
    for (auto& source : live) {
      source->service();
    }
    lock.lock();

    // The audio thread cannot notify (no locks there); poll. 2ms keeps
    // command-prime and refill latency negligible against a multi-second
    // resident window.
    m_wake.wait_for(lock, std::chrono::milliseconds(2), [this]() { return m_stop; });
  }
}

} // namespace orpheus
