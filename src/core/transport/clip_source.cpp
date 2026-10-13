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
    if (!result.isOk() || result.value != want) {
      return nullptr;
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
  const uint32_t observed = page.guard.load(std::memory_order_acquire);
  assert(observed > 0 && observed < kClaimed);
  if (observed == 0 || observed >= kClaimed) {
    return;
  }
  // This caller owns one pin: other owners cannot release it or claim the
  // page before this RMW, so release needs no contention-dependent retry.
  const uint32_t previous = page.guard.fetch_sub(1, std::memory_order_acq_rel);
  assert(previous > 0 && previous < kClaimed);
  static_cast<void>(previous);
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

  // The demand deadline is an ownership fence, not a read/cancellation bound.
  // A started decode must acknowledge cancellation before reader fallback.
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
  // request). Reset the transaction before serialized synchronous fallback.
  // An arbitrary reader call may still block indefinitely.
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

SessionGraphError StreamingClipSource::fillWindow(int64_t base, size_t max_pages,
                                                  size_t pageOpportunities) {
  // Fill order: audible demand, forward look-ahead, then reverse runway.
  std::array<int64_t, kWindowPages> wanted{};
  size_t wantedCount = 0;
  for (size_t index = 0; index + 1 < kWindowPages; ++index) {
    wanted[wantedCount++] = base + static_cast<int64_t>(index * static_cast<size_t>(kPageFrames));
  }
  wanted[wantedCount++] = base - static_cast<int64_t>(kPageFrames);

  size_t filled = 0;
  size_t attempted = 0;
  for (size_t index = 0; index < wantedCount && filled < max_pages && attempted < pageOpportunities;
       ++index) {
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

    ++attempted;
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

void StreamingClipSource::serviceWindow(size_t pageOpportunities) {
  const int64_t pos = m_demand.load(std::memory_order_relaxed);
  if (m_lengthFrames > 0 && pos < m_lengthFrames) {
    (void)fillWindow(alignDown(std::max<int64_t>(0, pos)), kWindowPages, pageOpportunities);
  }
  m_fillCv.notify_all();
}

void StreamingClipSource::service() {
  serviceWindow(kWindowPages);
}

void StreamingClipSource::finishCommandFillFailure(CommandFillWork& work,
                                                   CommandFillState failureState) {
  for (size_t index = 0; index < kNumPages; ++index) {
    const uint16_t bit = static_cast<uint16_t>(uint16_t{1} << index);
    if ((work.workerResidentMask & bit) != 0) {
      releaseReadyPin(m_pages[index]);
    }
    if ((work.freshPageMask & bit) != 0) {
      m_pages[index].start.store(-1, std::memory_order_release);
      m_pages[index].guard.store(0, std::memory_order_release);
    }
  }
  {
    std::lock_guard<std::mutex> lock(m_fillMutex);
    if (m_commandFillRequest.generation == work.generation) {
      m_commandFillRequest.residentPinMask = work.initialResidentMask;
      m_commandFillRequest.freshPageMask = 0;
      m_commandFillRequest.state = m_commandFillRequest.state == CommandFillState::CancelRequested
                                       ? CommandFillState::Cancelled
                                       : failureState;
    }
  }
  work = {};
  m_fillCv.notify_all();
}

bool StreamingClipSource::serviceCommandDemandPage(CommandFillWork& work) {
  if (!m_attached.load(std::memory_order_acquire)) {
    return false;
  }
  if (work.generation == 0) {
    std::lock_guard<std::mutex> lock(m_fillMutex);
    if (m_commandFillRequest.state != CommandFillState::Pending ||
        m_commandFillRequest.generation == 0) {
      return false;
    }
    work.generation = m_commandFillRequest.generation;
    work.firstPage = alignDown(m_commandFillRequest.start);
    const int64_t lastPage = alignDown(m_commandFillRequest.start +
                                       std::max<int64_t>(1, m_commandFillRequest.frames) - 1);
    work.pageCount =
        static_cast<size_t>((lastPage - work.firstPage) / static_cast<int64_t>(kPageFrames) + 1);
    work.initialResidentMask = m_commandFillRequest.residentPinMask;
    work.freshStarts.fill(-1);
    m_commandFillRequest.state = CommandFillState::Filling;
  }

  const auto requestIsFilling = [&]() {
    std::lock_guard<std::mutex> lock(m_fillMutex);
    return m_commandFillRequest.generation == work.generation &&
           m_commandFillRequest.state == CommandFillState::Filling;
  };
  if (!requestIsFilling()) {
    finishCommandFillFailure(work, CommandFillState::Cancelled);
    return true;
  }

  // One command page opportunity per round. Fresh decoded pages retain their
  // exclusive claims, without publishing start, until the entire set succeeds.
  const int64_t pageStart = work.firstPage + static_cast<int64_t>(work.pageOffset * kPageFrames);
  bool residentPinned = false;
  for (size_t index = 0; index < kNumPages; ++index) {
    Page& page = m_pages[index];
    if (page.start.load(std::memory_order_acquire) != pageStart) {
      continue;
    }
    const uint16_t bit = static_cast<uint16_t>(uint16_t{1} << index);
    if ((work.initialResidentMask & bit) != 0) {
      residentPinned = true;
      break;
    }
    uint32_t expected = page.guard.load(std::memory_order_acquire);
    for (size_t attempt = 0; attempt < kNumPages && expected < kClaimed - 1; ++attempt) {
      if (page.guard.compare_exchange_strong(expected, expected + 1, std::memory_order_acq_rel,
                                             std::memory_order_acquire)) {
        if (page.start.load(std::memory_order_acquire) == pageStart) {
          work.workerResidentMask |= bit;
          residentPinned = true;
        } else {
          releaseReadyPin(page);
        }
        break;
      }
    }
    break;
  }

  if (!residentPinned) {
    if (!requestIsFilling()) {
      finishCommandFillFailure(work, CommandFillState::Cancelled);
      return true;
    }
    size_t targetIndex = kNumPages;
    SessionGraphError decodeResult = SessionGraphError::NotReady;
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
          targetIndex = index;
          work.freshPageMask |= static_cast<uint16_t>(uint16_t{1} << index);
          decodeResult = decodePage(page, pageStart);
          break;
        }
      }
    }
    if (decodeResult != SessionGraphError::OK) {
      finishCommandFillFailure(work, CommandFillState::Failed);
      return true;
    }
    work.freshStarts[targetIndex] = pageStart;
  }
  if (!requestIsFilling()) {
    finishCommandFillFailure(work, CommandFillState::Cancelled);
    return true;
  }
  if (++work.pageOffset < work.pageCount) {
    return true;
  }

  bool published = false;
  {
    std::lock_guard<std::mutex> lock(m_fillMutex);
    if (m_commandFillRequest.generation == work.generation &&
        m_commandFillRequest.state == CommandFillState::Filling) {
      for (size_t index = kWindowPages; index < kNumPages; ++index) {
        const uint16_t bit = static_cast<uint16_t>(uint16_t{1} << index);
        if ((work.freshPageMask & bit) != 0) {
          m_pages[index].start.store(work.freshStarts[index], std::memory_order_release);
          m_pages[index].guard.store(1, std::memory_order_release);
        }
      }
      m_commandFillRequest.residentPinMask =
          static_cast<uint16_t>(work.initialResidentMask | work.workerResidentMask);
      m_commandFillRequest.freshPageMask = work.freshPageMask;
      m_commandFillRequest.state = CommandFillState::Succeeded;
      published = true;
    }
  }
  if (published) {
    work = {};
    m_fillCv.notify_all();
  } else {
    finishCommandFillFailure(work, CommandFillState::Cancelled);
  }
  return true;
}

void StreamingClipSource::serviceCommandDemand() {
  CommandFillWork work{};
  do {
    if (!serviceCommandDemandPage(work)) {
      return;
    }
  } while (work.generation != 0);
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
    bool registered = false;
    for (auto it = m_sources.begin(); it != m_sources.end();) {
      if (auto live = it->lock()) {
        registered |= live.get() == source.get();
        ++it;
      } else {
        it = m_sources.erase(it);
      }
    }
    if (!registered) {
      m_sources.push_back(source);
    }
    // Publish attachment under the snapshot lock, before the first service.
    source->m_attached.store(true, std::memory_order_release);
  }
  m_wake.notify_all();
}

void MediaStreamWorker::snapshotSources(std::vector<SourceService>& live,
                                        std::vector<SourceService>& next) {
  // Drop idle snapshot ownership before locking weak references. Only an
  // in-progress transaction keeps its source alive between service rounds.
  for (auto& entry : live) {
    if (entry.command.generation == 0) {
      entry.source.reset();
    }
  }
  std::erase_if(live, [](const SourceService& entry) { return !entry.source; });
  next.clear();
  next.reserve(m_sources.size());
  for (auto it = m_sources.begin(); it != m_sources.end();) {
    if (auto source = it->lock()) {
      SourceService entry{std::move(source), {}};
      for (auto& previous : live) {
        if (previous.source.get() == entry.source.get()) {
          entry.command = previous.command;
          break;
        }
      }
      next.push_back(std::move(entry));
      ++it;
    } else {
      it = m_sources.erase(it);
    }
  }
  live.swap(next);
  next.clear();
}

void MediaStreamWorker::serviceRound(std::vector<SourceService>& live, size_t& cursor) {
  if (live.empty()) {
    cursor = 0;
    return;
  }
  cursor %= live.size();
  // Mark sources with a command opportunity so completion cannot also grant
  // them a look-ahead page in this round. All command work precedes look-ahead.
  for (size_t offset = 0; offset < live.size(); ++offset) {
    const size_t index = (cursor + offset) % live.size();
    live[index].commandServiced = live[index].source->serviceCommandDemandPage(live[index].command);
  }
  for (size_t offset = 0; offset < live.size(); ++offset) {
    const size_t index = (cursor + offset) % live.size();
    if (!live[index].commandServiced) {
      live[index].source->serviceWindow(1);
    }
  }
  cursor = (cursor + 1) % live.size();
}

void MediaStreamWorker::run() {
  std::unique_lock<std::mutex> lock(m_mutex);
  std::vector<SourceService> live;
  std::vector<SourceService> next;
  size_t cursor = 0;
  while (!m_stop) {
    snapshotSources(live, next);
    lock.unlock();
    serviceRound(live, cursor);
    lock.lock();
    // Audio demand is polled without audio-side locking/notification. The
    // polling cadence is not a reader-service or command-latency guarantee.
    m_wake.wait_for(lock, std::chrono::milliseconds(2), [this]() { return m_stop; });
  }
  lock.unlock();
  for (auto& entry : live) {
    if (entry.command.generation != 0) {
      entry.source->finishCommandFillFailure(entry.command,
                                             StreamingClipSource::CommandFillState::Cancelled);
    }
  }
}

} // namespace orpheus
