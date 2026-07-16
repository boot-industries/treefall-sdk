// SPDX-License-Identifier: MIT
#include <orpheus/clip_source.h>

#include <algorithm>
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

bool StreamingClipSource::read(int64_t pos, float* dest, size_t frames, size_t& framesRead) {
  framesRead = 0;
  if (pos < 0 || pos >= m_lengthFrames) {
    return true; // valid read of zero frames
  }
  const int64_t available = m_lengthFrames - pos;
  size_t toCopy = static_cast<size_t>(std::min<int64_t>(static_cast<int64_t>(frames), available));

  // Publish the demand position and retire every READY page outside the
  // demand window — one page BEHIND the demand page (reverse/scrub runway,
  // FTR025 T3b) through two pages ahead. Retirement here — on the audio
  // thread, the pages' owner — is what hands FREE pages back to the worker
  // after seeks/loops; without it the ring would pin stale pages forever.
  // (Release store; the worker acquires before writing.) The window must
  // match what prefill()/service() fill, or the two sides would thrash.
  m_demand.store(pos, std::memory_order_relaxed);
  const int64_t windowBase = alignDown(pos) - static_cast<int64_t>(kPageFrames);
  const int64_t windowEnd =
      alignDown(pos) + static_cast<int64_t>(kWindowPages - 1) * static_cast<int64_t>(kPageFrames);
  for (auto& page : m_pages) {
    const int64_t start = page.start.load(std::memory_order_acquire);
    if (start >= 0 && (start < windowBase || start >= windowEnd)) {
      page.start.store(-1, std::memory_order_release);
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
      return false; // miss → caller emits silence + BufferUnderrun
    }
    copied += subFrames;
  }

  framesRead = toCopy;
  return true;
}

bool StreamingClipSource::fillPage(int64_t alignedStart) {
  // Worker service and control-thread refire priming may run concurrently.
  // Serialize both page ownership selection and reader access so they cannot
  // claim the same FREE page or publish duplicate copies of one page.
  std::lock_guard<std::mutex> lock(m_readerMutex);
  for (auto& page : m_pages) {
    if (page.start.load(std::memory_order_acquire) == alignedStart) {
      return true;
    }
  }

  Page* target = nullptr;
  for (auto& page : m_pages) {
    if (page.start.load(std::memory_order_acquire) == -1) {
      target = &page;
      break;
    }
  }
  if (target == nullptr) {
    return false;
  }

  const int64_t framesLeft = m_lengthFrames - alignedStart;
  const size_t want = static_cast<size_t>(
      std::min<int64_t>(static_cast<int64_t>(kPageFrames), std::max<int64_t>(0, framesLeft)));
  if (want == 0) {
    return false;
  }

  if (!m_reader || m_reader->seek(alignedStart) != SessionGraphError::OK) {
    return false;
  }
  auto result = m_reader->readSamples(target->data.data(), want);
  if (!result.isOk()) {
    return false;
  }
  const size_t got = result.value;
  if (got < kPageFrames) {
    std::fill(target->data.begin() + static_cast<ptrdiff_t>(got * m_numChannels),
              target->data.end(), 0.0f);
  }

  target->start.store(alignedStart, std::memory_order_release);
  return true;
}

void StreamingClipSource::prefill(int64_t pos, size_t max_pages) {
  const int64_t base = alignDown(std::max<int64_t>(0, pos));

  // Fill order: the audible (demand) page first, then the forward pages, then
  // the behind page — so a max_pages-capped prime always makes the position
  // under the cursor playable, and reverse runway comes after lookahead. The
  // candidate set matches the retirement window in read() exactly.
  int64_t wanted[kWindowPages];
  size_t num_wanted = 0;
  for (size_t i = 0; i + 1 < kWindowPages; ++i) {
    wanted[num_wanted++] = base + static_cast<int64_t>(i) * static_cast<int64_t>(kPageFrames);
  }
  wanted[num_wanted++] = base - static_cast<int64_t>(kPageFrames);

  size_t filled = 0;
  for (size_t i = 0; i < num_wanted && filled < max_pages; ++i) {
    const int64_t start = wanted[i];
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
    if (!resident && fillPage(start)) {
      ++filled;
    }
  }
}

void StreamingClipSource::service() {
  prefill(m_demand.load(std::memory_order_relaxed));
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
