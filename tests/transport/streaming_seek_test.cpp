// SPDX-License-Identifier: MIT
// ORP168: rendered first-block streaming seek and command-prime transactions.

#include "../support/rt_guard.hpp"
#include "../support/unique_temp_dir.hpp"

#include "audio_io/resampling_audio_file_reader.h"
#include "transport/transport_controller.h"

#include <orpheus/clip_source.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <barrier>
#include <bit>
#include <cassert>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include <tuple>
#include <vector>

using namespace orpheus;
using orpheus::tests::support::RtGuardState;
using orpheus::tests::support::RtSection;
constexpr size_t kVoiceDemandTestCapacity = kActiveVoiceSnapshotCapacity;

namespace orpheus {

class StreamingClipSourceTestAccess {
public:
  static void setAttached(StreamingClipSource& source, bool attached) {
    source.m_attached.store(attached, std::memory_order_release);
  }

  // Deterministic drive calls the background worker's actual snapshot/round
  // functions after parking its thread; no competing scheduler or sleeps.
  struct WorkerDrive {
    std::vector<MediaStreamWorker::SourceService> live;
    std::vector<MediaStreamWorker::SourceService> next;
    std::vector<MediaStreamWorker::PreviousSource> previous;
    size_t cursor{0};
  };

  static void parkWorker(MediaStreamWorker& worker) {
    {
      std::lock_guard<std::mutex> lock(worker.m_mutex);
      worker.m_stop = true;
    }
    worker.m_wake.notify_all();
    worker.m_thread.join();
  }

  static void serviceRound(MediaStreamWorker& worker, WorkerDrive& drive) {
    {
      std::lock_guard<std::mutex> lock(worker.m_mutex);
      worker.snapshotSources(drive.live, drive.next, drive.previous);
    }
    MediaStreamWorker::serviceRound(drive.live, drive.cursor);
  }

  static void rollbackDrive(WorkerDrive& drive) {
    for (auto& entry : drive.live) {
      if (entry.command.generation != 0) {
        entry.source->finishCommandFillFailure(entry.command,
                                               StreamingClipSource::CommandFillState::Cancelled);
      }
    }
    drive.live.clear();
    drive.next.clear();
  }

  static bool hasExpiredRegistration(MediaStreamWorker& worker) {
    std::lock_guard<std::mutex> lock(worker.m_mutex);
    return std::any_of(worker.m_sources.begin(), worker.m_sources.end(),
                       [](const auto& source) { return source.expired(); });
  }

  static void requestCancellation(StreamingClipSource& source) {
    std::lock_guard<std::mutex> lock(source.m_fillMutex);
    source.m_commandFillRequest.state = StreamingClipSource::CommandFillState::CancelRequested;
  }

  static bool commandSucceeded(StreamingClipSource& source) {
    std::lock_guard<std::mutex> lock(source.m_fillMutex);
    return source.m_commandFillRequest.state == StreamingClipSource::CommandFillState::Succeeded;
  }

  static bool waitForPending(StreamingClipSource& source, std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(source.m_fillMutex);
    return source.m_fillCv.wait_for(lock, timeout, [&]() {
      return source.m_commandFillRequest.state == StreamingClipSource::CommandFillState::Pending;
    });
  }

  static bool waitForCancelled(StreamingClipSource& source, std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(source.m_fillMutex);
    return source.m_fillCv.wait_for(lock, timeout, [&]() {
      return source.m_commandFillRequest.state == StreamingClipSource::CommandFillState::Cancelled;
    });
  }

  static void serviceCommandDemand(StreamingClipSource& source) {
    source.serviceCommandDemand();
  }

  static size_t residentPageIndex(StreamingClipSource& source, int64_t start) {
    for (size_t index = 0; index < StreamingClipSource::kNumPages; ++index) {
      if (source.m_pages[index].start.load(std::memory_order_acquire) == start) {
        return index;
      }
    }
    return StreamingClipSource::kNumPages;
  }

  static int64_t pageStart(StreamingClipSource& source, size_t index) {
    return source.m_pages[index].start.load(std::memory_order_acquire);
  }

  static uint32_t pageGuard(StreamingClipSource& source, size_t index) {
    return source.m_pages[index].guard.load(std::memory_order_acquire);
  }
  struct PublishedDemandSnapshot {
    std::array<int64_t, kVoiceDemandTestCapacity> positions{};
    size_t count{0};
  };

  static void publishDemands(StreamingClipSource& source,
                             const std::array<int64_t, kVoiceDemandTestCapacity>& positions,
                             size_t count) {
    source.beginVoiceDemands();
    for (size_t index = 0; index < count; ++index) {
      // A linear program: the window runs to the end of the clip and never jumps.
      source.addVoiceDemand(positions[index], source.lengthFrames(), -1);
    }
    source.publishVoiceDemands();
  }

  using VoiceDemand = StreamingClipSource::VoiceDemand;
  using DemandArray = std::array<VoiceDemand, kVoiceDemandTestCapacity>;

  static void publishPrograms(StreamingClipSource& source, const DemandArray& demands,
                              size_t count) {
    source.beginVoiceDemands();
    for (size_t index = 0; index < count; ++index) {
      source.addVoiceDemand(demands[index].position, demands[index].windowEnd,
                            demands[index].jumpTarget);
    }
    source.publishVoiceDemands();
  }

  static bool keepsPage(const StreamingClipSource& source, int64_t pageStart) {
    return source.keepsPage(pageStart, 0);
  }

  static bool readResidentFrame(StreamingClipSource& source, int64_t position, float* destination) {
    return source.readResidentFrame(position, destination);
  }

  static bool readPublishedDemands(StreamingClipSource& source, PublishedDemandSnapshot& result) {
    StreamingClipSource::VoiceDemandSnapshot snapshot{};
    if (!source.readWorkerVoiceDemands(snapshot)) {
      return false;
    }
    result.count = snapshot.count;
    for (size_t index = 0; index < snapshot.count; ++index) {
      result.positions[index] = snapshot.demands[index].position;
    }
    return true;
  }

  static bool readForVoice(StreamingClipSource& source, size_t demandIndex, int64_t position,
                           float* destination, size_t frames, size_t& framesRead) {
    return source.readForVoice(demandIndex, position, source.lengthFrames(), -1, destination,
                               frames, framesRead);
  }
  static bool acquireTransientReadPin(StreamingClipSource& source, size_t index,
                                      int64_t expectedStart) {
    return source.acquireTransientReadPin(source.m_pages[index], expectedStart);
  }

  static bool acquireReadyPin(StreamingClipSource& source, size_t index) {
    auto& guard = source.m_pages[index].guard;
    uint32_t expected = guard.load(std::memory_order_acquire);
    return expected < StreamingClipSource::kClaimed - 1 &&
           guard.compare_exchange_strong(expected, expected + 1, std::memory_order_acq_rel,
                                         std::memory_order_acquire);
  }

  static void releaseReadyPin(StreamingClipSource& source, size_t index) {
    source.releaseReadyPin(source.m_pages[index]);
  }

  static bool claimPage(StreamingClipSource& source, size_t index) {
    uint32_t expected = 0;
    return source.m_pages[index].guard.compare_exchange_strong(
        expected, StreamingClipSource::kClaimed, std::memory_order_acq_rel,
        std::memory_order_acquire);
  }

  static void releaseClaim(StreamingClipSource& source, size_t index) {
    source.m_pages[index].guard.store(0, std::memory_order_release);
  }

  // Test-only: makes every read of the page miss. No pin may be released on that page while
  // it is blocked.
  static uint32_t blockReads(StreamingClipSource& source, size_t index) {
    return source.m_pages[index].guard.exchange(StreamingClipSource::kClaimed,
                                                std::memory_order_acq_rel);
  }

  static void unblockReads(StreamingClipSource& source, size_t index, uint32_t previous) {
    source.m_pages[index].guard.store(previous, std::memory_order_release);
  }
};

class TransportControllerTestAccess {
public:
  static std::shared_ptr<StreamingClipSource> source(TransportController& transport,
                                                     ClipHandle handle) {
    std::lock_guard lock(transport.m_sourceRegistry.mutex());
    return std::dynamic_pointer_cast<StreamingClipSource>(
        transport.m_sourceRegistry.entries().at(handle).source);
  }
  static bool readSourceDemands(TransportController& transport, ClipHandle handle,
                                StreamingClipSourceTestAccess::PublishedDemandSnapshot& snapshot) {
    auto streaming = source(transport, handle);
    if (streaming == nullptr) {
      return false;
    }
    for (size_t attempt = 0; attempt < 32; ++attempt) {
      if (StreamingClipSourceTestAccess::readPublishedDemands(*streaming, snapshot)) {
        return true;
      }
    }
    return false;
  }

  static std::shared_ptr<IClipSource> playbackSource(TransportController& transport,
                                                     ClipHandle handle) {
    std::lock_guard lock(transport.m_sourceRegistry.mutex());
    return transport.m_sourceRegistry.entries().at(handle).source;
  }

  static void replaceUnpreparedReader(TransportController& transport, ClipHandle handle,
                                      std::shared_ptr<IAudioFileReader> reader,
                                      const AudioFileMetadata& metadata) {
    std::lock_guard lock(transport.m_sourceRegistry.mutex());
    auto& entry = transport.m_sourceRegistry.entries().at(handle);
    assert(!entry.source);
    entry.reader = std::move(reader);
    entry.metadata = metadata;
    entry.trimOutSamples = metadata.duration_samples;
  }

  static void consumeCommands(TransportController& transport) {
    transport.processCommands();
  }

  static SessionGraphError rejectPreparedLoopTransition(TransportController& transport,
                                                        ClipHandle handle, int64_t trimIn,
                                                        size_t& replacementIndex) {
    std::lock_guard lock(transport.m_sourceRegistry.mutex());
    const uint32_t index = transport.acquireCommandNode();
    if (index == UINT32_MAX) {
      return SessionGraphError::NotReady;
    }
    TransportController::CommandReservation reservation(transport, index);
    auto& entry = transport.m_sourceRegistry.entries().at(handle);
    auto* streaming = dynamic_cast<StreamingClipSource*>(entry.source.get());
    if (streaming == nullptr || !entry.commandLifetime) {
      return SessionGraphError::NotReady;
    }
    auto& command = transport.m_commandNodes[index].command;
    command = TransportCommand{};
    command.type = TransportCommand::Type::UpdateTrim;
    command.handle = handle;
    command.data.trim = {trimIn, entry.trimOutSamples};
    command.sourceLifetime = entry.commandLifetime.get();
    transport.m_sourceRegistry.retainSourceCommand(command.sourceLifetime);
    const auto result =
        streaming->prepareLoopAnchorTransition(trimIn, 1, true, command.loopAnchorTransition);
    if (result != SessionGraphError::OK) {
      return result;
    }
    command.loopAnchorSource = streaming;
    replacementIndex =
        static_cast<size_t>(std::countr_zero(command.loopAnchorTransition.replacementPageMask));
    // Deliberately do not publish: production reservation cleanup owns rollback.
    return SessionGraphError::OK;
  }
};

} // namespace orpheus

namespace {

constexpr std::array<float, 4> kMarkers = {-0.60f, -0.20f, 0.20f, 0.60f};

std::string writePositionCodedWav(const std::filesystem::path& directory, const std::string& name,
                                  uint32_t sampleRate, uint16_t channels) {
  const int64_t frames = static_cast<int64_t>(sampleRate) * 9;
  const std::filesystem::path path = directory / name;
  std::ofstream file(path, std::ios::binary);
  const uint32_t dataSize = static_cast<uint32_t>(frames * channels * sizeof(int16_t));
  const uint32_t fileSize = 36 + dataSize;
  const uint32_t fmtSize = 16;
  const uint16_t audioFormat = 1;
  const uint16_t blockAlign = channels * sizeof(int16_t);
  const uint32_t byteRate = sampleRate * blockAlign;
  const uint16_t bitsPerSample = 16;
  file.write("RIFF", 4);
  file.write(reinterpret_cast<const char*>(&fileSize), 4);
  file.write("WAVE", 4);
  file.write("fmt ", 4);
  file.write(reinterpret_cast<const char*>(&fmtSize), 4);
  file.write(reinterpret_cast<const char*>(&audioFormat), 2);
  file.write(reinterpret_cast<const char*>(&channels), 2);
  file.write(reinterpret_cast<const char*>(&sampleRate), 4);
  file.write(reinterpret_cast<const char*>(&byteRate), 4);
  file.write(reinterpret_cast<const char*>(&blockAlign), 2);
  file.write(reinterpret_cast<const char*>(&bitsPerSample), 2);
  file.write("data", 4);
  file.write(reinterpret_cast<const char*>(&dataSize), 4);

  const int64_t halfSecondFrames = sampleRate / 2;
  for (int64_t frame = 0; frame < frames; ++frame) {
    const float marker = kMarkers[static_cast<size_t>((frame / halfSecondFrames) % 4)];
    for (uint16_t channel = 0; channel < channels; ++channel) {
      const float value = marker * static_cast<float>(channel + 1) / static_cast<float>(channels);
      const int16_t sample = static_cast<int16_t>(std::lround(value * 32767.0f));
      file.write(reinterpret_cast<const char*>(&sample), sizeof(sample));
    }
  }
  return path.string();
}

class SeekCallback final : public ITransportCallback {
public:
  std::atomic<int> underruns{0};
  std::atomic<int> seeks{0};
  std::atomic<int> starts{0};
  std::atomic<int> secondRequestStarts{0};
  std::atomic<int> loops{0};
  std::vector<TransportUnderrun> classified; // control thread only (processCallbacks)

  void onClipStarted(ClipHandle, orpheus::StartRequestTag requestTag, uint32_t,
                     TransportPosition) override {
    starts.fetch_add(1, std::memory_order_relaxed);
    if (requestTag == 2) {
      secondRequestStarts.fetch_add(1, std::memory_order_relaxed);
    }
  }
  void onClipStopped(ClipHandle, orpheus::StartRequestTag, uint32_t, TransportPosition) override {}
  void onClipLooped(ClipHandle, orpheus::StartRequestTag, uint32_t, TransportPosition) override {
    loops.fetch_add(1, std::memory_order_relaxed);
  }
  void onBufferUnderrun(TransportPosition) override {
    underruns.fetch_add(1, std::memory_order_relaxed);
  }
  void onClassifiedUnderrun(const TransportUnderrun& underrun) override {
    classified.push_back(underrun);
  }
  void onClipSeeked(ClipHandle, TransportPosition) override {
    seeks.fetch_add(1, std::memory_order_relaxed);
  }
};

float trailingMean(const std::vector<float>& samples) {
  const size_t begin = samples.size() / 2;
  float sum = 0.0f;
  for (size_t index = begin; index < samples.size(); ++index) {
    sum += samples[index];
  }
  return sum / static_cast<float>(samples.size() - begin);
}

struct ServiceObservation {
  size_t source;
  int64_t start;
  size_t requested;
  int64_t enteredNs;
};

class ServiceTrace {
public:
  void record(size_t source, int64_t start, size_t requested) {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    std::lock_guard lock(m_mutex);
    m_observations.push_back({source, start, requested,
                              std::chrono::duration_cast<std::chrono::nanoseconds>(now).count()});
  }

  std::vector<ServiceObservation> observations() const {
    std::lock_guard lock(m_mutex);
    return m_observations;
  }

private:
  mutable std::mutex m_mutex;
  std::vector<ServiceObservation> m_observations;
};

class FaultReader final : public IAudioFileReader {
public:
  explicit FaultReader(int64_t frames, uint16_t channels = 1, uint32_t rate = 48000)
      : m_frames(frames), m_channels(channels), m_rate(rate) {}

  Result<AudioFileMetadata> open(const std::string&) override {
    m_open = true;
    Result<AudioFileMetadata> result{};
    result.value.format = AudioFileFormat::WAV;
    result.value.sample_rate = m_rate;
    result.value.num_channels = m_channels;
    result.value.duration_samples = m_frames;
    result.value.bit_depth = 16;
    result.error = SessionGraphError::OK;
    return result;
  }

  Result<size_t> readSamples(float* buffer, size_t samples) override {
    const size_t call = readCalls.fetch_add(1, std::memory_order_relaxed) + 1;
    if (m_trace) {
      m_trace->record(m_identity, m_position, samples);
    }
    if (std::this_thread::get_id() == m_callingThread) {
      readOnCallingThread.store(true, std::memory_order_relaxed);
    }
    {
      std::unique_lock<std::mutex> lock(m_gateMutex);
      if (call == m_blockReadCall) {
        m_blockedRead = true;
        m_gateCv.notify_all();
        m_gateCv.wait(lock, [&]() { return m_releaseBlockedRead; });
        m_blockReadCall = 0;
        m_releaseBlockedRead = false;
      }
    }
    if (call == failReadCall.load(std::memory_order_relaxed) || !m_open ||
        failRead.load(std::memory_order_relaxed)) {
      return {0, SessionGraphError::NotReady, {}};
    }
    if (call == zeroReadCall.load(std::memory_order_relaxed)) {
      return {0, SessionGraphError::OK, {}};
    }
    const size_t available = static_cast<size_t>(std::max<int64_t>(0, m_frames - m_position));
    size_t give = std::min(samples, available);
    if ((earlyEof.load(std::memory_order_relaxed) ||
         call == shortReadCall.load(std::memory_order_relaxed)) &&
        give != 0) {
      give -= 1;
    }
    for (size_t frame = 0; frame < give; ++frame) {
      for (uint16_t channel = 0; channel < m_channels; ++channel) {
        buffer[frame * m_channels + channel] =
            static_cast<float>(m_position + static_cast<int64_t>(frame));
      }
    }
    m_position += static_cast<int64_t>(give);
    return {give, SessionGraphError::OK, {}};
  }

  SessionGraphError seek(int64_t position) override {
    if (std::this_thread::get_id() == m_callingThread) {
      readOnCallingThread.store(true, std::memory_order_relaxed);
    }
    if (!m_open || failSeek.load(std::memory_order_relaxed)) {
      return SessionGraphError::NotReady;
    }
    m_position = std::clamp(position, int64_t{0}, m_frames);
    return SessionGraphError::OK;
  }

  void close() override {
    m_open = false;
  }
  int64_t getCurrentPosition() const override {
    return m_position;
  }
  bool isOpen() const override {
    return m_open;
  }

  std::atomic<bool> failRead{false};
  std::atomic<bool> failSeek{false};
  std::atomic<bool> earlyEof{false};
  std::atomic<bool> readOnCallingThread{false};
  std::atomic<size_t> readCalls{0};
  std::atomic<size_t> failReadCall{0};
  std::atomic<size_t> zeroReadCall{0};
  std::atomic<size_t> shortReadCall{0};

  void setServiceTrace(std::shared_ptr<ServiceTrace> trace, size_t identity) {
    m_trace = std::move(trace);
    m_identity = identity;
  }

  void resetReadCalls() {
    readCalls.store(0, std::memory_order_relaxed);
    failReadCall.store(0, std::memory_order_relaxed);
    zeroReadCall.store(0, std::memory_order_relaxed);
    shortReadCall.store(0, std::memory_order_relaxed);
  }

  void failReadOnCall(size_t call) {
    failReadCall.store(call, std::memory_order_relaxed);
  }

  void blockReadOnCall(size_t call) {
    std::lock_guard<std::mutex> lock(m_gateMutex);
    m_blockReadCall = call;
    m_blockedRead = false;
    m_releaseBlockedRead = false;
  }

  bool waitUntilReadBlocked(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(m_gateMutex);
    return m_gateCv.wait_for(lock, timeout, [&]() { return m_blockedRead; });
  }

  void releaseBlockedRead() {
    {
      std::lock_guard<std::mutex> lock(m_gateMutex);
      m_releaseBlockedRead = true;
    }
    m_gateCv.notify_all();
  }

  void setCallingThread(std::thread::id id) {
    m_callingThread = id;
  }

private:
  int64_t m_frames;
  uint16_t m_channels;
  uint32_t m_rate;
  int64_t m_position{0};
  bool m_open{false};
  std::thread::id m_callingThread{};
  std::mutex m_gateMutex;
  std::condition_variable m_gateCv;
  size_t m_blockReadCall{0};
  bool m_blockedRead{false};
  bool m_releaseBlockedRead{false};
  std::shared_ptr<ServiceTrace> m_trace;
  size_t m_identity{0};
};

class StreamingSeekMatrixTest : public ::testing::Test {
protected:
  static void SetUpTestSuite() {
    s_directory = orpheus::tests::support::UniqueTempPath("orp168_streaming_seek");
    std::filesystem::create_directories(s_directory);
  }

  static void TearDownTestSuite() {
    std::error_code error;
    std::filesystem::remove_all(s_directory, error);
  }

  static const std::string& sourcePath(uint32_t engineRate, uint16_t channels, uint32_t fileRate) {
    const auto key = std::make_tuple(engineRate, channels, fileRate);
    auto [it, inserted] = s_sources.try_emplace(key);
    if (inserted) {
      it->second = writePositionCodedWav(s_directory,
                                         "coded_" + std::to_string(engineRate) + "_" +
                                             std::to_string(channels) + "_" +
                                             std::to_string(fileRate) + ".wav",
                                         fileRate, channels);
    }
    return it->second;
  }

  static std::filesystem::path s_directory;
  static std::map<std::tuple<uint32_t, uint16_t, uint32_t>, std::string> s_sources;
};

std::filesystem::path StreamingSeekMatrixTest::s_directory;
std::map<std::tuple<uint32_t, uint16_t, uint32_t>, std::string> StreamingSeekMatrixTest::s_sources;

void configureLoopingTransport(TransportController& transport, SeekCallback& callback,
                               const std::string& path, int64_t trimIn, int64_t trimOut,
                               uint32_t sampleRate) {
  transport.setPreparedSourceMaxFrames(sampleRate);
  transport.setCallback(&callback);
  EXPECT_EQ(transport.registerClipAudio(1, path), SessionGraphError::OK);
  auto metadata = transport.getClipMetadata(1);
  ASSERT_TRUE(metadata.has_value());
  metadata->trimInSamples = trimIn;
  metadata->trimOutSamples = trimOut;
  metadata->loopEnabled = true;
  EXPECT_EQ(transport.updateClipMetadata(1, *metadata), SessionGraphError::OK);
  EXPECT_EQ(transport.startClip(1), SessionGraphError::OK);
}

std::unique_ptr<TransportController> makeStreamingTransport(uint32_t block) {
  TransportConfig config{.sampleRate = 48000,
                         .outputChannels = 1,
                         .maxBlockFrames = block,
                         .maxActiveVoices = 32,
                         .numGroups = 1,
                         .maxSourceChannels = 1,
                         .sourceChannelPolicy = SourceChannelPolicy::Discrete};
  return std::make_unique<TransportController>(nullptr, config);
}

void configureStreamingClip(TransportController& transport, SeekCallback& callback,
                            const std::string& path, int64_t trimIn, int64_t trimOut, double rate,
                            bool loop, VoiceMode mode,
                            const std::vector<ClipPlaybackSegment>& segments = {}) {
  transport.setPreparedSourceMaxFrames(48000);
  transport.setCallback(&callback);
  ASSERT_EQ(transport.registerClipAudio(1, path), SessionGraphError::OK);
  ASSERT_EQ(transport.setClipVoiceMode(1, mode), SessionGraphError::OK);
  auto metadata = transport.getClipMetadata(1);
  ASSERT_TRUE(metadata.has_value());
  metadata->trimInSamples = trimIn;
  metadata->trimOutSamples = trimOut;
  metadata->loopEnabled = loop;
  metadata->playbackRate = rate;
  metadata->segmentCount = static_cast<uint32_t>(segments.size());
  for (size_t index = 0; index < segments.size(); ++index) {
    metadata->segments[index] = segments[index];
  }
  ASSERT_EQ(transport.updateClipMetadata(1, *metadata), SessionGraphError::OK);
}

void renderBlocks(TransportController& transport, std::vector<float>& out, uint32_t block,
                  size_t count) {
  out.assign(block, 0.0f);
  float* buffers[] = {out.data()};
  for (size_t index = 0; index < count; ++index) {
    std::fill(out.begin(), out.end(), 0.0f);
    transport.processAudio(buffers, 1, block);
    transport.processCallbacks();
  }
}

// Renders one block per poll so the published demand keeps the worker refreshing its window.
bool waitForEvicted(TransportController& transport, StreamingClipSource& source,
                    std::vector<float>& out, uint32_t block, int64_t pageStart) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (std::chrono::steady_clock::now() < deadline) {
    if (StreamingClipSourceTestAccess::residentPageIndex(source, pageStart) ==
        StreamingClipSource::kNumPages) {
      return true;
    }
    renderBlocks(transport, out, block, 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return false;
}

bool waitForResident(StreamingClipSource& source, int64_t pageStart) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (std::chrono::steady_clock::now() < deadline) {
    if (StreamingClipSourceTestAccess::residentPageIndex(source, pageStart) <
        StreamingClipSource::kNumPages) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return false;
}

void drainAndCrossLoop(TransportController& transport, SeekCallback& callback,
                       std::vector<float>& output, uint32_t block, int64_t trimOut) {
  float* buffers[] = {output.data()};
  transport.processAudio(buffers, 1, block);
  ASSERT_EQ(transport.seekClip(1, trimOut - 2000), SessionGraphError::OK);
  RtGuardState::reset();
  for (int index = 0; index < 16; ++index) {
    {
      RtSection section;
      transport.processAudio(buffers, 1, block);
    }
    transport.processCallbacks();
  }
  EXPECT_EQ(callback.underruns.load(), 0);
}

TEST_F(StreamingSeekMatrixTest, FirstPostSeekBlockIsTargetAudioWithoutUnderrun) {
  for (const uint32_t engineRate : {44100u, 48000u, 96000u, 192000u}) {
    for (const uint32_t blockSize : {64u, 127u, 512u, 1024u}) {
      for (const uint16_t channels : {uint16_t{1}, uint16_t{4}}) {
        for (const double playbackRate : {0.5, 2.0}) {
          for (const bool mismatched : {false, true}) {
            const uint32_t fileRate =
                mismatched ? (engineRate == 44100 ? 48000 : 44100) : engineRate;
            SCOPED_TRACE(
                "engine=" + std::to_string(engineRate) + " block=" + std::to_string(blockSize) +
                " channels=" + std::to_string(channels) + " rate=" + std::to_string(playbackRate) +
                " file=" + std::to_string(fileRate));

            TransportConfig config{};
            config.sampleRate = engineRate;
            config.maxBlockFrames = blockSize;
            config.outputChannels = channels;
            config.maxSourceChannels = channels;
            config.numGroups = 1;
            config.sourceChannelPolicy = SourceChannelPolicy::Discrete;
            auto transport = std::make_unique<TransportController>(nullptr, config);
            transport->setPreparedSourceMaxFrames(engineRate);
            SeekCallback callback;
            transport->setCallback(&callback);
            ASSERT_EQ(transport->registerClipAudio(1, sourcePath(engineRate, channels, fileRate)),
                      SessionGraphError::OK);
            ASSERT_EQ(transport->updateClipTrimPoints(
                          1, 0, static_cast<int64_t>(8.75 * static_cast<double>(engineRate))),
                      SessionGraphError::OK);
            auto metadata = transport->getClipMetadata(1);
            ASSERT_TRUE(metadata.has_value());
            metadata->playbackRate = playbackRate;
            ASSERT_EQ(transport->updateClipMetadata(1, *metadata), SessionGraphError::OK);
            ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);

            std::vector<std::vector<float>> output(channels, std::vector<float>(blockSize, 0.0f));
            std::vector<float*> pointers(channels);
            for (uint16_t channel = 0; channel < channels; ++channel) {
              pointers[channel] = output[channel].data();
            }
            transport->processAudio(pointers.data(), channels, blockSize);
            const int64_t target = static_cast<int64_t>(6.75 * static_cast<double>(engineRate));
            ASSERT_EQ(transport->seekClip(1, target), SessionGraphError::OK);
            RtGuardState::reset();
            {
              RtSection section;
              transport->processAudio(pointers.data(), channels, blockSize);
            }
            transport->processCallbacks();

            for (uint16_t channel = 0; channel < channels; ++channel) {
              const float scale = static_cast<float>(channel + 1) / static_cast<float>(channels);
              EXPECT_NEAR(trailingMean(output[channel]), -0.20f * scale, 0.12f * scale);
            }
            EXPECT_EQ(callback.seeks.load(), 1);
            EXPECT_EQ(callback.underruns.load(), 0);
            EXPECT_EQ(RtGuardState::allocViolations(), 0u);
            EXPECT_EQ(RtGuardState::deallocViolations(), 0u);
          }
        }
      }
    }
  }
}

TEST_F(StreamingSeekMatrixTest, PageBoundaryAtFourTimesRatePrimesBothPages) {
  constexpr uint32_t rate = 48000;
  constexpr uint32_t block = 1024;
  TransportConfig config{.sampleRate = rate,
                         .outputChannels = 1,
                         .maxBlockFrames = block,
                         .maxActiveVoices = 32,
                         .numGroups = 1,
                         .maxSourceChannels = 1,
                         .sourceChannelPolicy = SourceChannelPolicy::Discrete};
  auto transport = std::make_unique<TransportController>(nullptr, config);
  transport->setPreparedSourceMaxFrames(rate);
  SeekCallback callback;
  transport->setCallback(&callback);
  ASSERT_EQ(transport->registerClipAudio(1, sourcePath(rate, 1, rate)), SessionGraphError::OK);
  auto metadata = transport->getClipMetadata(1);
  ASSERT_TRUE(metadata.has_value());
  metadata->playbackRate = 4.0;
  ASSERT_EQ(transport->updateClipMetadata(1, *metadata), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);
  std::vector<float> output(block);
  float* buffers[] = {output.data()};
  transport->processAudio(buffers, 1, block);
  const int64_t target = 2 * static_cast<int64_t>(StreamingClipSource::kPageFrames) - 32;
  ASSERT_EQ(transport->seekClip(1, target), SessionGraphError::OK);
  RtGuardState::reset();
  {
    RtSection section;
    transport->processAudio(buffers, 1, block);
  }
  transport->processCallbacks();
  EXPECT_GT(std::abs(trailingMean(output)), 0.05f);
  EXPECT_EQ(callback.underruns.load(), 0);
  EXPECT_EQ(RtGuardState::totalViolations(), 0u);
}

TEST_F(StreamingSeekMatrixTest, TwoQueuedDistantSeeksRenderFinalAcceptedTarget) {
  constexpr uint32_t rate = 48000;
  constexpr uint32_t block = 1024;
  TransportConfig config{.sampleRate = rate,
                         .outputChannels = 1,
                         .maxBlockFrames = block,
                         .maxActiveVoices = 32,
                         .numGroups = 1,
                         .maxSourceChannels = 1,
                         .sourceChannelPolicy = SourceChannelPolicy::Discrete};
  auto transport = std::make_unique<TransportController>(nullptr, config);
  transport->setPreparedSourceMaxFrames(rate);
  SeekCallback callback;
  transport->setCallback(&callback);
  ASSERT_EQ(transport->registerClipAudio(1, sourcePath(rate, 1, rate)), SessionGraphError::OK);
  auto metadata = transport->getClipMetadata(1);
  ASSERT_TRUE(metadata.has_value());
  metadata->playbackRate = 4.0;
  ASSERT_EQ(transport->updateClipMetadata(1, *metadata), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);
  std::vector<float> output(block);
  float* buffers[] = {output.data()};
  transport->processAudio(buffers, 1, block);

  const int64_t firstTarget = 4 * static_cast<int64_t>(StreamingClipSource::kPageFrames) - 32;
  const int64_t finalTarget = static_cast<int64_t>(6.75 * static_cast<double>(rate));
  ASSERT_EQ(transport->seekClip(1, firstTarget), SessionGraphError::OK);
  ASSERT_EQ(transport->seekClip(1, finalTarget), SessionGraphError::OK);
  RtGuardState::reset();
  {
    RtSection section;
    transport->processAudio(buffers, 1, block);
  }
  transport->processCallbacks();
  EXPECT_NEAR(trailingMean(output), -0.20f, 0.12f);
  EXPECT_EQ(callback.seeks.load(), 2);
  EXPECT_EQ(callback.underruns.load(), 0);
  EXPECT_EQ(RtGuardState::totalViolations(), 0u);
}

TEST_F(StreamingSeekMatrixTest, CommandQueueSaturationRejectsBeforePriming) {
  constexpr uint32_t rate = 48000;
  constexpr uint32_t block = 512;
  TransportConfig config{.sampleRate = rate,
                         .outputChannels = 1,
                         .maxBlockFrames = block,
                         .maxActiveVoices = 32,
                         .numGroups = 1,
                         .maxSourceChannels = 1,
                         .sourceChannelPolicy = SourceChannelPolicy::Discrete};
  auto transport = std::make_unique<TransportController>(nullptr, config);
  transport->setPreparedSourceMaxFrames(rate);
  SeekCallback callback;
  transport->setCallback(&callback);
  ASSERT_EQ(transport->registerClipAudio(1, sourcePath(rate, 1, rate)), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);

  std::vector<float> output(block);
  float* buffers[] = {output.data()};
  transport->processAudio(buffers, 1, block);
  const int64_t priorPosition = transport->getClipPosition(1);
  for (size_t index = 0; index < 255; ++index) {
    ASSERT_EQ(transport->updateClipGain(1, 0.0f), SessionGraphError::OK);
  }
  EXPECT_EQ(transport->seekClip(1, static_cast<int64_t>(6.75 * static_cast<double>(rate))),
            SessionGraphError::NotReady);
  transport->processAudio(buffers, 1, block);
  transport->processCallbacks();
  EXPECT_GT(transport->getClipPosition(1), priorPosition);
  EXPECT_EQ(callback.seeks.load(), 0);
  EXPECT_EQ(callback.underruns.load(), 0);
}
TEST_F(StreamingSeekMatrixTest, QueueFullTrimUpdatePreservesOldLoopAnchor) {
  constexpr uint32_t rate = 48000;
  constexpr uint32_t block = 512;
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  const int64_t trimOut = 6 * page + 4000;
  TransportConfig config{.sampleRate = rate,
                         .outputChannels = 1,
                         .maxBlockFrames = block,
                         .maxActiveVoices = 32,
                         .numGroups = 1,
                         .maxSourceChannels = 1,
                         .sourceChannelPolicy = SourceChannelPolicy::Discrete};
  auto transport = std::make_unique<TransportController>(nullptr, config);
  SeekCallback callback;
  configureLoopingTransport(*transport, callback, sourcePath(rate, 1, rate), 2 * page, trimOut,
                            rate);
  std::vector<float> output(block);
  float* buffers[] = {output.data()};
  transport->processAudio(buffers, 1, block);
  auto source = TransportControllerTestAccess::source(*transport, 1);
  ASSERT_TRUE(source);
  const size_t anchorIndex = StreamingClipSourceTestAccess::residentPageIndex(*source, 2 * page);
  ASSERT_LT(anchorIndex, StreamingClipSource::kNumPages);
  const uint32_t anchorGuard = StreamingClipSourceTestAccess::pageGuard(*source, anchorIndex);
  ASSERT_GT(anchorGuard, 0u);

  const auto before = transport->getClipMetadata(1);
  ASSERT_TRUE(before.has_value());
  for (size_t index = 0; index < 255; ++index) {
    ASSERT_EQ(transport->updateClipGain(1, before->gainDb), SessionGraphError::OK);
  }
  EXPECT_EQ(transport->updateClipTrimPoints(1, 3 * page + 100, before->trimOutSamples),
            SessionGraphError::NotReady);
  const auto after = transport->getClipMetadata(1);
  ASSERT_TRUE(after.has_value());
  EXPECT_EQ(after->trimInSamples, before->trimInSamples);
  EXPECT_EQ(after->trimOutSamples, before->trimOutSamples);
  EXPECT_EQ(StreamingClipSourceTestAccess::pageStart(*source, anchorIndex), 2 * page);
  EXPECT_EQ(StreamingClipSourceTestAccess::pageGuard(*source, anchorIndex), anchorGuard);

  drainAndCrossLoop(*transport, callback, output, block, before->trimOutSamples);
}

TEST_F(StreamingSeekMatrixTest, QueueFullLoopTogglePreservesOldLoopAnchor) {
  constexpr uint32_t rate = 48000;
  constexpr uint32_t block = 512;
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  const int64_t trimOut = 6 * page + 4000;
  TransportConfig config{.sampleRate = rate,
                         .outputChannels = 1,
                         .maxBlockFrames = block,
                         .maxActiveVoices = 32,
                         .numGroups = 1,
                         .maxSourceChannels = 1,
                         .sourceChannelPolicy = SourceChannelPolicy::Discrete};
  auto transport = std::make_unique<TransportController>(nullptr, config);
  SeekCallback callback;
  configureLoopingTransport(*transport, callback, sourcePath(rate, 1, rate), 2 * page, trimOut,
                            rate);
  std::vector<float> output(block);
  float* buffers[] = {output.data()};
  transport->processAudio(buffers, 1, block);
  auto source = TransportControllerTestAccess::source(*transport, 1);
  ASSERT_TRUE(source);
  const size_t anchorIndex = StreamingClipSourceTestAccess::residentPageIndex(*source, 2 * page);
  ASSERT_LT(anchorIndex, StreamingClipSource::kNumPages);
  const uint32_t anchorGuard = StreamingClipSourceTestAccess::pageGuard(*source, anchorIndex);
  ASSERT_GT(anchorGuard, 0u);

  const auto before = transport->getClipMetadata(1);
  ASSERT_TRUE(before.has_value());
  for (size_t index = 0; index < 255; ++index) {
    ASSERT_EQ(transport->updateClipGain(1, before->gainDb), SessionGraphError::OK);
  }
  EXPECT_EQ(transport->setClipLoopMode(1, false), SessionGraphError::NotReady);
  const auto after = transport->getClipMetadata(1);
  ASSERT_TRUE(after.has_value());
  EXPECT_TRUE(after->loopEnabled);
  EXPECT_EQ(StreamingClipSourceTestAccess::pageStart(*source, anchorIndex), 2 * page);
  EXPECT_EQ(StreamingClipSourceTestAccess::pageGuard(*source, anchorIndex), anchorGuard);

  drainAndCrossLoop(*transport, callback, output, block, before->trimOutSamples);
}

TEST_F(StreamingSeekMatrixTest, QueueFullMetadataUpdatePreservesOldLoopAnchor) {
  constexpr uint32_t rate = 48000;
  constexpr uint32_t block = 512;
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  const int64_t trimOut = 6 * page + 4000;
  TransportConfig config{.sampleRate = rate,
                         .outputChannels = 1,
                         .maxBlockFrames = block,
                         .maxActiveVoices = 32,
                         .numGroups = 1,
                         .maxSourceChannels = 1,
                         .sourceChannelPolicy = SourceChannelPolicy::Discrete};
  auto transport = std::make_unique<TransportController>(nullptr, config);
  SeekCallback callback;
  configureLoopingTransport(*transport, callback, sourcePath(rate, 1, rate), 2 * page, trimOut,
                            rate);
  std::vector<float> output(block);
  float* buffers[] = {output.data()};
  transport->processAudio(buffers, 1, block);
  auto source = TransportControllerTestAccess::source(*transport, 1);
  ASSERT_TRUE(source);
  const size_t anchorIndex = StreamingClipSourceTestAccess::residentPageIndex(*source, 2 * page);
  ASSERT_LT(anchorIndex, StreamingClipSource::kNumPages);
  const uint32_t anchorGuard = StreamingClipSourceTestAccess::pageGuard(*source, anchorIndex);
  ASSERT_GT(anchorGuard, 0u);

  const auto before = transport->getClipMetadata(1);
  ASSERT_TRUE(before.has_value());
  for (size_t index = 0; index < 255; ++index) {
    ASSERT_EQ(transport->updateClipGain(1, before->gainDb), SessionGraphError::OK);
  }
  auto replacement = *before;
  replacement.trimInSamples = 3 * page + 100;
  replacement.gainDb = -3.0f;
  EXPECT_EQ(transport->updateClipMetadata(1, replacement), SessionGraphError::NotReady);
  const auto after = transport->getClipMetadata(1);
  ASSERT_TRUE(after.has_value());
  EXPECT_EQ(after->trimInSamples, before->trimInSamples);
  EXPECT_FLOAT_EQ(after->gainDb, before->gainDb);
  EXPECT_EQ(after->loopEnabled, before->loopEnabled);
  EXPECT_EQ(StreamingClipSourceTestAccess::pageStart(*source, anchorIndex), 2 * page);
  EXPECT_EQ(StreamingClipSourceTestAccess::pageGuard(*source, anchorIndex), anchorGuard);

  drainAndCrossLoop(*transport, callback, output, block, before->trimOutSamples);
}

TEST_F(StreamingSeekMatrixTest, UnrelatedVoiceDoesNotRetainStoppedSourceStartPrime) {
  constexpr uint32_t rate = 48000;
  constexpr uint32_t block = 512;
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  TransportConfig config{.sampleRate = rate,
                         .outputChannels = 1,
                         .maxBlockFrames = block,
                         .maxActiveVoices = 32,
                         .numGroups = 1,
                         .maxSourceChannels = 1,
                         .sourceChannelPolicy = SourceChannelPolicy::Discrete};
  auto transport = std::make_unique<TransportController>(nullptr, config);
  SeekCallback callback;
  transport->setPreparedSourceMaxFrames(rate);
  transport->setCallback(&callback);
  ASSERT_EQ(transport->registerClipAudio(1, sourcePath(rate, 1, rate)), SessionGraphError::OK);
  ASSERT_EQ(transport->registerClipAudio(2, sourcePath(rate, 1, rate)), SessionGraphError::OK);
  ASSERT_EQ(transport->updateClipTrimPoints(1, 2 * page, 6 * page + 4000), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);
  std::vector<float> output(block);
  float* buffers[] = {output.data()};
  transport->processAudio(buffers, 1, block);
  ASSERT_EQ(transport->startClip(2), SessionGraphError::OK);
  transport->processAudio(buffers, 1, block);
  ASSERT_TRUE(transport->isClipPlaying(1));
  ASSERT_TRUE(transport->isClipPlaying(2));

  ASSERT_EQ(transport->stopClip(1), SessionGraphError::OK);
  for (size_t index = 0; index < 64; ++index) {
    transport->processAudio(buffers, 1, block);
  }
  EXPECT_FALSE(transport->isClipPlaying(1));
  EXPECT_TRUE(transport->isClipPlaying(2));
  EXPECT_EQ(transport->unregisterClipAudio(1), SessionGraphError::OK);
}

TEST_F(StreamingSeekMatrixTest, AdmittedLoopTransitionsRetainPinsUntilAudioConsumption) {
  constexpr uint32_t rate = 48000;
  constexpr uint32_t block = 512;
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  const int64_t trimOut = 6 * page + 4000;
  TransportConfig config{.sampleRate = rate,
                         .outputChannels = 1,
                         .maxBlockFrames = block,
                         .maxActiveVoices = 32,
                         .numGroups = 1,
                         .maxSourceChannels = 1,
                         .sourceChannelPolicy = SourceChannelPolicy::Discrete};
  auto transport = std::make_unique<TransportController>(nullptr, config);
  SeekCallback callback;
  configureLoopingTransport(*transport, callback, sourcePath(rate, 1, rate), 2 * page, trimOut,
                            rate);
  std::vector<float> output(block);
  float* buffers[] = {output.data()};
  transport->processAudio(buffers, 1, block);
  // Consume the start-prime page first, leaving only the persistent old anchor.
  ASSERT_EQ(transport->seekClip(1, 5 * page + 100), SessionGraphError::OK);
  transport->processAudio(buffers, 1, block);
  auto source = TransportControllerTestAccess::source(*transport, 1);
  ASSERT_TRUE(source);
  const size_t oldIndex = StreamingClipSourceTestAccess::residentPageIndex(*source, 2 * page);
  ASSERT_LT(oldIndex, StreamingClipSource::kNumPages);
  // The seeked voice's program wraps to trim-in, so its window keeps the anchor page too. A
  // command-pool page then carries the audio-owned voice-demand lease beside the anchor's pin;
  // a steady-window page carries the anchor's pin alone.
  const uint32_t oldBaseline = StreamingClipSourceTestAccess::pageGuard(*source, oldIndex);
  ASSERT_EQ(oldBaseline, oldIndex >= StreamingClipSource::kWindowPages ? 2u : 1u);

  ASSERT_EQ(transport->updateClipTrimPoints(1, 3 * page + 100, trimOut), SessionGraphError::OK);
  const size_t intermediateIndex =
      StreamingClipSourceTestAccess::residentPageIndex(*source, 3 * page);
  ASSERT_LT(intermediateIndex, StreamingClipSource::kNumPages);
  // Preparing a later command must not mutate the already-admitted transition.
  ASSERT_EQ(transport->updateClipTrimPoints(1, 4 * page + 100, trimOut), SessionGraphError::OK);
  const size_t finalIndex = StreamingClipSourceTestAccess::residentPageIndex(*source, 4 * page);
  ASSERT_LT(finalIndex, StreamingClipSource::kNumPages);
  EXPECT_EQ(StreamingClipSourceTestAccess::pageGuard(*source, oldIndex), oldBaseline);
  EXPECT_EQ(StreamingClipSourceTestAccess::pageGuard(*source, intermediateIndex), 1u);
  EXPECT_EQ(StreamingClipSourceTestAccess::pageGuard(*source, finalIndex), 1u);

  std::barrier consume(2);
  const auto controlThread = std::this_thread::get_id();
  std::thread::id audioThread;
  RtGuardState::reset();
  std::thread audio([&]() {
    audioThread = std::this_thread::get_id();
    consume.arrive_and_wait();
    RtSection section;
    // Drain production commands without rendering/retiring pages afterward,
    // so the ownership boundary is observed independently of the demand window.
    TransportControllerTestAccess::consumeCommands(*transport);
  });
  EXPECT_FALSE(StreamingClipSourceTestAccess::claimPage(*source, oldIndex));
  EXPECT_FALSE(StreamingClipSourceTestAccess::claimPage(*source, intermediateIndex));
  consume.arrive_and_wait();
  audio.join();

  EXPECT_NE(audioThread, controlThread);
  EXPECT_EQ(StreamingClipSourceTestAccess::pageStart(*source, oldIndex), 2 * page);
  EXPECT_EQ(StreamingClipSourceTestAccess::pageStart(*source, intermediateIndex), 3 * page);
  EXPECT_EQ(StreamingClipSourceTestAccess::pageGuard(*source, oldIndex), oldBaseline - 1);
  EXPECT_EQ(StreamingClipSourceTestAccess::pageGuard(*source, intermediateIndex), 0u);
  EXPECT_EQ(StreamingClipSourceTestAccess::pageGuard(*source, finalIndex), 1u);
  EXPECT_EQ(RtGuardState::totalViolations(), 0u);
}

TEST_F(StreamingSeekMatrixTest, RejectedPreparedLoopCommandRollsBackBeforeOwnershipTransfer) {
  constexpr uint32_t rate = 48000;
  constexpr uint32_t block = 512;
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  const int64_t trimOut = 6 * page + 4000;
  TransportConfig config{.sampleRate = rate,
                         .outputChannels = 1,
                         .maxBlockFrames = block,
                         .maxActiveVoices = 32,
                         .numGroups = 1,
                         .maxSourceChannels = 1,
                         .sourceChannelPolicy = SourceChannelPolicy::Discrete};
  auto transport = std::make_unique<TransportController>(nullptr, config);
  SeekCallback callback;
  configureLoopingTransport(*transport, callback, sourcePath(rate, 1, rate), 2 * page, trimOut,
                            rate);
  std::vector<float> output(block);
  float* buffers[] = {output.data()};
  transport->processAudio(buffers, 1, block);
  auto source = TransportControllerTestAccess::source(*transport, 1);
  ASSERT_TRUE(source);
  const size_t oldIndex = StreamingClipSourceTestAccess::residentPageIndex(*source, 2 * page);
  ASSERT_LT(oldIndex, StreamingClipSource::kNumPages);
  const uint32_t oldGuard = StreamingClipSourceTestAccess::pageGuard(*source, oldIndex);
  const auto before = transport->getClipMetadata(1);
  ASSERT_TRUE(before.has_value());
  const auto ingressBefore = transport->getCommandIngressTelemetry();
  size_t replacementIndex = StreamingClipSource::kNumPages;

  ASSERT_EQ(TransportControllerTestAccess::rejectPreparedLoopTransition(
                *transport, 1, 3 * page + 100, replacementIndex),
            SessionGraphError::OK);
  ASSERT_LT(replacementIndex, StreamingClipSource::kNumPages);
  EXPECT_NE(replacementIndex, oldIndex);
  EXPECT_EQ(StreamingClipSourceTestAccess::pageStart(*source, oldIndex), 2 * page);
  EXPECT_EQ(StreamingClipSourceTestAccess::pageGuard(*source, oldIndex), oldGuard);
  EXPECT_EQ(StreamingClipSourceTestAccess::pageGuard(*source, replacementIndex), 0u);
  EXPECT_FALSE(StreamingClipSourceTestAccess::claimPage(*source, oldIndex));
  const auto after = transport->getClipMetadata(1);
  ASSERT_TRUE(after.has_value());
  EXPECT_EQ(after->trimInSamples, before->trimInSamples);
  EXPECT_EQ(after->trimOutSamples, before->trimOutSamples);
  EXPECT_EQ(transport->getCommandIngressTelemetry().preparationRejectedCount,
            ingressBefore.preparationRejectedCount + 1);

  StreamingClipSource::LoopAnchorTransition inspection{};
  ASSERT_EQ(source->prepareLoopAnchorTransition(0, 1, false, inspection), SessionGraphError::OK);
  EXPECT_EQ(inspection.previousPageMask, static_cast<uint16_t>(1u << oldIndex));
  EXPECT_EQ(inspection.previousStart, 2 * page);
  source->rollbackLoopAnchorTransition(inspection);
  drainAndCrossLoop(*transport, callback, output, block, trimOut);
}

TEST_F(StreamingSeekMatrixTest, QueuedLoopNudgesReleaseSupersededAnchorsInOrder) {
  constexpr uint32_t rate = 48000;
  constexpr uint32_t block = 512;
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  const int64_t trimOut = 6 * page + 4000;
  TransportConfig config{.sampleRate = rate,
                         .outputChannels = 1,
                         .maxBlockFrames = block,
                         .maxActiveVoices = 32,
                         .numGroups = 1,
                         .maxSourceChannels = 1,
                         .sourceChannelPolicy = SourceChannelPolicy::Discrete};
  auto transport = std::make_unique<TransportController>(nullptr, config);
  SeekCallback callback;
  configureLoopingTransport(*transport, callback, sourcePath(rate, 1, rate), 2 * page, trimOut,
                            rate);
  std::vector<float> output(block);
  float* buffers[] = {output.data()};
  transport->processAudio(buffers, 1, block);

  for (const int64_t trimIn : {3 * page + 100, 4 * page + 100, 5 * page + 100}) {
    ASSERT_EQ(transport->updateClipTrimPoints(1, trimIn, trimOut), SessionGraphError::OK);
  }
  drainAndCrossLoop(*transport, callback, output, block, trimOut);
  auto finalMetadata = transport->getClipMetadata(1);
  ASSERT_TRUE(finalMetadata.has_value());
  EXPECT_EQ(finalMetadata->trimInSamples, 5 * page + 100);

  ASSERT_EQ(transport->updateClipTrimPoints(1, 6 * page + 100, trimOut), SessionGraphError::OK);
  transport->processAudio(buffers, 1, block);
  EXPECT_EQ(callback.underruns.load(), 0);
}

TEST_F(StreamingSeekMatrixTest, PendingSeekBlocksUnregisterAndReplacementUntilConsumed) {
  constexpr uint32_t rate = 48000;
  constexpr uint32_t block = 512;
  TransportConfig config{.sampleRate = rate,
                         .outputChannels = 1,
                         .maxBlockFrames = block,
                         .maxActiveVoices = 32,
                         .numGroups = 1,
                         .maxSourceChannels = 1,
                         .sourceChannelPolicy = SourceChannelPolicy::Discrete};
  auto transport = std::make_unique<TransportController>(nullptr, config);
  transport->setPreparedSourceMaxFrames(rate);
  ASSERT_EQ(transport->registerClipAudio(1, sourcePath(rate, 1, rate)), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);
  std::vector<float> output(block);
  float* buffers[] = {output.data()};
  transport->processAudio(buffers, 1, block);

  ASSERT_EQ(transport->seekClip(1, static_cast<int64_t>(6.75 * static_cast<double>(rate))),
            SessionGraphError::OK);
  EXPECT_EQ(transport->unregisterClipAudio(1), SessionGraphError::NotReady);
  EXPECT_EQ(transport->registerClipAudio(1, sourcePath(rate, 1, rate)),
            SessionGraphError::NotReady);
  transport->processAudio(buffers, 1, block);
  EXPECT_EQ(transport->unregisterClipAudio(1), SessionGraphError::NotReady);
  ASSERT_EQ(transport->panic(), SessionGraphError::OK);
  transport->processAudio(buffers, 1, block);
  ASSERT_EQ(transport->unregisterClipAudio(1), SessionGraphError::OK);
  EXPECT_EQ(transport->registerClipAudio(1, sourcePath(rate, 1, rate)), SessionGraphError::OK);
}

TEST_F(StreamingSeekMatrixTest, QueuedStartAndExactEofSeekBlockUnregisterUntilConsumed) {
  constexpr uint32_t rate = 48000;
  constexpr uint32_t block = 512;
  TransportConfig config{.sampleRate = rate,
                         .outputChannels = 1,
                         .maxBlockFrames = block,
                         .maxActiveVoices = 32,
                         .numGroups = 1,
                         .maxSourceChannels = 1,
                         .sourceChannelPolicy = SourceChannelPolicy::Discrete};
  auto transport = std::make_unique<TransportController>(nullptr, config);
  transport->setPreparedSourceMaxFrames(rate);
  ASSERT_EQ(transport->registerClipAudio(1, sourcePath(rate, 1, rate)), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);
  EXPECT_EQ(transport->unregisterClipAudio(1), SessionGraphError::NotReady);
  std::vector<float> output(block);
  float* buffers[] = {output.data()};
  transport->processAudio(buffers, 1, block);
  ASSERT_EQ(transport->seekClip(1, 9 * static_cast<int64_t>(rate)), SessionGraphError::OK);
  EXPECT_EQ(transport->unregisterClipAudio(1), SessionGraphError::NotReady);
  transport->processAudio(buffers, 1, block);
  ASSERT_EQ(transport->panic(), SessionGraphError::OK);
  transport->processAudio(buffers, 1, block);
  ASSERT_EQ(transport->unregisterClipAudio(1), SessionGraphError::OK);
}

TEST_F(StreamingSeekMatrixTest, LoopSeekAtOrPastTrimOutPrimesEvictedTrimIn) {
  constexpr uint32_t rate = 48000;
  constexpr uint32_t block = 1024;
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  const int64_t trimIn = 3 * page + 100;
  const int64_t trimOut = 4 * page + 2000;
  const int64_t fileLength = 9 * static_cast<int64_t>(rate);
  const int64_t evictionTarget = 5 * page + 1000;

  for (const int64_t target : {trimOut, fileLength}) {
    SCOPED_TRACE("target=" + std::to_string(target));
    TransportConfig config{.sampleRate = rate,
                           .outputChannels = 1,
                           .maxBlockFrames = block,
                           .maxActiveVoices = 32,
                           .numGroups = 1,
                           .maxSourceChannels = 1,
                           .sourceChannelPolicy = SourceChannelPolicy::Discrete};
    auto transport = std::make_unique<TransportController>(nullptr, config);
    transport->setPreparedSourceMaxFrames(rate);
    SeekCallback callback;
    transport->setCallback(&callback);
    ASSERT_EQ(transport->registerClipAudio(1, sourcePath(rate, 1, rate)), SessionGraphError::OK);
    ASSERT_EQ(transport->updateClipTrimPoints(1, trimIn, fileLength), SessionGraphError::OK);
    ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);

    std::vector<float> output(block);
    float* buffers[] = {output.data()};
    transport->processAudio(buffers, 1, block);

    // Render far from trim-IN so read() retires its initial worker page.
    ASSERT_EQ(transport->seekClip(1, evictionTarget), SessionGraphError::OK);
    transport->processAudio(buffers, 1, block);

    ASSERT_EQ(transport->updateClipTrimPoints(1, trimIn, trimOut), SessionGraphError::OK);
    ASSERT_EQ(transport->setClipLoopMode(1, true), SessionGraphError::OK);
    ASSERT_EQ(transport->seekClip(1, target), SessionGraphError::OK);
    RtGuardState::reset();
    {
      RtSection section;
      transport->processAudio(buffers, 1, block);
    }
    transport->processCallbacks();

    EXPECT_NEAR(trailingMean(output), -0.60f, 0.12f);
    EXPECT_EQ(callback.seeks.load(), 2);
    EXPECT_EQ(callback.underruns.load(), 0);
    EXPECT_EQ(RtGuardState::totalViolations(), 0u);
  }
}

TEST_F(StreamingSeekMatrixTest, StartUsesCommandPrimeWhenSteadyWindowIsFull) {
  constexpr uint32_t rate = 48000;
  constexpr uint32_t block = 1024;
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  const int64_t trimIn = 3 * page + 100;
  const int64_t fileLength = 9 * static_cast<int64_t>(rate);
  const int64_t evictionTarget = 5 * page + 1000;

  TransportConfig config{.sampleRate = rate,
                         .outputChannels = 1,
                         .maxBlockFrames = block,
                         .maxActiveVoices = 32,
                         .numGroups = 1,
                         .maxSourceChannels = 1,
                         .sourceChannelPolicy = SourceChannelPolicy::Discrete};
  auto transport = std::make_unique<TransportController>(nullptr, config);
  transport->setPreparedSourceMaxFrames(rate);
  SeekCallback callback;
  transport->setCallback(&callback);
  ASSERT_EQ(transport->registerClipAudio(1, sourcePath(rate, 1, rate)), SessionGraphError::OK);
  ASSERT_EQ(transport->updateClipTrimPoints(1, trimIn, fileLength), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);

  std::vector<float> output(block);
  float* buffers[] = {output.data()};
  transport->processAudio(buffers, 1, block);

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // The target page is already resident, so this advances demand without
  // consuming command-prime capacity. The worker then fills the six steady
  // slots around the target while trim-IN remains evicted.
  ASSERT_EQ(transport->seekClip(1, evictionTarget), SessionGraphError::OK);
  transport->processAudio(buffers, 1, block);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // With every steady slot occupied, start preparation must use the remaining
  // command-prime capacity rather than returning NotReady.
  ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);
  EXPECT_EQ(transport->unregisterClipAudio(1), SessionGraphError::NotReady);

  RtGuardState::reset();
  {
    RtSection section;
    transport->processAudio(buffers, 1, block);
  }
  transport->processCallbacks();

  EXPECT_NEAR(trailingMean(output), -0.80f, 0.12f);
  EXPECT_EQ(callback.underruns.load(), 0);
  EXPECT_EQ(RtGuardState::totalViolations(), 0u);
  EXPECT_EQ(transport->unregisterClipAudio(1), SessionGraphError::NotReady);

  // Remove the voice and prove the Start reservation was released after its
  // first render rather than leaking into the registry lifetime.
  ASSERT_EQ(transport->panic(), SessionGraphError::OK);
  transport->processAudio(buffers, 1, block);
  ASSERT_EQ(transport->unregisterClipAudio(1), SessionGraphError::OK);
}

TEST_F(StreamingSeekMatrixTest, SeekBeforeEvictedTrimInPrimesEffectiveFirstRender) {
  constexpr uint32_t rate = 48000;
  constexpr uint32_t block = 1024;
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  const int64_t trimIn = 3 * page + 100;
  const int64_t fileLength = 9 * static_cast<int64_t>(rate);
  const int64_t evictionTarget = 5 * page + 1000;
  const int64_t preTrimTarget = 2 * page + 1000;

  for (const bool looping : {false, true}) {
    SCOPED_TRACE(looping ? "looping" : "non-looping");
    TransportConfig config{.sampleRate = rate,
                           .outputChannels = 1,
                           .maxBlockFrames = block,
                           .maxActiveVoices = 32,
                           .numGroups = 1,
                           .maxSourceChannels = 1,
                           .sourceChannelPolicy = SourceChannelPolicy::Discrete};
    auto transport = std::make_unique<TransportController>(nullptr, config);
    transport->setPreparedSourceMaxFrames(rate);
    SeekCallback callback;
    transport->setCallback(&callback);
    ASSERT_EQ(transport->registerClipAudio(1, sourcePath(rate, 1, rate)), SessionGraphError::OK);
    ASSERT_EQ(transport->updateClipTrimPoints(1, trimIn, fileLength), SessionGraphError::OK);
    ASSERT_EQ(transport->setClipLoopMode(1, looping), SessionGraphError::OK);
    ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);

    std::vector<float> output(block);
    float* buffers[] = {output.data()};
    transport->processAudio(buffers, 1, block);

    // Evict the trim-IN page from the steady window before the pre-trim seek.
    ASSERT_EQ(transport->seekClip(1, evictionTarget), SessionGraphError::OK);
    transport->processAudio(buffers, 1, block);

    ASSERT_EQ(transport->seekClip(1, preTrimTarget), SessionGraphError::OK);
    RtGuardState::reset();
    {
      RtSection section;
      transport->processAudio(buffers, 1, block);
    }
    transport->processCallbacks();

    EXPECT_NEAR(trailingMean(output), -0.60f, 0.12f);
    EXPECT_EQ(callback.seeks.load(), 2);
    EXPECT_EQ(callback.underruns.load(), 0);
    EXPECT_EQ(RtGuardState::totalViolations(), 0u);
  }
}

TEST_F(StreamingSeekMatrixTest, SegmentProgramSeekPrimesEveryPossibleFirstBlockStart) {
  constexpr uint32_t rate = 48000;
  constexpr uint32_t block = 1024;
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  TransportConfig config{.sampleRate = rate,
                         .outputChannels = 1,
                         .maxBlockFrames = block,
                         .maxActiveVoices = 32,
                         .numGroups = 1,
                         .maxSourceChannels = 1,
                         .sourceChannelPolicy = SourceChannelPolicy::Discrete};
  auto transport = std::make_unique<TransportController>(nullptr, config);
  transport->setPreparedSourceMaxFrames(rate);
  SeekCallback callback;
  transport->setCallback(&callback);
  ASSERT_EQ(transport->registerClipAudio(1, sourcePath(rate, 1, rate)), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);
  std::vector<float> output(block);
  float* buffers[] = {output.data()};
  transport->processAudio(buffers, 1, block);

  auto metadata = transport->getClipMetadata(1);
  ASSERT_TRUE(metadata.has_value());
  metadata->playbackRate = 4.0;
  metadata->loopEnabled = true;
  metadata->segmentCount = 2;
  metadata->segments[0] = {3 * page + 100, 3 * page + 2000, 1};
  metadata->segments[1] = {5 * page + 100, 5 * page + 5000, 1};
  ASSERT_EQ(transport->updateClipMetadata(1, *metadata), SessionGraphError::OK);
  ASSERT_EQ(transport->seekClip(1, 3 * page + 1500), SessionGraphError::OK);
  RtGuardState::reset();
  {
    RtSection section;
    transport->processAudio(buffers, 1, block);
  }
  transport->processCallbacks();
  EXPECT_EQ(callback.seeks.load(), 1);
  EXPECT_EQ(callback.underruns.load(), 0);
  EXPECT_EQ(RtGuardState::totalViolations(), 0u);
}

TEST_F(StreamingSeekMatrixTest, FadeOverlapSeekMovesBothVoicesWithoutUnderrun) {
  constexpr uint32_t rate = 48000;
  constexpr uint32_t block = 512;
  TransportConfig config{.sampleRate = rate,
                         .outputChannels = 1,
                         .maxBlockFrames = block,
                         .maxActiveVoices = 32,
                         .numGroups = 1,
                         .maxSourceChannels = 1,
                         .sourceChannelPolicy = SourceChannelPolicy::Discrete};
  auto transport = std::make_unique<TransportController>(nullptr, config);
  transport->setPreparedSourceMaxFrames(rate);
  SeekCallback callback;
  transport->setCallback(&callback);
  ASSERT_EQ(transport->registerClipAudio(1, sourcePath(rate, 1, rate)), SessionGraphError::OK);
  ASSERT_EQ(transport->setClipVoiceMode(1, VoiceMode::MonoWithFadeOverlap), SessionGraphError::OK);
  auto metadata = transport->getClipMetadata(1);
  ASSERT_TRUE(metadata.has_value());
  metadata->stopFadeOutSeconds = 0.1;
  ASSERT_EQ(transport->updateClipMetadata(1, *metadata), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);
  std::vector<float> output(block);
  float* buffers[] = {output.data()};
  transport->processAudio(buffers, 1, block);
  ASSERT_EQ(transport->stopClip(1), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);
  transport->processAudio(buffers, 1, block);
  ASSERT_EQ(transport->getTotalActiveVoiceCount(), 2u);
  ASSERT_EQ(transport->seekClip(1, static_cast<int64_t>(6.75 * static_cast<double>(rate))),
            SessionGraphError::OK);
  RtGuardState::reset();
  {
    RtSection section;
    transport->processAudio(buffers, 1, block);
  }
  transport->processCallbacks();
  EXPECT_EQ(callback.underruns.load(), 0);
  EXPECT_EQ(RtGuardState::totalViolations(), 0u);
}

TEST_F(StreamingSeekMatrixTest,
       SharedStreamingFadeOverlapKeepsBothDemandWindowsAndRemovesIdleState) {
  constexpr uint32_t rate = 48000;
  constexpr uint32_t block = 512;
  constexpr int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  TransportConfig config{.sampleRate = rate,
                         .outputChannels = 1,
                         .maxBlockFrames = block,
                         .maxActiveVoices = 32,
                         .numGroups = 1,
                         .maxSourceChannels = 1,
                         .sourceChannelPolicy = SourceChannelPolicy::Discrete};
  TransportController transport(nullptr, config);
  transport.setPreparedSourceMaxFrames(rate);
  ASSERT_EQ(transport.setMaxVoicesPerClip(2), SessionGraphError::OK);
  SeekCallback callback;
  transport.setCallback(&callback);
  ASSERT_EQ(transport.registerClipAudio(1, sourcePath(rate, 1, rate)), SessionGraphError::OK);
  ASSERT_EQ(transport.setClipVoiceMode(1, VoiceMode::MonoWithFadeOverlap), SessionGraphError::OK);
  auto metadata = transport.getClipMetadata(1);
  ASSERT_TRUE(metadata.has_value());
  metadata->stopFadeOutSeconds = 4.0;
  metadata->loopEnabled = true;
  ASSERT_EQ(transport.updateClipMetadata(1, *metadata), SessionGraphError::OK);
  ASSERT_EQ(transport.prepareClipAudio(1), SessionGraphError::OK);
  const auto sharedSource = TransportControllerTestAccess::source(transport, 1);
  ASSERT_NE(sharedSource, nullptr);

  std::array<float, block> output{};
  float* buffers[] = {output.data()};
  const auto render = [&]() {
    {
      RtSection section;
      transport.processAudio(buffers, 1, block);
    }
    transport.processCallbacks();
  };

  RtGuardState::reset();
  ASSERT_EQ(transport.startClip(1, 1), SessionGraphError::OK);
  render();
  ASSERT_EQ(transport.seekClip(1, 6 * page + 100), SessionGraphError::OK);
  render();
  ASSERT_EQ(transport.stopClip(1), SessionGraphError::OK);
  render();
  ASSERT_EQ(transport.startClip(1, 2), SessionGraphError::OK);

  StreamingClipSourceTestAccess::PublishedDemandSnapshot snapshot{};
  bool publishedOverlap = false;
  for (size_t attempt = 0; attempt < 8; ++attempt) {
    render();
    if (TransportControllerTestAccess::readSourceDemands(transport, 1, snapshot) &&
        snapshot.count == 2) {
      publishedOverlap = true;
      break;
    }
  }
  ASSERT_TRUE(publishedOverlap);
  ASSERT_EQ(transport.getActiveVoiceCount(1), 2u);
  bool sawOldCursor = false;
  bool sawFreshCursor = false;
  for (size_t voice = 0; voice < snapshot.count; ++voice) {
    sawOldCursor |= snapshot.positions[voice] > 6 * page;
    sawFreshCursor |= snapshot.positions[voice] < page;
  }
  EXPECT_TRUE(sawOldCursor);
  EXPECT_TRUE(sawFreshCursor);
  EXPECT_EQ(callback.secondRequestStarts.load(std::memory_order_relaxed), 1);
  EXPECT_EQ(callback.underruns.load(std::memory_order_relaxed), 0);
  EXPECT_EQ(TransportControllerTestAccess::source(transport, 1), sharedSource);
  EXPECT_TRUE(std::any_of(output.begin(), output.end(),
                          [](float sample) { return std::abs(sample) > 0.01f; }));
  EXPECT_EQ(RtGuardState::allocViolations(), 0u);
  EXPECT_EQ(RtGuardState::deallocViolations(), 0u);

  ASSERT_EQ(transport.panic(), SessionGraphError::OK);
  render();
  EXPECT_EQ(transport.getTotalActiveVoiceCount(), 0u);
  ASSERT_TRUE(TransportControllerTestAccess::readSourceDemands(transport, 1, snapshot));
  EXPECT_EQ(snapshot.count, 0u);
  transport.setCallback(nullptr);
}

TEST(PreparedClipSourceDecodeTest, ExactLengthAndFinalPartialChunkAreFullyDecoded) {
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  for (const int64_t length : {page, page + 37}) {
    FaultReader reader(length, 2);
    ASSERT_TRUE(reader.open("").isOk());
    auto source = PreparedClipSource::decode(reader, 2, length);
    ASSERT_NE(source, nullptr);
    EXPECT_EQ(reader.readCalls.load(), length == page ? 1u : 2u);
    EXPECT_EQ(source->lengthFrames(), length);
    EXPECT_EQ(source->numChannels(), 2u);
    std::array<float, 128> samples{};
    samples.fill(-1.0f);
    size_t framesRead = 0;
    ASSERT_TRUE(source->read(length - 17, samples.data(), 64, framesRead));
    EXPECT_EQ(framesRead, 17u);
    for (size_t frame = 0; frame < framesRead; ++frame) {
      EXPECT_FLOAT_EQ(samples[2 * frame], static_cast<float>(length - 17 + frame));
      EXPECT_FLOAT_EQ(samples[2 * frame + 1], samples[2 * frame]);
    }
    EXPECT_FLOAT_EQ(samples[34], -1.0f);
    EXPECT_TRUE(source->isFullyResident());
  }
}

TEST(PreparedClipSourceDecodeTest, EarlyZeroShortReadAndErrorPublishNothingAndRetry) {
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  for (const int fault : {0, 1, 2, 3}) {
    SCOPED_TRACE(fault);
    FaultReader reader(2 * page + 37);
    ASSERT_TRUE(reader.open("").isOk());
    if (fault == 0) {
      reader.zeroReadCall.store(2);
    } else if (fault == 1) {
      reader.shortReadCall.store(2);
    } else if (fault == 2) {
      reader.failReadOnCall(2);
    } else {
      reader.failSeek.store(true);
    }
    EXPECT_EQ(PreparedClipSource::decode(reader, 1, 2 * page + 37), nullptr);
    EXPECT_EQ(reader.readCalls.load(), fault == 3 ? 0u : 2u);
    reader.resetReadCalls();
    reader.failSeek.store(false);
    auto recovered = PreparedClipSource::decode(reader, 1, 2 * page + 37);
    ASSERT_NE(recovered, nullptr);
    EXPECT_EQ(reader.readCalls.load(), 3u);
    std::array<float, 64> samples{};
    size_t framesRead = 0;
    ASSERT_TRUE(recovered->read(page - 16, samples.data(), samples.size(), framesRead));
    EXPECT_EQ(framesRead, samples.size());
    EXPECT_FLOAT_EQ(samples.front(), static_cast<float>(page - 16));
    EXPECT_FLOAT_EQ(samples.back(), static_cast<float>(page + 47));
  }
}

TEST_F(StreamingSeekMatrixTest, FailedPreparedPublicationPreservesPreviousSourceAndPins) {
  constexpr uint32_t rate = 48000;
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  TransportConfig config{.sampleRate = rate,
                         .outputChannels = 1,
                         .maxBlockFrames = 512,
                         .maxActiveVoices = 32,
                         .numGroups = 1,
                         .maxSourceChannels = 1,
                         .sourceChannelPolicy = SourceChannelPolicy::Discrete};
  TransportController transport(nullptr, config);
  transport.setPreparedSourceMaxFrames(rate);
  const auto& path = sourcePath(rate, 1, rate);
  ASSERT_EQ(transport.registerClipAudio(1, path), SessionGraphError::OK);
  ASSERT_EQ(transport.setClipLoopMode(1, true), SessionGraphError::OK);
  ASSERT_EQ(transport.prepareClipAudio(1), SessionGraphError::OK);
  auto previous = TransportControllerTestAccess::source(transport, 1);
  ASSERT_NE(previous, nullptr);
  const size_t anchor = StreamingClipSourceTestAccess::residentPageIndex(*previous, 0);
  ASSERT_LT(anchor, StreamingClipSource::kNumPages);
  StreamingClipSource::PrimeReservation held{};
  ASSERT_EQ(previous->primeForCommand(100, 32, held), SessionGraphError::OK);
  const uint32_t oldGuard = StreamingClipSourceTestAccess::pageGuard(*previous, anchor);
  ASSERT_EQ(oldGuard, 2u);
  ASSERT_EQ(transport.registerClipAudio(2, path), SessionGraphError::OK);
  auto reader = std::make_shared<FaultReader>(page + 37);
  const auto metadata = reader->open("");
  ASSERT_TRUE(metadata.isOk());
  TransportControllerTestAccess::replaceUnpreparedReader(transport, 2, reader, metadata.value);
  transport.setPreparedSourceMaxFrames(2 * page);
  const auto registered = transport.getClipMetadata(2);
  ASSERT_TRUE(registered.has_value());
  for (const int fault : {0, 1, 2}) {
    SCOPED_TRACE(fault);
    reader->resetReadCalls();
    reader->zeroReadCall.store(fault == 0 ? 2 : 0);
    reader->shortReadCall.store(fault == 1 ? 2 : 0);
    reader->failReadOnCall(fault == 2 ? 2 : 0);
    EXPECT_EQ(transport.prepareClipAudio(2), SessionGraphError::InternalError);
    EXPECT_EQ(TransportControllerTestAccess::playbackSource(transport, 2), nullptr);
    // startClip retries preparation through the production command path.
    reader->resetReadCalls();
    reader->zeroReadCall.store(fault == 0 ? 2 : 0);
    reader->shortReadCall.store(fault == 1 ? 2 : 0);
    reader->failReadOnCall(fault == 2 ? 2 : 0);
    EXPECT_EQ(transport.startClip(2), SessionGraphError::InternalError);
    EXPECT_EQ(TransportControllerTestAccess::playbackSource(transport, 2), nullptr);
    EXPECT_EQ(transport.registerClipAudio(1, path), SessionGraphError::NotReady);
    EXPECT_EQ(TransportControllerTestAccess::source(transport, 1), previous);
    EXPECT_EQ(StreamingClipSourceTestAccess::pageGuard(*previous, anchor), oldGuard);
    EXPECT_EQ(StreamingClipSourceTestAccess::pageStart(*previous, anchor), 0);
    EXPECT_TRUE(previous->hasPendingCommandPrimes());
    const auto after = transport.getClipMetadata(2);
    ASSERT_TRUE(after.has_value());
    EXPECT_EQ(after->trimInSamples, registered->trimInSamples);
    EXPECT_EQ(after->trimOutSamples, registered->trimOutSamples);
    EXPECT_EQ(after->loopEnabled, registered->loopEnabled);
    EXPECT_EQ(transport.getTotalActiveVoiceCount(), 0u);
    std::array<float, 512> output{};
    output.fill(1.0f);
    float* channels[] = {output.data()};
    RtGuardState::reset();
    {
      RtSection section;
      transport.processAudio(channels, 1, output.size());
    }
    EXPECT_EQ(transport.getTotalActiveVoiceCount(), 0u);
    EXPECT_TRUE(
        std::all_of(output.begin(), output.end(), [](float sample) { return sample == 0.0f; }));
    EXPECT_EQ(StreamingClipSourceTestAccess::pageGuard(*previous, anchor), oldGuard);
    EXPECT_EQ(RtGuardState::totalViolations(), 0u);
  }
  reader->resetReadCalls();
  ASSERT_EQ(transport.prepareClipAudio(2), SessionGraphError::OK);
  auto recovered = TransportControllerTestAccess::playbackSource(transport, 2);
  ASSERT_NE(recovered, nullptr);
  EXPECT_TRUE(recovered->isFullyResident());
  std::array<float, 64> samples{};
  size_t framesRead = 0;
  ASSERT_TRUE(recovered->read(page, samples.data(), samples.size(), framesRead));
  EXPECT_EQ(framesRead, 37u);
  EXPECT_FLOAT_EQ(samples.front(), static_cast<float>(page));
  EXPECT_FLOAT_EQ(samples[36], static_cast<float>(page + 36));
  EXPECT_EQ(TransportControllerTestAccess::source(transport, 1), previous);
  previous->releaseCommandPrime(held);
  EXPECT_EQ(StreamingClipSourceTestAccess::pageGuard(*previous, anchor), 1u);
  EXPECT_FALSE(previous->hasPendingCommandPrimes());
}

TEST(MediaStreamWorkerServiceTest, RepeatedAttachPrunesExpiredAndPreservesPins) {
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  MediaStreamWorker worker;
  StreamingClipSourceTestAccess::parkWorker(worker);
  auto expiredReader = std::make_shared<FaultReader>(12 * page);
  ASSERT_TRUE(expiredReader->open("").isOk());
  auto expired = std::make_shared<StreamingClipSource>(expiredReader, 1, 12 * page);
  worker.attach(expired);
  expired.reset();
  EXPECT_TRUE(StreamingClipSourceTestAccess::hasExpiredRegistration(worker));
  auto reader = std::make_shared<FaultReader>(12 * page);
  ASSERT_TRUE(reader->open("").isOk());
  auto source = std::make_shared<StreamingClipSource>(reader, 1, 12 * page);
  ASSERT_EQ(source->prefill(0, 1), SessionGraphError::OK);
  StreamingClipSource::LoopAnchorTransition anchor{};
  ASSERT_EQ(source->prepareLoopAnchorTransition(0, 1, true, anchor), SessionGraphError::OK);
  const size_t anchorIndex = static_cast<size_t>(std::countr_zero(anchor.replacementPageMask));
  source->commitLoopAnchorTransition(anchor);
  StreamingClipSource::PrimeReservation held{};
  ASSERT_EQ(source->primeForCommand(100, 32, held), SessionGraphError::OK);
  EXPECT_EQ(StreamingClipSourceTestAccess::pageGuard(*source, anchorIndex), 2u);
  auto trace = std::make_shared<ServiceTrace>();
  reader->setServiceTrace(trace, 0);
  reader->resetReadCalls();
  reader->failRead.store(true);
  source->setDemand(10 * page);
  for (size_t repeat = 0; repeat < 8; ++repeat) {
    worker.attach(source);
  }
  EXPECT_FALSE(StreamingClipSourceTestAccess::hasExpiredRegistration(worker));
  StreamingClipSourceTestAccess::WorkerDrive drive;
  for (size_t round = 0; round < 8; ++round) {
    StreamingClipSourceTestAccess::serviceRound(worker, drive);
    EXPECT_EQ(reader->readCalls.load(), round + 1)
        << "an unsuccessful decode exposes duplicate service even without cache publication";
    EXPECT_EQ(StreamingClipSourceTestAccess::pageGuard(*source, anchorIndex), 2u);
    EXPECT_TRUE(source->hasPendingCommandPrimes());
  }
  const auto observations = trace->observations();
  ASSERT_EQ(observations.size(), 8u);
  for (const auto& observation : observations) {
    EXPECT_EQ(observation.source, 0u);
    EXPECT_EQ(observation.start, 10 * page);
    EXPECT_EQ(observation.requested, StreamingClipSource::kPageFrames);
  }
  source->releaseCommandPrime(held);
  EXPECT_FALSE(source->hasPendingCommandPrimes());
  EXPECT_EQ(StreamingClipSourceTestAccess::pageGuard(*source, anchorIndex), 1u);
  StreamingClipSource::LoopAnchorTransition disable{};
  ASSERT_EQ(source->prepareLoopAnchorTransition(0, 1, false, disable), SessionGraphError::OK);
  source->commitLoopAnchorTransition(disable);
  for (size_t index = 0; index < StreamingClipSource::kNumPages; ++index) {
    EXPECT_EQ(StreamingClipSourceTestAccess::pageGuard(*source, index), 0u);
  }
  StreamingClipSourceTestAccess::rollbackDrive(drive);
  std::cout << "media_attach identity_repeats=8 rounds=8 actual_read_attempts="
            << observations.size() << " expired_reader_calls=" << expiredReader->readCalls.load()
            << " final_pins=0\n";
}

TEST(MediaStreamWorkerServiceTest, SourceCountOpportunityDistribution) {
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  for (const size_t count : {size_t{1}, size_t{2}, size_t{8}, size_t{32}}) {
    for (const bool roundRobin : {false, true}) {
      SCOPED_TRACE("sources=" + std::to_string(count) +
                   (roundRobin ? " round_robin" : " full_window_replay"));
      auto trace = std::make_shared<ServiceTrace>();
      std::vector<std::shared_ptr<StreamingClipSource>> sources;
      MediaStreamWorker worker;
      StreamingClipSourceTestAccess::parkWorker(worker);
      for (size_t index = 0; index < count; ++index) {
        auto reader = std::make_shared<FaultReader>(12 * page);
        ASSERT_TRUE(reader->open("").isOk());
        reader->setServiceTrace(trace, index);
        sources.push_back(std::make_shared<StreamingClipSource>(reader, 1, 12 * page));
        if (roundRobin) {
          worker.attach(sources.back());
        }
      }
      StreamingClipSourceTestAccess::WorkerDrive drive;
      const auto begin = std::chrono::steady_clock::now();
      if (roundRobin) {
        for (size_t round = 0; round < 5; ++round) {
          StreamingClipSourceTestAccess::serviceRound(worker, drive);
          const auto observations = trace->observations();
          ASSERT_EQ(observations.size(), (round + 1) * count);
          for (size_t offset = 0; offset < count; ++offset) {
            const auto& observation = observations[round * count + offset];
            EXPECT_EQ(observation.source, (round + offset) % count);
            EXPECT_EQ(observation.start, static_cast<int64_t>(round) * page);
          }
        }
      } else {
        // Replay the retained full-window public service entry point used by
        // the old scheduler, with the same readers/topology as the new rounds.
        for (auto& source : sources) {
          source->service();
        }
      }
      const auto end = std::chrono::steady_clock::now();
      const auto observations = trace->observations();
      ASSERT_EQ(observations.size(), 5 * count);
      std::vector<size_t> served(count, 0);
      std::vector<size_t> first(count, observations.size());
      size_t maximumSkew = 0;
      int64_t maximumFirstEntryNs = 0;
      const int64_t beginNs =
          std::chrono::duration_cast<std::chrono::nanoseconds>(begin.time_since_epoch()).count();
      for (size_t ordinal = 0; ordinal < observations.size(); ++ordinal) {
        const auto& observation = observations[ordinal];
        ASSERT_LT(observation.source, count);
        if (first[observation.source] == observations.size()) {
          first[observation.source] = ordinal;
          maximumFirstEntryNs = std::max(maximumFirstEntryNs, observation.enteredNs - beginNs);
        }
        ++served[observation.source];
        const auto [minimum, maximum] = std::minmax_element(served.begin(), served.end());
        maximumSkew = std::max(maximumSkew, *maximum - *minimum);
      }
      for (size_t index = 0; index < count; ++index) {
        EXPECT_EQ(served[index], 5u);
        EXPECT_EQ(first[index], roundRobin ? index : index * 5);
        EXPECT_FALSE(sources[index]->hasPendingCommandPrimes());
        for (size_t slot = 0; slot < StreamingClipSource::kNumPages; ++slot) {
          EXPECT_EQ(StreamingClipSourceTestAccess::pageGuard(*sources[index], slot), 0u);
        }
      }
      EXPECT_EQ(maximumSkew, count == 1 ? 0u : (roundRobin ? 1u : 5u));
      std::cout << "media_service source_count=" << count
                << " mode=" << (roundRobin ? "round_robin" : "full_window_replay")
                << " actual_reads=" << observations.size()
                << " reads_per_source=5 maximum_prefix_count_skew=" << maximumSkew
                << " last_first_opportunity=" << *std::max_element(first.begin(), first.end())
                << " maximum_first_entry_ns=" << maximumFirstEntryNs << " wall_ns="
                << std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()
                << " hard_read_bound=unqualified\n";
      StreamingClipSourceTestAccess::rollbackDrive(drive);
    }
  }
}

TEST(StreamingClipSourceDemandTest, PublishedVoiceDemandsStayCoherentDuringUpdates) {
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  auto reader = std::make_shared<FaultReader>(2048 * page);
  ASSERT_TRUE(reader->open("").isOk());
  StreamingClipSource source(reader, 1, 2048 * page);
  std::atomic<bool> done{false};
  std::atomic<bool> incoherent{false};
  std::atomic<size_t> acceptedSnapshots{0};

  std::thread publisher([&]() {
    std::array<int64_t, kVoiceDemandTestCapacity> positions{};
    for (size_t round = 0; round < 10000; ++round) {
      positions[0] = static_cast<int64_t>(round) * page;
      positions[1] = positions[0] + page;
      StreamingClipSourceTestAccess::publishDemands(source, positions, 2);
    }
    positions.fill(0);
    StreamingClipSourceTestAccess::publishDemands(source, positions, 0);
    done.store(true, std::memory_order_release);
  });

  do {
    StreamingClipSourceTestAccess::PublishedDemandSnapshot snapshot{};
    if (!StreamingClipSourceTestAccess::readPublishedDemands(source, snapshot)) {
      continue;
    }
    acceptedSnapshots.fetch_add(1, std::memory_order_relaxed);
    if (snapshot.count != 0 &&
        (snapshot.count != 2 || snapshot.positions[1] != snapshot.positions[0] + page)) {
      incoherent.store(true, std::memory_order_relaxed);
    }
  } while (!done.load(std::memory_order_acquire));
  publisher.join();

  EXPECT_FALSE(incoherent.load(std::memory_order_relaxed));
  EXPECT_GT(acceptedSnapshots.load(std::memory_order_relaxed), 0u);
  std::array<int64_t, kVoiceDemandTestCapacity> emptyPositions{};
  StreamingClipSourceTestAccess::publishDemands(source, emptyPositions, 0);
  StreamingClipSourceTestAccess::PublishedDemandSnapshot finalSnapshot{};
  ASSERT_TRUE(StreamingClipSourceTestAccess::readPublishedDemands(source, finalSnapshot));
  EXPECT_EQ(finalSnapshot.count, 0u);
}

TEST(StreamingClipSourceDemandTest, TwoVoiceWindowsRetainIndependentCurrentAndLookaheadPages) {
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  auto reader = std::make_shared<FaultReader>(20 * page);
  ASSERT_TRUE(reader->open("").isOk());
  StreamingClipSource source(reader, 1, 20 * page);
  std::array<int64_t, kVoiceDemandTestCapacity> positions{};
  positions[0] = page + 16;
  positions[1] = 10 * page + 24;
  StreamingClipSourceTestAccess::publishDemands(source, positions, 2);

  // Six steady pages are split into two three-page windows: behind, current,
  // and one forward page for each independently located cursor.
  source.service();
  for (const int64_t start : {int64_t{0}, page, 2 * page, 9 * page, 10 * page, 11 * page}) {
    EXPECT_NE(StreamingClipSourceTestAccess::residentPageIndex(source, start),
              StreamingClipSource::kNumPages)
        << "missing initial voice window page " << start;
  }

  std::array<float, 16> samples{};
  size_t framesRead = 0;
  ASSERT_TRUE(StreamingClipSourceTestAccess::readForVoice(source, 0, page + 16, samples.data(),
                                                          samples.size(), framesRead));
  ASSERT_EQ(framesRead, samples.size());
  EXPECT_FLOAT_EQ(samples[0], static_cast<float>(page + 16));
  ASSERT_TRUE(StreamingClipSourceTestAccess::readForVoice(source, 1, 10 * page + 24, samples.data(),
                                                          samples.size(), framesRead));
  ASSERT_EQ(framesRead, samples.size());
  EXPECT_FLOAT_EQ(samples[0], static_cast<float>(10 * page + 24));

  positions[0] = 2 * page + 16;
  positions[1] = 11 * page + 24;
  StreamingClipSourceTestAccess::publishDemands(source, positions, 2);
  ASSERT_TRUE(StreamingClipSourceTestAccess::readForVoice(source, 0, positions[0], samples.data(),
                                                          samples.size(), framesRead));
  ASSERT_EQ(framesRead, samples.size());
  EXPECT_FLOAT_EQ(samples[0], static_cast<float>(positions[0]));
  ASSERT_TRUE(StreamingClipSourceTestAccess::readForVoice(source, 1, positions[1], samples.data(),
                                                          samples.size(), framesRead));
  ASSERT_EQ(framesRead, samples.size());
  EXPECT_FLOAT_EQ(samples[0], static_cast<float>(positions[1]));
  source.service();

  for (const int64_t start : {page, 2 * page, 3 * page, 10 * page, 11 * page, 12 * page}) {
    EXPECT_NE(StreamingClipSourceTestAccess::residentPageIndex(source, start),
              StreamingClipSource::kNumPages)
        << "one voice read evicted another voice's current/lookahead page " << start;
  }
  EXPECT_EQ(StreamingClipSourceTestAccess::residentPageIndex(source, 0),
            StreamingClipSource::kNumPages);
  EXPECT_EQ(StreamingClipSourceTestAccess::residentPageIndex(source, 9 * page),
            StreamingClipSource::kNumPages);
}

TEST(StreamingClipSourceDemandTest, SixCurrentPageCapacityReportsMissAndRecoversAfterIdle) {
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  auto reader = std::make_shared<FaultReader>(16 * page);
  ASSERT_TRUE(reader->open("").isOk());
  StreamingClipSource source(reader, 1, 16 * page);
  std::array<int64_t, kVoiceDemandTestCapacity> positions{};
  for (size_t voice = 0; voice < 7; ++voice) {
    positions[voice] = static_cast<int64_t>(voice) * page + 8;
  }
  StreamingClipSourceTestAccess::publishDemands(source, positions, 7);
  source.service();

  std::array<float, 8> samples{};
  size_t framesRead = 0;
  for (size_t voice = 0; voice < 6; ++voice) {
    ASSERT_TRUE(StreamingClipSourceTestAccess::readForVoice(
        source, voice, positions[voice], samples.data(), samples.size(), framesRead));
    ASSERT_EQ(framesRead, samples.size());
  }
  EXPECT_FALSE(StreamingClipSourceTestAccess::readForVoice(source, 6, positions[6], samples.data(),
                                                           samples.size(), framesRead));
  EXPECT_EQ(framesRead, 0u);

  // Retire every demand before reopening the previously unsupported cursor.
  // Its miss frees obsolete pages; the same worker/source restores service.
  const int64_t recoveryPosition = positions[6];
  positions.fill(0);
  StreamingClipSourceTestAccess::publishDemands(source, positions, 0);
  positions[0] = recoveryPosition;
  StreamingClipSourceTestAccess::publishDemands(source, positions, 1);
  EXPECT_FALSE(StreamingClipSourceTestAccess::readForVoice(source, 0, positions[0], samples.data(),
                                                           samples.size(), framesRead));
  source.service();
  ASSERT_TRUE(StreamingClipSourceTestAccess::readForVoice(source, 0, positions[0], samples.data(),
                                                          samples.size(), framesRead));
  EXPECT_EQ(framesRead, samples.size());
  EXPECT_FLOAT_EQ(samples.front(), static_cast<float>(recoveryPosition));
  EXPECT_FLOAT_EQ(samples.back(), static_cast<float>(recoveryPosition + samples.size() - 1));
}

TEST(StreamingClipSourceDemandTest, CoLocatedVoicesShareOneWindowWithLookahead) {
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  auto reader = std::make_shared<FaultReader>(16 * page);
  ASSERT_TRUE(reader->open("").isOk());
  StreamingClipSource source(reader, 1, 16 * page);
  constexpr size_t kVoices = 8;
  std::array<int64_t, kVoiceDemandTestCapacity> positions{};
  for (size_t voice = 0; voice < kVoices; ++voice) {
    positions[voice] = page + 16 + static_cast<int64_t>(voice) * 64;
  }
  StreamingClipSourceTestAccess::publishDemands(source, positions, kVoices);
  source.service();

  // Eight voices on one page are one cursor: the whole steady budget forms a
  // single behind/current/four-forward window, and the audio-side reads keep it.
  std::array<float, 16> samples{};
  size_t framesRead = 0;
  for (size_t voice = 0; voice < kVoices; ++voice) {
    ASSERT_TRUE(StreamingClipSourceTestAccess::readForVoice(
        source, voice, positions[voice], samples.data(), samples.size(), framesRead));
    ASSERT_EQ(framesRead, samples.size());
    EXPECT_FLOAT_EQ(samples[0], static_cast<float>(positions[voice]));
  }
  EXPECT_NE(StreamingClipSourceTestAccess::residentPageIndex(source, 0),
            StreamingClipSource::kNumPages)
      << "missing behind page";
  for (int64_t ahead = 1; ahead <= 4; ++ahead) {
    EXPECT_NE(StreamingClipSourceTestAccess::residentPageIndex(source, (1 + ahead) * page),
              StreamingClipSource::kNumPages)
        << "missing look-ahead page " << ahead;
  }
}

TEST(StreamingClipSourceDemandTest, CoLocatedVoicesCrossPageBoundaryWithoutMiss) {
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  constexpr size_t kBlockFrames = 512;
  // Three page crossings at one worker service per audio block.
  const size_t blocks = static_cast<size_t>(3 * page) / kBlockFrames + 8;
  for (const size_t voices : {size_t{6}, size_t{8}, size_t{32}}) {
    SCOPED_TRACE(::testing::Message() << "voices=" << voices);
    ASSERT_LE(voices, kVoiceDemandTestCapacity);
    auto reader = std::make_shared<FaultReader>(16 * page);
    ASSERT_TRUE(reader->open("").isOk());
    StreamingClipSource source(reader, 1, 16 * page);
    std::array<int64_t, kVoiceDemandTestCapacity> positions{};
    for (size_t voice = 0; voice < voices; ++voice) {
      positions[voice] = page + 16 + static_cast<int64_t>(voice) * 64;
    }
    StreamingClipSourceTestAccess::publishDemands(source, positions, voices);
    source.service();

    std::vector<float> samples(kBlockFrames);
    size_t failedReads = 0;
    size_t wrongReads = 0;
    for (size_t block = 0; block < blocks; ++block) {
      StreamingClipSourceTestAccess::publishDemands(source, positions, voices);
      for (size_t voice = 0; voice < voices; ++voice) {
        size_t framesRead = 0;
        const bool hit = StreamingClipSourceTestAccess::readForVoice(
            source, voice, positions[voice], samples.data(), kBlockFrames, framesRead);
        const int64_t last = positions[voice] + static_cast<int64_t>(kBlockFrames) - 1;
        if (!hit) {
          ++failedReads;
        } else if (framesRead != kBlockFrames ||
                   samples.front() != static_cast<float>(positions[voice]) ||
                   samples.back() != static_cast<float>(last)) {
          ++wrongReads;
        }
        positions[voice] += static_cast<int64_t>(kBlockFrames);
      }
      source.service();
    }
    EXPECT_GE(positions[voices - 1], 4 * page);
    EXPECT_EQ(failedReads, 0u);
    EXPECT_EQ(wrongReads, 0u);
  }
}

TEST(StreamingClipSourceDemandTest, GroupedVoicesSplitBudgetPerDistinctCursor) {
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  auto reader = std::make_shared<FaultReader>(16 * page);
  ASSERT_TRUE(reader->open("").isOk());
  StreamingClipSource source(reader, 1, 16 * page);
  constexpr size_t kVoices = 8;
  std::array<int64_t, kVoiceDemandTestCapacity> positions{};
  // Interleave four voices on page 2 with four voices on page 10.
  for (size_t voice = 0; voice < kVoices; ++voice) {
    const int64_t groupPage = voice % 2 == 0 ? 2 * page : 10 * page;
    positions[voice] = groupPage + 16 + static_cast<int64_t>(voice) * 64;
  }
  StreamingClipSourceTestAccess::publishDemands(source, positions, kVoices);
  source.service();

  std::array<float, 16> samples{};
  size_t framesRead = 0;
  for (size_t voice = 0; voice < kVoices; ++voice) {
    ASSERT_TRUE(StreamingClipSourceTestAccess::readForVoice(
        source, voice, positions[voice], samples.data(), samples.size(), framesRead));
    ASSERT_EQ(framesRead, samples.size());
  }
  source.service();

  // Two distinct cursors split six steady pages into two three-page windows.
  for (const int64_t start : {page, 2 * page, 3 * page, 9 * page, 10 * page, 11 * page}) {
    EXPECT_NE(StreamingClipSourceTestAccess::residentPageIndex(source, start),
              StreamingClipSource::kNumPages)
        << "missing grouped voice window page " << start;
  }
  for (const int64_t start : {4 * page, 12 * page}) {
    EXPECT_EQ(StreamingClipSourceTestAccess::residentPageIndex(source, start),
              StreamingClipSource::kNumPages)
        << "grouped window exceeded its three-page share at " << start;
  }
}

TEST(StreamingClipSourceDemandTest, DifferentProgramsOnOnePageSplitTheSteadyBudget) {
  // Voices on one cursor page that follow different programs read different pages. The six
  // steady pages are therefore budgeted per program shape, not per cursor page: otherwise
  // every shape keeps a full window and together they retain more pages than the pool holds.
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  auto reader = std::make_shared<FaultReader>(20 * page);
  ASSERT_TRUE(reader->open("").isOk());
  StreamingClipSource source(reader, 1, 20 * page);
  StreamingClipSourceTestAccess::DemandArray demands{};
  demands[0] = {2 * page + 16, source.lengthFrames(), -1};
  demands[1] = {2 * page + 80, 2 * page + 1000, 12 * page + 5};
  demands[2] = {2 * page + 144, 2 * page + 2000, 16 * page + 5};
  StreamingClipSourceTestAccess::publishPrograms(source, demands, 3);

  size_t kept = 0;
  for (int64_t index = 0; index < 20; ++index) {
    kept += StreamingClipSourceTestAccess::keepsPage(source, index * page) ? 1u : 0u;
  }
  EXPECT_LE(kept, StreamingClipSource::kWindowPages);
  for (const int64_t start : {2 * page, 3 * page, 12 * page, 16 * page}) {
    EXPECT_TRUE(StreamingClipSourceTestAccess::keepsPage(source, start))
        << "each program keeps its current page and its first hop page: " << start;
  }
}

TEST(StreamingClipSourceDemandTest, TransientReadPinExcludesRetirementAndReuse) {
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  auto reader = std::make_shared<FaultReader>(8 * page);
  ASSERT_TRUE(reader->open("").isOk());
  StreamingClipSource source(reader, 1, 8 * page);
  ASSERT_EQ(source.prefill(0, 1), SessionGraphError::OK);
  const size_t pageIndex = StreamingClipSourceTestAccess::residentPageIndex(source, 0);
  ASSERT_LT(pageIndex, StreamingClipSource::kNumPages);

  std::array<float, 32> samples{};
  size_t framesRead = 0;
  ASSERT_TRUE(source.read(0, samples.data(), samples.size(), framesRead));
  ASSERT_EQ(framesRead, samples.size());
  ASSERT_TRUE(StreamingClipSourceTestAccess::acquireTransientReadPin(source, pageIndex, 0));
  EXPECT_EQ(StreamingClipSourceTestAccess::pageGuard(source, pageIndex), 1u);
  EXPECT_FALSE(StreamingClipSourceTestAccess::claimPage(source, pageIndex));

  StreamingClipSourceTestAccess::releaseReadyPin(source, pageIndex);
  EXPECT_EQ(StreamingClipSourceTestAccess::pageGuard(source, pageIndex), 0u);
  EXPECT_TRUE(StreamingClipSourceTestAccess::claimPage(source, pageIndex));
  StreamingClipSourceTestAccess::releaseClaim(source, pageIndex);
}

TEST(StreamingClipSourceDemandTest, ActiveVoiceLeaseProtectsAndReleasesCommandPageForReuse) {
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  auto reader = std::make_shared<FaultReader>(12 * page);
  ASSERT_TRUE(reader->open("").isOk());
  StreamingClipSource source(reader, 1, 12 * page);
  ASSERT_EQ(source.prefill(2 * page + 32), SessionGraphError::OK);

  StreamingClipSource::PrimeReservation activePrime{};
  ASSERT_EQ(source.primeForCommand(9 * page + 16, 32, activePrime), SessionGraphError::OK);
  size_t activeIndex = StreamingClipSource::kNumPages;
  for (size_t index = StreamingClipSource::kWindowPages; index < StreamingClipSource::kNumPages;
       ++index) {
    if ((activePrime.pageMask & static_cast<uint16_t>(uint16_t{1} << index)) != 0) {
      activeIndex = index;
    }
  }
  ASSERT_LT(activeIndex, StreamingClipSource::kNumPages);
  EXPECT_EQ(StreamingClipSourceTestAccess::pageGuard(source, activeIndex), 1u);

  std::array<int64_t, kVoiceDemandTestCapacity> positions{};
  positions[0] = 2 * page + 32;
  positions[1] = 9 * page + 16;
  StreamingClipSourceTestAccess::publishDemands(source, positions, 2);
  EXPECT_EQ(StreamingClipSourceTestAccess::pageGuard(source, activeIndex), 2u)
      << "active source window must own a command-pool page past first-render release";
  source.releaseCommandPrime(activePrime);
  EXPECT_EQ(StreamingClipSourceTestAccess::pageGuard(source, activeIndex), 1u);

  StreamingClipSource::PrimeReservation otherPrime{};
  ASSERT_EQ(source.primeForCommand(10 * page + 16, 32, otherPrime), SessionGraphError::OK);
  EXPECT_EQ(StreamingClipSourceTestAccess::pageGuard(source, activeIndex), 1u)
      << "an unrelated prime must use another command slot";
  source.releaseCommandPrime(otherPrime);

  positions.fill(0);
  positions[0] = 2 * page + 32;
  StreamingClipSourceTestAccess::publishDemands(source, positions, 1);
  EXPECT_EQ(StreamingClipSourceTestAccess::pageGuard(source, activeIndex), 0u)
      << "idle demand retirement must release the active-window lease";

  StreamingClipSource::PrimeReservation reused{};
  ASSERT_EQ(source.primeForCommand(8 * page + 16, 32, reused), SessionGraphError::OK);
  EXPECT_EQ(StreamingClipSourceTestAccess::pageStart(source, activeIndex), 8 * page);
  EXPECT_EQ(StreamingClipSourceTestAccess::pageGuard(source, activeIndex), 1u);
  source.releaseCommandPrime(reused);
  EXPECT_FALSE(source.hasPendingCommandPrimes());
}

TEST(MediaStreamWorkerServiceTest, SharedSourceRoundsServeEveryCurrentCursorBeforeLookahead) {
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  MediaStreamWorker worker;
  StreamingClipSourceTestAccess::parkWorker(worker);
  auto trace = std::make_shared<ServiceTrace>();
  auto reader = std::make_shared<FaultReader>(20 * page);
  ASSERT_TRUE(reader->open("").isOk());
  reader->setServiceTrace(trace, 0);
  auto source = std::make_shared<StreamingClipSource>(reader, 1, 20 * page);
  worker.attach(source);
  std::array<int64_t, kVoiceDemandTestCapacity> positions{};
  positions[0] = page + 16;
  positions[1] = 10 * page + 16;
  StreamingClipSourceTestAccess::publishDemands(*source, positions, 2);

  StreamingClipSourceTestAccess::WorkerDrive drive;
  for (size_t round = 0; round < 4; ++round) {
    StreamingClipSourceTestAccess::serviceRound(worker, drive);
    const auto observations = trace->observations();
    ASSERT_EQ(observations.size(), round + 1);
    const std::array<int64_t, 4> expectedStarts{page, 10 * page, 2 * page, 11 * page};
    EXPECT_EQ(observations.back().start, expectedStarts[round])
        << "current demand pages must precede every look-ahead page";
  }
  StreamingClipSourceTestAccess::rollbackDrive(drive);
}

TEST(MediaStreamWorkerServiceTest, CommandPagesAreFairAndPrecedeLookAhead) {
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  MediaStreamWorker worker;
  StreamingClipSourceTestAccess::parkWorker(worker);
  auto trace = std::make_shared<ServiceTrace>();
  std::array<std::shared_ptr<StreamingClipSource>, 3> sources;
  for (size_t index = 0; index < sources.size(); ++index) {
    auto reader = std::make_shared<FaultReader>(16 * page);
    ASSERT_TRUE(reader->open("").isOk());
    reader->setServiceTrace(trace, index);
    sources[index] = std::make_shared<StreamingClipSource>(reader, 1, 16 * page);
    worker.attach(sources[index]);
  }
  std::array<StreamingClipSource::PrimeReservation, 2> reservations{};
  std::array<SessionGraphError, 2> results{};
  std::array<std::thread, 2> controls;
  for (size_t index = 0; index < controls.size(); ++index) {
    controls[index] = std::thread([&, index]() {
      results[index] =
          sources[index + 1]->primeForCommand(8 * page, 2 * page + 1, reservations[index]);
    });
  }
  bool pending = true;
  for (size_t index = 1; index < sources.size(); ++index) {
    pending &=
        StreamingClipSourceTestAccess::waitForPending(*sources[index], std::chrono::seconds(1));
  }
  StreamingClipSourceTestAccess::WorkerDrive drive;
  if (pending) {
    for (size_t round = 0; round < 3; ++round) {
      StreamingClipSourceTestAccess::serviceRound(worker, drive);
      const auto observations = trace->observations();
      EXPECT_EQ(observations.size(), (round + 1) * sources.size());
      if (observations.size() == (round + 1) * sources.size()) {
        EXPECT_EQ(observations[3 * round].source, round == 2 ? 2u : 1u);
        EXPECT_EQ(observations[3 * round + 1].source, round == 2 ? 1u : 2u);
        EXPECT_EQ(observations[3 * round + 2].source, 0u);
        EXPECT_EQ(observations[3 * round].start, (8 + round) * page);
        EXPECT_EQ(observations[3 * round + 1].start, (8 + round) * page);
        EXPECT_EQ(observations[3 * round + 2].start, round * page);
      }
      if (round < 2) {
        for (size_t index = 1; index < sources.size(); ++index) {
          EXPECT_FALSE(StreamingClipSourceTestAccess::commandSucceeded(*sources[index]));
          EXPECT_EQ(StreamingClipSourceTestAccess::residentPageIndex(*sources[index], 8 * page),
                    StreamingClipSource::kNumPages)
              << "fresh command pages must remain unpublished between rounds";
        }
      }
    }
  }
  StreamingClipSourceTestAccess::rollbackDrive(drive);
  for (auto& control : controls) {
    control.join();
  }
  ASSERT_TRUE(pending);
  for (size_t index = 0; index < results.size(); ++index) {
    ASSERT_EQ(results[index], SessionGraphError::OK);
    sources[index + 1]->releaseCommandPrime(reservations[index]);
    EXPECT_FALSE(sources[index + 1]->hasPendingCommandPrimes());
    for (size_t slot = 0; slot < StreamingClipSource::kNumPages; ++slot) {
      EXPECT_EQ(StreamingClipSourceTestAccess::pageGuard(*sources[index + 1], slot), 0u);
    }
  }
}

TEST(MediaStreamWorkerServiceTest, PartialCommandFailureAndCancellationReleaseOnlyNewOwnership) {
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  for (const bool cancel : {false, true}) {
    SCOPED_TRACE(cancel);
    auto reader = std::make_shared<FaultReader>(12 * page);
    ASSERT_TRUE(reader->open("").isOk());
    auto source = std::make_shared<StreamingClipSource>(reader, 1, 12 * page);
    ASSERT_EQ(source->prefill(0, 1), SessionGraphError::OK);
    StreamingClipSource::LoopAnchorTransition anchor{};
    ASSERT_EQ(source->prepareLoopAnchorTransition(0, 1, true, anchor), SessionGraphError::OK);
    const size_t anchorIndex = static_cast<size_t>(std::countr_zero(anchor.replacementPageMask));
    source->commitLoopAnchorTransition(anchor);
    reader->resetReadCalls();
    auto trace = std::make_shared<ServiceTrace>();
    reader->setServiceTrace(trace, 0);
    MediaStreamWorker worker;
    StreamingClipSourceTestAccess::parkWorker(worker);
    worker.attach(source);
    StreamingClipSource::PrimeReservation reservation{};
    SessionGraphError result{};
    std::thread control(
        [&]() { result = source->primeForCommand(page - 32, 2 * page + 64, reservation); });
    const bool pending =
        StreamingClipSourceTestAccess::waitForPending(*source, std::chrono::seconds(1));
    StreamingClipSourceTestAccess::WorkerDrive drive;
    if (pending) {
      // Advance to the first missing-page decode, not an assumed worker round.
      for (size_t round = 0; round < 2 && reader->readCalls.load() == 0; ++round) {
        StreamingClipSourceTestAccess::serviceRound(worker, drive);
      }
      EXPECT_EQ(reader->readCalls.load(), 1u);
      EXPECT_EQ(StreamingClipSourceTestAccess::pageGuard(*source, anchorIndex), 2u);
      EXPECT_EQ(StreamingClipSourceTestAccess::residentPageIndex(*source, page),
                StreamingClipSource::kNumPages);
      if (cancel) {
        StreamingClipSourceTestAccess::requestCancellation(*source);
      } else {
        reader->failReadOnCall(reader->readCalls.load() + 1);
      }
      StreamingClipSourceTestAccess::serviceRound(worker, drive);
      EXPECT_EQ(reader->readCalls.load(), cancel ? 1u : 2u);
      const auto reads = trace->observations();
      EXPECT_EQ(reads.size(), cancel ? 1u : 2u);
      if (!reads.empty()) {
        EXPECT_EQ(reads[0].start, page);
      }
      if (!cancel && reads.size() == 2) {
        EXPECT_EQ(reads[1].start, 2 * page);
      }
    }
    StreamingClipSourceTestAccess::rollbackDrive(drive);
    control.join();
    ASSERT_TRUE(pending);
    EXPECT_EQ(result, SessionGraphError::NotReady);
    EXPECT_EQ(reservation.pageMask, 0u);
    EXPECT_FALSE(source->hasPendingCommandPrimes());
    EXPECT_EQ(StreamingClipSourceTestAccess::pageGuard(*source, anchorIndex), 1u);
    EXPECT_EQ(StreamingClipSourceTestAccess::pageStart(*source, anchorIndex), 0);
    for (size_t slot = 0; slot < StreamingClipSource::kNumPages; ++slot) {
      EXPECT_EQ(StreamingClipSourceTestAccess::pageGuard(*source, slot),
                slot == anchorIndex ? 1u : 0u);
    }
    // Direct-drive retry verifies every released command-pool slot is reusable.
    StreamingClipSourceTestAccess::setAttached(*source, false);
    reader->resetReadCalls();
    ASSERT_EQ(source->primeForCommand(page - 32, 2 * page + 64, reservation),
              SessionGraphError::OK);
    source->releaseCommandPrime(reservation);
    StreamingClipSource::LoopAnchorTransition disable{};
    ASSERT_EQ(source->prepareLoopAnchorTransition(0, 1, false, disable), SessionGraphError::OK);
    source->commitLoopAnchorTransition(disable);
    for (size_t slot = 0; slot < StreamingClipSource::kNumPages; ++slot) {
      EXPECT_EQ(StreamingClipSourceTestAccess::pageGuard(*source, slot), 0u);
    }
  }
}

TEST(StreamingClipSourcePrimeTest, FinalPartialPageIsValidButEarlyZeroAndShortReadsAreNot) {
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  for (const bool zero : {false, true}) {
    SCOPED_TRACE(zero);
    auto reader = std::make_shared<FaultReader>(page + 37);
    ASSERT_TRUE(reader->open("").isOk());
    StreamingClipSource source(reader, 1, page + 37);
    ASSERT_EQ(source.prefill(0, 1), SessionGraphError::OK);
    StreamingClipSource::PrimeReservation existing{};
    ASSERT_EQ(source.primeForCommand(0, 1, existing), SessionGraphError::OK);
    const size_t previous = StreamingClipSourceTestAccess::residentPageIndex(source, 0);
    ASSERT_LT(previous, StreamingClipSource::kNumPages);
    reader->resetReadCalls();
    reader->zeroReadCall.store(zero ? 1 : 0);
    reader->shortReadCall.store(zero ? 0 : 1);
    StreamingClipSource::PrimeReservation failed{};
    EXPECT_EQ(source.primeForCommand(page, 37, failed), SessionGraphError::InternalError);
    EXPECT_EQ(failed.pageMask, 0u);
    EXPECT_EQ(StreamingClipSourceTestAccess::residentPageIndex(source, page),
              StreamingClipSource::kNumPages);
    EXPECT_EQ(StreamingClipSourceTestAccess::pageGuard(source, previous), 1u);
    reader->resetReadCalls();
    ASSERT_EQ(source.primeForCommand(page - 8, 45, failed), SessionGraphError::OK);
    std::array<float, 64> samples{};
    samples.fill(-1.0f);
    size_t framesRead = 0;
    ASSERT_TRUE(source.read(page - 8, samples.data(), samples.size(), framesRead));
    EXPECT_EQ(framesRead, 45u);
    EXPECT_FLOAT_EQ(samples.front(), static_cast<float>(page - 8));
    EXPECT_FLOAT_EQ(samples[44], static_cast<float>(page + 36));
    EXPECT_FLOAT_EQ(samples[45], -1.0f);
    source.releaseCommandPrime(failed);
    source.releaseCommandPrime(existing);
    EXPECT_FALSE(source.hasPendingCommandPrimes());
    for (size_t slot = 0; slot < StreamingClipSource::kNumPages; ++slot) {
      EXPECT_EQ(StreamingClipSourceTestAccess::pageGuard(source, slot), 0u);
    }
  }
}

TEST(StreamingClipSourcePrimeTest, PrimeFailuresRollBackAndRecover) {
  auto reader =
      std::make_shared<FaultReader>(4 * static_cast<int64_t>(StreamingClipSource::kPageFrames));
  ASSERT_TRUE(reader->open("").isOk());
  StreamingClipSource source(reader, 1, 4 * static_cast<int64_t>(StreamingClipSource::kPageFrames));
  StreamingClipSource::PrimeReservation reservation{};
  reader->failRead.store(true, std::memory_order_relaxed);
  EXPECT_EQ(source.primeForCommand(2 * static_cast<int64_t>(StreamingClipSource::kPageFrames), 32,
                                   reservation),
            SessionGraphError::NotReady);
  EXPECT_EQ(reservation.pageMask, 0);
  EXPECT_FALSE(source.hasPendingCommandPrimes());
  reader->failRead.store(false, std::memory_order_relaxed);
  ASSERT_EQ(source.primeForCommand(2 * static_cast<int64_t>(StreamingClipSource::kPageFrames), 32,
                                   reservation),
            SessionGraphError::OK);
  EXPECT_NE(reservation.pageMask, 0);
  source.releaseCommandPrime(reservation);
  EXPECT_FALSE(source.hasPendingCommandPrimes());

  reservation = {};
  reader->failSeek.store(true, std::memory_order_relaxed);
  EXPECT_EQ(source.primeForCommand(3 * static_cast<int64_t>(StreamingClipSource::kPageFrames), 32,
                                   reservation),
            SessionGraphError::NotReady);
  EXPECT_EQ(reservation.pageMask, 0);
  reader->failSeek.store(false, std::memory_order_relaxed);
  ASSERT_EQ(source.primeForCommand(3 * static_cast<int64_t>(StreamingClipSource::kPageFrames), 32,
                                   reservation),
            SessionGraphError::OK);
  source.releaseCommandPrime(reservation);
  EXPECT_FALSE(source.hasPendingCommandPrimes());
}

TEST(StreamingClipSourcePrimeTest, CommandPrefillUsesPrimeCapacityAfterSteadyExhaustion) {
  // 12 pages: after the seek-eviction refill the widened 6-page steady window
  // must be FULL (pages 4..9) so the 3*page demand page can only be served
  // from command-prime capacity.
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  auto reader = std::make_shared<FaultReader>(12 * page);
  ASSERT_TRUE(reader->open("").isOk());
  StreamingClipSource source(reader, 1, 12 * page);
  StreamingClipSource::PrimeReservation reservation{};

  ASSERT_EQ(source.prefill(3 * page + 100), SessionGraphError::OK);
  std::vector<float> scratch(32, 0.0f);
  size_t framesRead = 0;
  ASSERT_TRUE(source.read(5 * page + 100, scratch.data(), 32, framesRead));
  source.service();

  EXPECT_EQ(source.prefill(3 * page + 100, 1), SessionGraphError::NotReady);
  ASSERT_EQ(source.prefill(3 * page + 100, 1, &reservation), SessionGraphError::OK);
  const uint16_t commandMask = static_cast<uint16_t>(0xFFFFu << StreamingClipSource::kWindowPages);
  EXPECT_NE(reservation.pageMask & commandMask, 0);
  EXPECT_TRUE(source.read(3 * page + 100, scratch.data(), 32, framesRead));
  EXPECT_EQ(framesRead, 32u);

  source.releaseCommandPrime(reservation);
  EXPECT_FALSE(source.hasPendingCommandPrimes());
}

TEST(StreamingClipSourcePrimeTest, EarlyEofDoesNotPublishSilence) {
  auto reader =
      std::make_shared<FaultReader>(2 * static_cast<int64_t>(StreamingClipSource::kPageFrames));
  ASSERT_TRUE(reader->open("").isOk());
  StreamingClipSource source(reader, 1, 2 * static_cast<int64_t>(StreamingClipSource::kPageFrames));
  StreamingClipSource::PrimeReservation reservation{};
  reader->earlyEof.store(true, std::memory_order_relaxed);
  EXPECT_EQ(source.primeForCommand(0, 32, reservation), SessionGraphError::InternalError);
  EXPECT_EQ(reservation.pageMask, 0);
  EXPECT_FALSE(source.hasPendingCommandPrimes());
  reader->earlyEof.store(false, std::memory_order_relaxed);
  ASSERT_EQ(source.primeForCommand(0, 32, reservation), SessionGraphError::OK);
  source.releaseCommandPrime(reservation);
}

TEST(StreamingClipSourcePrimeTest, PrimeCapacityRejectsAtomicallyAndRecovers) {
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  auto reader = std::make_shared<FaultReader>(8 * page);
  ASSERT_TRUE(reader->open("").isOk());
  StreamingClipSource source(reader, 1, 8 * page);
  StreamingClipSource::PrimeReservation first{};
  StreamingClipSource::PrimeReservation second{};
  StreamingClipSource::PrimeReservation rejected{};
  ASSERT_EQ(source.primeForCommand(page - 32, 64, first), SessionGraphError::OK);
  ASSERT_EQ(source.primeForCommand(3 * page - 32, 64, second), SessionGraphError::OK);
  EXPECT_TRUE(source.hasPendingCommandPrimes());
  EXPECT_EQ(source.primeForCommand(5 * page - 32, 64, rejected), SessionGraphError::NotReady);
  EXPECT_EQ(rejected.pageMask, 0);
  source.releaseCommandPrime(first);
  source.releaseCommandPrime(second);
  EXPECT_FALSE(source.hasPendingCommandPrimes());
  std::vector<float> scratch(64, 0.0f);
  size_t framesRead = 0;
  EXPECT_FALSE(source.read(5 * page, scratch.data(), 64, framesRead));
  EXPECT_EQ(framesRead, 0u);
  ASSERT_EQ(source.primeForCommand(5 * page - 32, 64, rejected), SessionGraphError::OK);
  source.releaseCommandPrime(rejected);
  EXPECT_FALSE(source.hasPendingCommandPrimes());
}

TEST(StreamingClipSourcePrimeTest, ConcurrentReadyPinReleaseAndAcquisitionPreventsReclamation) {
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  auto reader = std::make_shared<FaultReader>(8 * page);
  ASSERT_TRUE(reader->open("").isOk());
  StreamingClipSource source(reader, 1, 8 * page);
  ASSERT_EQ(source.prefill(0, 1), SessionGraphError::OK);
  StreamingClipSource::LoopAnchorTransition anchor{};
  ASSERT_EQ(source.prepareLoopAnchorTransition(0, 1, true, anchor), SessionGraphError::OK);
  const size_t pageIndex = static_cast<size_t>(std::countr_zero(anchor.replacementPageMask));
  ASSERT_LT(pageIndex, StreamingClipSource::kNumPages);
  source.commitLoopAnchorTransition(anchor);
  ASSERT_EQ(StreamingClipSourceTestAccess::pageGuard(source, pageIndex), 1u);

  constexpr size_t kOwners = 4;
  constexpr size_t kRounds = 512;
  std::array<bool, kOwners> ownsPin{};
  std::barrier begin(static_cast<std::ptrdiff_t>(kOwners + 2));
  std::barrier end(static_cast<std::ptrdiff_t>(kOwners + 2));
  std::array<std::thread, kOwners> owners;
  std::atomic<size_t> prematureClaims{0};
  RtGuardState::reset();
  for (size_t owner = 0; owner < kOwners; ++owner) {
    owners[owner] = std::thread([&, owner]() {
      for (size_t round = 0; round < kRounds; ++round) {
        begin.arrive_and_wait();
        if (ownsPin[owner]) {
          // Exercise the exact bounded helper, not releaseCommandPrime's
          // separate RMW. Each test owner acquired precisely one READY pin.
          RtSection section;
          StreamingClipSourceTestAccess::releaseReadyPin(source, pageIndex);
        }
        end.arrive_and_wait();
      }
    });
  }
  std::thread claimant([&]() {
    for (size_t round = 0; round < kRounds; ++round) {
      begin.arrive_and_wait();
      for (size_t attempt = 0; attempt < 32; ++attempt) {
        if (StreamingClipSourceTestAccess::claimPage(source, pageIndex)) {
          prematureClaims.fetch_add(1, std::memory_order_relaxed);
          StreamingClipSourceTestAccess::releaseClaim(source, pageIndex);
        }
      }
      end.arrive_and_wait();
    }
  });

  std::array<float, 32> scratch{};
  for (size_t round = 0; round < kRounds; ++round) {
    for (size_t owner = 0; owner < kOwners; ++owner) {
      ownsPin[owner] = StreamingClipSourceTestAccess::acquireReadyPin(source, pageIndex);
      EXPECT_TRUE(ownsPin[owner]);
    }
    begin.arrive_and_wait();
    // The real command-prime acquisition races the four valid-owner releases
    // while the anchor's independent pin prevents a zero-guard claim.
    StreamingClipSource::PrimeReservation reservation{};
    const auto result = source.primeForCommand(100, scratch.size(), reservation);
    EXPECT_EQ(result, SessionGraphError::OK);
    if (result == SessionGraphError::OK) {
      EXPECT_EQ(reservation.pageMask, static_cast<uint16_t>(uint16_t{1} << pageIndex));
      source.releaseCommandPrime(reservation);
    }
    end.arrive_and_wait();
    EXPECT_EQ(StreamingClipSourceTestAccess::pageGuard(source, pageIndex), 1u);
    EXPECT_EQ(StreamingClipSourceTestAccess::pageStart(source, pageIndex), 0);
    size_t framesRead = 0;
    bool readSucceeded;
    {
      RtSection section;
      readSucceeded = source.read(100, scratch.data(), scratch.size(), framesRead);
    }
    EXPECT_TRUE(readSucceeded);
    EXPECT_EQ(framesRead, scratch.size());
    EXPECT_FLOAT_EQ(scratch.front(), 100.0f);
    EXPECT_FLOAT_EQ(scratch.back(), 131.0f);
  }
  for (auto& owner : owners) {
    owner.join();
  }
  claimant.join();
  EXPECT_EQ(prematureClaims.load(std::memory_order_relaxed), 0u);
  EXPECT_EQ(reader->readCalls.load(std::memory_order_relaxed), 1u);
  EXPECT_FALSE(source.hasPendingCommandPrimes());
  EXPECT_EQ(RtGuardState::totalViolations(), 0u);

  StreamingClipSource::LoopAnchorTransition disabled{};
  ASSERT_EQ(source.prepareLoopAnchorTransition(0, 1, false, disabled), SessionGraphError::OK);
  EXPECT_FALSE(StreamingClipSourceTestAccess::claimPage(source, pageIndex));
  source.commitLoopAnchorTransition(disabled);
  ASSERT_TRUE(StreamingClipSourceTestAccess::claimPage(source, pageIndex));
  StreamingClipSource::PrimeReservation blocked{};
  EXPECT_EQ(source.primeForCommand(100, scratch.size(), blocked), SessionGraphError::NotReady);
  EXPECT_EQ(blocked.pageMask, 0);
  EXPECT_EQ(StreamingClipSourceTestAccess::pageStart(source, pageIndex), 0);
  StreamingClipSourceTestAccess::releaseClaim(source, pageIndex);
  ASSERT_EQ(source.primeForCommand(100, scratch.size(), blocked), SessionGraphError::OK);
  source.releaseCommandPrime(blocked);
  EXPECT_EQ(StreamingClipSourceTestAccess::pageGuard(source, pageIndex), 0u);
  EXPECT_FALSE(source.hasPendingCommandPrimes());
}

TEST(StreamingClipSourcePrimeTest,
     ConcurrentWorkerServiceAndCommandPrimeDoesNotDuplicateOwnership) {
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  auto reader = std::make_shared<FaultReader>(7 * page);
  ASSERT_TRUE(reader->open("").isOk());
  StreamingClipSource source(reader, 1, 7 * page);
  std::atomic<bool> stop{false};
  std::thread worker([&]() {
    while (!stop.load(std::memory_order_acquire)) {
      source.service();
    }
  });

  for (size_t index = 0; index < 32; ++index) {
    StreamingClipSource::PrimeReservation reservation{};
    ASSERT_EQ(source.primeForCommand(5 * page, 32, reservation), SessionGraphError::OK);
    ASSERT_NE(reservation.pageMask, 0);
    source.releaseCommandPrime(reservation);
  }
  stop.store(true, std::memory_order_release);
  worker.join();
  EXPECT_FALSE(source.hasPendingCommandPrimes());
}

TEST(StreamingClipSourcePrimeTest, AttachedPrimeDecodesOnWorkerThreadOnly) {
  // OCC191: while attached to a MediaStreamWorker the worker is the SOLE
  // decoder. primeForCommand must request + wait, never decode on the
  // command-priming (control) thread.
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  auto reader = std::make_shared<FaultReader>(7 * page);
  ASSERT_TRUE(reader->open("").isOk());
  StreamingClipSource source(reader, 1, 7 * page);
  // Keep the attached source alive for the worker's weak references; the
  // no-op deleter prevents freeing the stack-allocated source.
  std::shared_ptr<StreamingClipSource> keepAlive(&source, [](StreamingClipSource*) {});
  MediaStreamWorker worker;
  worker.attach(keepAlive);
  reader->setCallingThread(std::this_thread::get_id());

  StreamingClipSource::PrimeReservation reservation{};
  ASSERT_EQ(source.primeForCommand(2 * page, 32, reservation), SessionGraphError::OK);
  // Any decode/seek on this thread (including the timeout fallback) is a
  // regression: it would stall the worker that must keep the window ahead.
  EXPECT_FALSE(reader->readOnCallingThread.load());
  source.releaseCommandPrime(reservation);
  EXPECT_FALSE(source.hasPendingCommandPrimes());
}

TEST(StreamingClipSourcePrimeTest, AttachedPrimeFailureRollsBackWithoutLeak) {
  // OCC191 failure path: a worker decode failure must surface as NotReady with
  // no pins, no pending-prime accounting, and recover cleanly on retry.
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  auto reader = std::make_shared<FaultReader>(7 * page);
  ASSERT_TRUE(reader->open("").isOk());
  StreamingClipSource source(reader, 1, 7 * page);
  std::shared_ptr<StreamingClipSource> keepAlive(&source, [](StreamingClipSource*) {});
  // Fail decodes BEFORE attaching so the worker can never make the demanded
  // page resident from its steady window: the prime must take the
  // worker-decode path, which fails deterministically.
  reader->failRead.store(true, std::memory_order_relaxed);
  MediaStreamWorker worker;
  worker.attach(keepAlive);

  StreamingClipSource::PrimeReservation reservation{};
  EXPECT_EQ(source.primeForCommand(2 * page, 32, reservation), SessionGraphError::NotReady);
  EXPECT_EQ(reservation.pageMask, 0);
  EXPECT_FALSE(source.hasPendingCommandPrimes());

  reader->failRead.store(false, std::memory_order_relaxed);
  ASSERT_EQ(source.primeForCommand(2 * page, 32, reservation), SessionGraphError::OK);
  EXPECT_NE(reservation.pageMask, 0);
  source.releaseCommandPrime(reservation);
  EXPECT_FALSE(source.hasPendingCommandPrimes());
}

TEST(StreamingClipSourcePrimeTest, TimeoutCancellationAcknowledgesBeforeFallback) {
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  auto reader = std::make_shared<FaultReader>(12 * page);
  ASSERT_TRUE(reader->open("").isOk());
  StreamingClipSource source(reader, 1, 12 * page);
  StreamingClipSourceTestAccess::setAttached(source, true);
  reader->blockReadOnCall(1);

  StreamingClipSource::PrimeReservation reservation{};
  SessionGraphError primeResult = SessionGraphError::InternalError;
  std::atomic<bool> primeFinished{false};
  std::thread control([&]() {
    primeResult = source.primeForCommand(page + 32, static_cast<size_t>(page + 64), reservation);
    primeFinished.store(true, std::memory_order_release);
  });
  ASSERT_TRUE(StreamingClipSourceTestAccess::waitForPending(source, std::chrono::seconds(1)));

  std::thread worker([&]() { StreamingClipSourceTestAccess::serviceCommandDemand(source); });
  ASSERT_TRUE(reader->waitUntilReadBlocked(std::chrono::seconds(1)));
  std::this_thread::sleep_for(std::chrono::milliseconds(2100));
  EXPECT_FALSE(primeFinished.load(std::memory_order_acquire));

  reader->releaseBlockedRead();
  worker.join();
  control.join();
  EXPECT_EQ(primeResult, SessionGraphError::OK);
  ASSERT_NE(reservation.pageMask, 0);
  source.releaseCommandPrime(reservation);
  EXPECT_FALSE(source.hasPendingCommandPrimes());

  StreamingClipSourceTestAccess::setAttached(source, false);
  std::vector<float> scratch(32, 0.0f);
  size_t framesRead = 0;
  EXPECT_FALSE(source.read(10 * page, scratch.data(), 32, framesRead));

  std::array<StreamingClipSource::PrimeReservation, StreamingClipSource::kCommandPrimePages>
      reservations{};
  for (size_t index = 0; index < reservations.size(); ++index) {
    ASSERT_EQ(source.primeForCommand((6 + static_cast<int64_t>(index)) * page + 32, 32,
                                     reservations[index]),
              SessionGraphError::OK);
    ASSERT_NE(reservations[index].pageMask, 0);
    for (size_t previous = 0; previous < index; ++previous) {
      EXPECT_EQ(reservations[index].pageMask & reservations[previous].pageMask, 0);
    }
  }
  for (auto& held : reservations) {
    source.releaseCommandPrime(held);
  }
  EXPECT_FALSE(source.hasPendingCommandPrimes());
}

TEST(StreamingClipSourcePrimeTest, WorkerRollbackPreservesLateResidentPage) {
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  auto reader = std::make_shared<FaultReader>(8 * page);
  ASSERT_TRUE(reader->open("").isOk());
  StreamingClipSource source(reader, 1, 8 * page);
  reader->blockReadOnCall(1);
  SessionGraphError prefillResult = SessionGraphError::InternalError;
  std::thread prefill([&]() { prefillResult = source.prefill(page, 1); });
  ASSERT_TRUE(reader->waitUntilReadBlocked(std::chrono::seconds(1)));

  StreamingClipSourceTestAccess::setAttached(source, true);
  StreamingClipSource::PrimeReservation reservation{};
  SessionGraphError primeResult = SessionGraphError::InternalError;
  std::thread control([&]() {
    primeResult = source.primeForCommand(page + 32, static_cast<size_t>(page + 64), reservation);
  });
  ASSERT_TRUE(StreamingClipSourceTestAccess::waitForPending(source, std::chrono::seconds(1)));

  reader->releaseBlockedRead();
  prefill.join();
  ASSERT_EQ(prefillResult, SessionGraphError::OK);
  reader->failReadOnCall(reader->readCalls.load(std::memory_order_relaxed) + 1);
  StreamingClipSourceTestAccess::serviceCommandDemand(source);
  control.join();
  EXPECT_EQ(primeResult, SessionGraphError::NotReady);
  EXPECT_EQ(reservation.pageMask, 0);
  EXPECT_FALSE(source.hasPendingCommandPrimes());

  std::vector<float> scratch(32, 0.0f);
  size_t framesRead = 0;
  ASSERT_TRUE(source.read(page + 100, scratch.data(), 32, framesRead));
  EXPECT_EQ(framesRead, 32u);

  reader->failReadOnCall(0);
  reservation = {};
  primeResult = SessionGraphError::InternalError;
  std::thread retry([&]() {
    primeResult = source.primeForCommand(page + 32, static_cast<size_t>(page + 64), reservation);
  });
  ASSERT_TRUE(StreamingClipSourceTestAccess::waitForPending(source, std::chrono::seconds(1)));
  StreamingClipSourceTestAccess::serviceCommandDemand(source);
  retry.join();
  ASSERT_EQ(primeResult, SessionGraphError::OK);
  ASSERT_NE(reservation.pageMask, 0);
  source.releaseCommandPrime(reservation);
  EXPECT_FALSE(source.hasPendingCommandPrimes());
}

TEST(StreamingClipSourcePrimeTest, FailedReanchorRetainsExactOldPage) {
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  auto reader = std::make_shared<FaultReader>(16 * page);
  ASSERT_TRUE(reader->open("").isOk());
  StreamingClipSource source(reader, 1, 16 * page);
  ASSERT_EQ(source.prefill(0), SessionGraphError::OK);

  StreamingClipSource::LoopAnchorTransition anchor{};
  ASSERT_EQ(source.prepareLoopAnchorTransition(0, 1, true, anchor), SessionGraphError::OK);
  const size_t oldIndex = static_cast<size_t>(std::countr_zero(anchor.replacementPageMask));
  ASSERT_LT(oldIndex, StreamingClipSource::kNumPages);
  source.commitLoopAnchorTransition(anchor);
  const uint32_t oldGuard = StreamingClipSourceTestAccess::pageGuard(source, oldIndex);
  ASSERT_EQ(oldGuard, 1u);

  std::array<StreamingClipSource::PrimeReservation, StreamingClipSource::kCommandPrimePages> held{};
  for (size_t index = 0; index < held.size(); ++index) {
    ASSERT_EQ(source.primeForCommand((6 + static_cast<int64_t>(index)) * page, 32, held[index]),
              SessionGraphError::OK);
  }

  StreamingClipSource::LoopAnchorTransition failed{};
  EXPECT_EQ(source.prepareLoopAnchorTransition(10 * page, 1, true, failed),
            SessionGraphError::NotReady);
  EXPECT_EQ(failed.previousPageMask, 0u);
  EXPECT_EQ(failed.replacementPageMask, 0u);
  EXPECT_EQ(StreamingClipSourceTestAccess::pageStart(source, oldIndex), 0);
  EXPECT_EQ(StreamingClipSourceTestAccess::pageGuard(source, oldIndex), oldGuard);
  EXPECT_FALSE(StreamingClipSourceTestAccess::claimPage(source, oldIndex));
  source.setDemand(10 * page);
  source.service();

  std::vector<float> scratch(32, 0.0f);
  size_t framesRead = 0;
  ASSERT_TRUE(source.read(100, scratch.data(), 32, framesRead));
  EXPECT_EQ(framesRead, 32u);

  for (auto& reservation : held) {
    source.releaseCommandPrime(reservation);
  }
  StreamingClipSource::LoopAnchorTransition released{};
  ASSERT_EQ(source.prepareLoopAnchorTransition(0, 1, false, released), SessionGraphError::OK);
  source.commitLoopAnchorTransition(released);

  StreamingClipSource::PrimeReservation recovered{};
  ASSERT_EQ(source.primeForCommand(10 * page, 32, recovered), SessionGraphError::OK);
  source.releaseCommandPrime(recovered);
  EXPECT_FALSE(source.hasPendingCommandPrimes());
}

TEST(StreamingClipSourcePrimeTest, RepeatedFallbackAnchorDoesNotLeakPins) {
  // A window anchor that falls back to one page because the command pool is full must not
  // pin that page again each time the same anchor is prepared, whether it is committed or
  // rolled back.
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  auto reader = std::make_shared<FaultReader>(16 * page);
  ASSERT_TRUE(reader->open("").isOk());
  StreamingClipSource source(reader, 1, 16 * page);
  ASSERT_EQ(source.prefill(0), SessionGraphError::OK);

  // Three single-page reservations leave exactly one command-pool slot.
  std::array<StreamingClipSource::PrimeReservation, 3> held{};
  for (size_t index = 0; index < held.size(); ++index) {
    ASSERT_EQ(source.primeForCommand((11 + static_cast<int64_t>(index)) * page, 32, held[index]),
              SessionGraphError::OK);
  }

  // The window covers pages 8 and 9 and needs two slots, so the anchor keeps page 8 only.
  const int64_t pos = 9 * page - 100;
  StreamingClipSource::LoopAnchorTransition first{};
  ASSERT_EQ(source.prepareLoopAnchorTransition(pos, 512, true, first), SessionGraphError::OK);
  ASSERT_EQ(std::popcount(first.replacementPageMask), 1);
  const size_t index = static_cast<size_t>(std::countr_zero(first.replacementPageMask));
  ASSERT_LT(index, StreamingClipSource::kNumPages);
  ASSERT_EQ(StreamingClipSourceTestAccess::pageStart(source, index), 8 * page);
  source.commitLoopAnchorTransition(first);
  ASSERT_EQ(StreamingClipSourceTestAccess::pageGuard(source, index), 1u);

  StreamingClipSource::LoopAnchorTransition repeated{};
  ASSERT_EQ(source.prepareLoopAnchorTransition(pos, 512, true, repeated), SessionGraphError::OK);
  source.commitLoopAnchorTransition(repeated);
  EXPECT_EQ(StreamingClipSourceTestAccess::pageGuard(source, index), 1u) << "after commit";

  StreamingClipSource::LoopAnchorTransition rolledBack{};
  ASSERT_EQ(source.prepareLoopAnchorTransition(pos, 512, true, rolledBack), SessionGraphError::OK);
  source.rollbackLoopAnchorTransition(rolledBack);
  EXPECT_EQ(StreamingClipSourceTestAccess::pageGuard(source, index), 1u) << "after rollback";

  StreamingClipSource::LoopAnchorTransition released{};
  ASSERT_EQ(source.prepareLoopAnchorTransition(0, 1, false, released), SessionGraphError::OK);
  source.commitLoopAnchorTransition(released);
  EXPECT_EQ(StreamingClipSourceTestAccess::pageGuard(source, index), 0u) << "after release";

  for (auto& reservation : held) {
    source.releaseCommandPrime(reservation);
  }
  EXPECT_FALSE(source.hasPendingCommandPrimes());
}

TEST(StreamingClipSourcePrimeTest, LoopAnchorReleasePreservesCommandPin) {
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  auto reader = std::make_shared<FaultReader>(10 * page);
  ASSERT_TRUE(reader->open("").isOk());
  StreamingClipSource source(reader, 1, 10 * page);

  StreamingClipSource::PrimeReservation commandReservation{};
  ASSERT_EQ(source.primeForCommand(6 * page, 32, commandReservation), SessionGraphError::OK);
  ASSERT_NE(commandReservation.pageMask, 0);

  StreamingClipSource::LoopAnchorTransition anchor{};
  ASSERT_EQ(source.prepareLoopAnchorTransition(6 * page, 1, true, anchor), SessionGraphError::OK);
  source.commitLoopAnchorTransition(anchor);
  StreamingClipSource::LoopAnchorTransition released{};
  ASSERT_EQ(source.prepareLoopAnchorTransition(0, 1, false, released), SessionGraphError::OK);
  source.commitLoopAnchorTransition(released);

  std::vector<float> scratch(32, 0.0f);
  size_t framesRead = 0;
  ASSERT_TRUE(source.read(6 * page + 100, scratch.data(), 32, framesRead));
  EXPECT_EQ(framesRead, 32u);

  source.releaseCommandPrime(commandReservation);
  EXPECT_FALSE(source.hasPendingCommandPrimes());
}

TEST(StreamingClipSourcePrimeTest, PrefillWidenedWindowCoversFourPagesAhead) {
  // OCC191: the resident window grew from 1 behind + demand + 2 ahead to
  // 1 behind + demand + 4 ahead, so a demand at page 0 must serve page 4.
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  auto reader = std::make_shared<FaultReader>(9 * page);
  ASSERT_TRUE(reader->open("").isOk());
  StreamingClipSource source(reader, 1, 9 * page);

  ASSERT_EQ(source.prefill(0), SessionGraphError::OK);
  std::vector<float> scratch(32, 0.0f);
  size_t framesRead = 0;
  ASSERT_TRUE(source.read(4 * page + 100, scratch.data(), 32, framesRead));
  EXPECT_EQ(framesRead, 32u);
}

TEST_F(StreamingSeekMatrixTest, LoopViaMetadataPinsAnchorForLoopRestart) {
  // OCC191: the app toggles loop through updateClipMetadata (not
  // setClipLoopMode). The loop-restart page (trim-IN) must be pinned while
  // loop is enabled so the audio-thread loop boundary — which cannot prime —
  // never misses the first read of the loop restart.
  constexpr uint32_t rate = 48000;
  constexpr uint32_t block = 512;
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  const int64_t trimIn = 2 * page;         // page 2
  const int64_t trimOut = 3 * page + 4000; // inside page 3
  const int64_t evictionTarget = 6 * page; // far from trim-IN
  const int64_t fileLength = 9 * static_cast<int64_t>(rate);

  TransportConfig config{.sampleRate = rate,
                         .outputChannels = 1,
                         .maxBlockFrames = block,
                         .maxActiveVoices = 32,
                         .numGroups = 1,
                         .maxSourceChannels = 1,
                         .sourceChannelPolicy = SourceChannelPolicy::Discrete};
  auto transport = std::make_unique<TransportController>(nullptr, config);
  transport->setPreparedSourceMaxFrames(rate);
  SeekCallback callback;
  transport->setCallback(&callback);
  ASSERT_EQ(transport->registerClipAudio(1, sourcePath(rate, 1, rate)), SessionGraphError::OK);
  ASSERT_EQ(transport->updateClipTrimPoints(1, trimIn, fileLength), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);

  std::vector<float> output(block);
  float* buffers[] = {output.data()};
  transport->processAudio(buffers, 1, block);

  // Evict the trim-IN page: render far away (still inside the file) so read()
  // retires page 2 without natural-ending the voice.
  ASSERT_EQ(transport->seekClip(1, evictionTarget), SessionGraphError::OK);
  transport->processAudio(buffers, 1, block);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  transport->processAudio(buffers, 1, block);

  // Enable loop AND narrow the trim window through updateClipMetadata (the
  // app's actual path for both).
  auto metadata = transport->getClipMetadata(1);
  ASSERT_TRUE(metadata.has_value());
  metadata->trimInSamples = trimIn;
  metadata->trimOutSamples = trimOut;
  metadata->loopEnabled = true;
  ASSERT_EQ(transport->updateClipMetadata(1, *metadata), SessionGraphError::OK);

  // Seek near trim-OUT; the next render crosses the loop boundary and
  // restarts at trim-IN. The loop-restart read must not miss.
  ASSERT_EQ(transport->seekClip(1, trimOut - 2000), SessionGraphError::OK);
  RtGuardState::reset();
  for (int i = 0; i < 12; ++i) {
    {
      RtSection section;
      transport->processAudio(buffers, 1, block);
    }
    transport->processCallbacks();
  }

  EXPECT_EQ(callback.underruns.load(), 0)
      << "loop-restart read at trim-IN missed with loop anchor pinned";
  EXPECT_EQ(RtGuardState::totalViolations(), 0u);
  transport->setCallback(nullptr);
}

TEST_F(StreamingSeekMatrixTest, LoopNudgeChurnKeepsRestartPageResident) {
  // OCC191 app repro: trim-IN nudges (< / >) move trim-IN while loop is
  // enabled. The loop-restart page must follow every nudge so the audio-thread
  // loop boundary never reads a non-resident page at the new trim-IN.
  constexpr uint32_t rate = 48000;
  constexpr uint32_t block = 512;
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  const int64_t baseIn = 2 * page;         // page 2 (small, like app's page-0 nudges)
  const int64_t trimOut = 3 * page + 4000; // inside page 3
  const int64_t evictionTarget = 6 * page;
  const int64_t fileLength = 9 * static_cast<int64_t>(rate);

  TransportConfig config{.sampleRate = rate,
                         .outputChannels = 1,
                         .maxBlockFrames = block,
                         .maxActiveVoices = 32,
                         .numGroups = 1,
                         .maxSourceChannels = 1,
                         .sourceChannelPolicy = SourceChannelPolicy::Discrete};
  auto transport = std::make_unique<TransportController>(nullptr, config);
  transport->setPreparedSourceMaxFrames(rate);
  SeekCallback callback;
  transport->setCallback(&callback);
  ASSERT_EQ(transport->registerClipAudio(1, sourcePath(rate, 1, rate)), SessionGraphError::OK);
  ASSERT_EQ(transport->updateClipTrimPoints(1, baseIn, fileLength), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);

  std::vector<float> output(block);
  float* buffers[] = {output.data()};
  transport->processAudio(buffers, 1, block);

  // Evict the trim-IN page by rendering far away.
  ASSERT_EQ(transport->seekClip(1, evictionTarget), SessionGraphError::OK);
  transport->processAudio(buffers, 1, block);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  transport->processAudio(buffers, 1, block);

  // Enable loop AND narrow the trim window via updateClipMetadata.
  auto metadata = transport->getClipMetadata(1);
  ASSERT_TRUE(metadata.has_value());
  metadata->trimInSamples = baseIn;
  metadata->trimOutSamples = trimOut;
  metadata->loopEnabled = true;
  ASSERT_EQ(transport->updateClipMetadata(1, *metadata), SessionGraphError::OK);

  // Nudge trim-IN left/right a few ticks while loop is enabled — each nudge
  // must re-anchor the loop-restart page (app's < / > keys).
  int64_t trimIn = baseIn;
  for (int tick = 0; tick < 4; ++tick) {
    trimIn -= 640; // nudge left one tick
    auto m = transport->getClipMetadata(1);
    ASSERT_TRUE(m.has_value());
    m->trimInSamples = trimIn;
    m->loopEnabled = true;
    ASSERT_EQ(transport->updateClipMetadata(1, *m), SessionGraphError::OK);
    transport->processAudio(buffers, 1, block);
  }
  for (int tick = 0; tick < 4; ++tick) {
    trimIn += 640; // nudge right
    auto m = transport->getClipMetadata(1);
    ASSERT_TRUE(m.has_value());
    m->trimInSamples = trimIn;
    m->loopEnabled = true;
    ASSERT_EQ(transport->updateClipMetadata(1, *m), SessionGraphError::OK);
    transport->processAudio(buffers, 1, block);
  }

  // Seek near trim-OUT and render across the loop boundary repeatedly.
  ASSERT_EQ(transport->seekClip(1, trimOut - 2000), SessionGraphError::OK);
  RtGuardState::reset();
  for (int i = 0; i < 16; ++i) {
    {
      RtSection section;
      transport->processAudio(buffers, 1, block);
    }
    transport->processCallbacks();
  }

  EXPECT_EQ(callback.underruns.load(), 0)
      << "loop restart missed after trim-IN nudges with loop enabled";
  EXPECT_EQ(RtGuardState::totalViolations(), 0u);
  transport->setCallback(nullptr);
}

TEST_F(StreamingSeekMatrixTest, LoopWrapNearPageEndKeepsSuccessorPageResident) {
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  for (const uint32_t block : {64u, 512u, 2048u}) {
    for (const double rate : {0.5, 1.0, 1.04, 2.0}) {
      const int64_t step = static_cast<int64_t>(std::ceil(static_cast<double>(block) * rate));
      for (const int64_t k : {int64_t{1}, static_cast<int64_t>(block / 2), step + 2}) {
        SCOPED_TRACE("block=" + std::to_string(block) + " rate=" + std::to_string(rate) +
                     " k=" + std::to_string(k));
        const int64_t trimIn = 2 * page - k;
        const int64_t trimOut = 5 * page + 60000;
        auto transport = makeStreamingTransport(block);
        SeekCallback callback;
        ASSERT_NO_FATAL_FAILURE(configureStreamingClip(*transport, callback,
                                                       sourcePath(48000, 1, 48000), trimIn, trimOut,
                                                       rate, true, VoiceMode::MonoWithFadeOverlap));
        ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);
        std::vector<float> out;
        renderBlocks(*transport, out, block, 2);

        const int64_t firstRender = static_cast<int64_t>(block) * 4 + 2;
        const int64_t lead = firstRender + 4 * step + 2;
        const int64_t parked = trimOut - lead;
        const auto blocks = static_cast<size_t>(
            std::ceil(static_cast<double>(lead) / (static_cast<double>(block) * rate)) + 2);
        for (int wrap = 0; wrap < 10; ++wrap) {
          ASSERT_EQ(transport->seekClip(1, parked), SessionGraphError::OK);
          renderBlocks(*transport, out, block, blocks);
        }
        EXPECT_EQ(callback.loops.load(), 10);
        EXPECT_EQ(callback.underruns.load(), 0);
        EXPECT_TRUE(callback.classified.empty());
      }
    }
  }
}

TEST_F(StreamingSeekMatrixTest, LoopWrapWithTrimOutOnPageBoundaryHasNoMiss) {
  // The last frame a block interpolates is the one at trim-out, so a trim-out on or next to a
  // page boundary needs the page after the window resident as well.
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  constexpr uint32_t block = 512;
  for (const int64_t trimOut : {5 * page - 1, 5 * page, 5 * page + 1}) {
    for (const double rate : {1.0, 1.04}) {
      SCOPED_TRACE("trimOut=" + std::to_string(trimOut) + " rate=" + std::to_string(rate));
      const int64_t trimIn = 2 * page - 256;
      auto transport = makeStreamingTransport(block);
      SeekCallback callback;
      ASSERT_NO_FATAL_FAILURE(configureStreamingClip(*transport, callback,
                                                     sourcePath(48000, 1, 48000), trimIn, trimOut,
                                                     rate, true, VoiceMode::MonoWithFadeOverlap));
      ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);
      std::vector<float> out;
      renderBlocks(*transport, out, block, 2);
      auto source = TransportControllerTestAccess::source(*transport, 1);
      ASSERT_TRUE(source);

      const int64_t step = static_cast<int64_t>(std::ceil(static_cast<double>(block) * rate));
      const int64_t lead = static_cast<int64_t>(block) * 4 + 2 + 4 * step + 2;
      const int64_t parked = trimOut - lead;
      const auto blocks = static_cast<size_t>(
          std::ceil(static_cast<double>(lead) / (static_cast<double>(block) * rate)) + 2);
      // The seek primes only the first render window. The page holding trim-out comes from the
      // worker's program-aware look-ahead, so give it the time real playback has (about a
      // hundred milliseconds) instead of racing it with back-to-back unpaced blocks.
      const int64_t windowEndPage = trimOut / page * page;
      for (int wrap = 0; wrap < 10; ++wrap) {
        ASSERT_EQ(transport->seekClip(1, parked), SessionGraphError::OK);
        renderBlocks(*transport, out, block, 1);
        ASSERT_TRUE(waitForResident(*source, windowEndPage))
            << "the page holding trim-out was not prefetched, wrap " << wrap;
        renderBlocks(*transport, out, block, blocks - 1);
      }
      EXPECT_EQ(callback.loops.load(), 10);
      EXPECT_EQ(callback.underruns.load(), 0);
      EXPECT_TRUE(callback.classified.empty());
    }
  }
}

TEST_F(StreamingSeekMatrixTest, RetriggerFarFromTrimInPrimesFirstRenderWindow) {
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  const int64_t far = 5 * page + 100;
  enum class Trigger { StartOverlap, StartStrict, RestartOverlap };
  for (const uint32_t block : {512u, 2048u}) {
    for (const double rate : {1.0, 1.04, 2.0}) {
      for (const int64_t k : {int64_t{1}, int64_t{256}}) {
        for (const bool loop : {false, true}) {
          for (const Trigger trigger :
               {Trigger::StartOverlap, Trigger::StartStrict, Trigger::RestartOverlap}) {
            SCOPED_TRACE("block=" + std::to_string(block) + " rate=" + std::to_string(rate) +
                         " k=" + std::to_string(k) + " loop=" + std::to_string(loop) +
                         " trigger=" + std::to_string(static_cast<int>(trigger)));
            const VoiceMode mode = trigger == Trigger::StartStrict ? VoiceMode::MonoStrict
                                                                   : VoiceMode::MonoWithFadeOverlap;
            auto transport = makeStreamingTransport(block);
            SeekCallback callback;
            ASSERT_NO_FATAL_FAILURE(
                configureStreamingClip(*transport, callback, sourcePath(48000, 1, 48000),
                                       2 * page - k, 5 * page + 60000, rate, loop, mode));
            ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);
            std::vector<float> out;
            renderBlocks(*transport, out, block, 2);
            for (int round = 0; round < 5; ++round) {
              ASSERT_EQ(transport->seekClip(1, far), SessionGraphError::OK);
              renderBlocks(*transport, out, block, 4);
              ASSERT_EQ(trigger == Trigger::RestartOverlap ? transport->restartClip(1)
                                                           : transport->startClip(1),
                        SessionGraphError::OK);
              renderBlocks(*transport, out, block, 2);
            }
            EXPECT_EQ(callback.underruns.load(), 0);
            EXPECT_TRUE(callback.classified.empty());
            EXPECT_EQ(transport->getActiveVoiceCount(1), 1u);
          }
        }
      }
    }
  }
}

TEST_F(StreamingSeekMatrixTest, ColdStartShortOfPageBoundaryNeedsNoHostPrefill) {
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  constexpr uint32_t block = 512;
  int total = 0;
  for (int trial = 0; trial < 40; ++trial) {
    auto transport = makeStreamingTransport(block);
    SeekCallback callback;
    ASSERT_NO_FATAL_FAILURE(
        configureStreamingClip(*transport, callback, sourcePath(48000, 1, 48000), 2 * page - 256,
                               5 * page, 1.04, true, VoiceMode::MonoWithFadeOverlap));
    ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);
    std::vector<float> out;
    renderBlocks(*transport, out, block, 4);
    total += callback.underruns.load();
    EXPECT_TRUE(callback.classified.empty());
  }
  EXPECT_EQ(total, 0);
}

TEST_F(StreamingSeekMatrixTest, PrepareClipAudioAfterPlayDoesNotWaitOutFillTimeout) {
  // A stopped clip keeps the steady pages its last voice left behind. Only the audio thread
  // evicts, and it no longer reads, so those pages must not starve the restart window: the
  // control-thread prefill must complete in worker time, not at kFillWaitTimeout.
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  constexpr uint32_t block = 512;
  const auto budget = std::chrono::milliseconds(500);
  auto transport = makeStreamingTransport(block);
  SeekCallback callback;
  ASSERT_NO_FATAL_FAILURE(configureStreamingClip(*transport, callback, sourcePath(48000, 1, 48000),
                                                 page - 256, 6 * page, 1.0, false,
                                                 VoiceMode::MonoWithFadeOverlap));
  std::vector<float> out;
  for (const size_t playBlocks : {size_t{20}, size_t{100}, size_t{150}, size_t{270}, size_t{400}}) {
    SCOPED_TRACE("playBlocks=" + std::to_string(playBlocks));
    ASSERT_EQ(transport->prepareClipAudio(1), SessionGraphError::OK);
    ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);
    renderBlocks(*transport, out, block, playBlocks);
    ASSERT_EQ(transport->stopClip(1), SessionGraphError::OK);
    for (int index = 0; index < 64 && transport->getActiveVoiceCount(1) != 0; ++index) {
      renderBlocks(*transport, out, block, 1);
    }
    ASSERT_EQ(transport->getActiveVoiceCount(1), 0u);
    renderBlocks(*transport, out, block, 20);

    const auto begin = std::chrono::steady_clock::now();
    const SessionGraphError result = transport->prepareClipAudio(1);
    const auto elapsed = std::chrono::steady_clock::now() - begin;
    EXPECT_EQ(result, SessionGraphError::OK);
    EXPECT_LT(elapsed, budget)
        << "prepareClipAudio blocked "
        << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()
        << " ms on a stopped clip";
  }
  EXPECT_EQ(callback.underruns.load(), 0);
  transport->setCallback(nullptr);
}

TEST_F(StreamingSeekMatrixTest, PrepareClipAudioOnLiveClipPinsRestartWindowUntilNextStart) {
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  const int64_t far = 5 * page + 100;
  constexpr uint32_t block = 512;
  auto transport = makeStreamingTransport(block);
  SeekCallback callback;
  ASSERT_NO_FATAL_FAILURE(configureStreamingClip(*transport, callback, sourcePath(48000, 1, 48000),
                                                 2 * page - 256, 5 * page + 60000, 1.0, false,
                                                 VoiceMode::MonoWithFadeOverlap));
  ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);
  std::vector<float> out;
  renderBlocks(*transport, out, block, 2);
  ASSERT_EQ(transport->seekClip(1, far), SessionGraphError::OK);
  renderBlocks(*transport, out, block, 4);
  ASSERT_EQ(transport->prepareClipAudio(1), SessionGraphError::OK);
  renderBlocks(*transport, out, block, 4);
  auto source = TransportControllerTestAccess::source(*transport, 1);
  ASSERT_TRUE(source);
  for (const int64_t start : {page, 2 * page}) {
    const size_t index = StreamingClipSourceTestAccess::residentPageIndex(*source, start);
    ASSERT_LT(index, StreamingClipSource::kNumPages) << "restart page not resident: " << start;
    EXPECT_GE(StreamingClipSourceTestAccess::pageGuard(*source, index), 1u);
  }
  ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);
  renderBlocks(*transport, out, block, 2);
  EXPECT_EQ(callback.underruns.load(), 0);
  EXPECT_TRUE(callback.classified.empty());
  ASSERT_EQ(transport->seekClip(1, far), SessionGraphError::OK);
  renderBlocks(*transport, out, block, 4);
  for (const int64_t start : {page, 2 * page}) {
    EXPECT_TRUE(waitForEvicted(*transport, *source, out, block, start))
        << "pin on page " << start << " was not released by the non-looping start";
  }
}

TEST_F(StreamingSeekMatrixTest, PrepareClipAudioOnLiveClipPinsAreReleasedByUpdatesAndUnregister) {
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  const int64_t far = 5 * page + 100;
  constexpr uint32_t block = 512;
  auto transport = makeStreamingTransport(block);
  SeekCallback callback;
  ASSERT_NO_FATAL_FAILURE(configureStreamingClip(*transport, callback, sourcePath(48000, 1, 48000),
                                                 2 * page - 256, 5 * page + 60000, 1.0, false,
                                                 VoiceMode::MonoWithFadeOverlap));
  ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);
  std::vector<float> out;
  renderBlocks(*transport, out, block, 2);
  ASSERT_EQ(transport->seekClip(1, far), SessionGraphError::OK);
  renderBlocks(*transport, out, block, 4);
  ASSERT_EQ(transport->prepareClipAudio(1), SessionGraphError::OK);
  auto source = TransportControllerTestAccess::source(*transport, 1);
  ASSERT_TRUE(source);

  // Phase 1: a loop-mode update releases the live-prepare pins.
  ASSERT_EQ(transport->setClipLoopMode(1, false), SessionGraphError::OK);
  renderBlocks(*transport, out, block, 1);
  ASSERT_EQ(transport->seekClip(1, far), SessionGraphError::OK);
  renderBlocks(*transport, out, block, 4);
  for (const int64_t start : {page, 2 * page}) {
    EXPECT_TRUE(waitForEvicted(*transport, *source, out, block, start))
        << "pin on page " << start << " survived the loop-mode update";
  }

  // Phase 2: unregister succeeds after a live prepare, once the voice has stopped.
  ASSERT_EQ(transport->seekClip(1, far), SessionGraphError::OK);
  renderBlocks(*transport, out, block, 4);
  ASSERT_EQ(transport->prepareClipAudio(1), SessionGraphError::OK);
  ASSERT_EQ(transport->stopClip(1), SessionGraphError::OK);
  for (int index = 0; index < 64 && transport->getActiveVoiceCount(1) != 0; ++index) {
    renderBlocks(*transport, out, block, 1);
  }
  ASSERT_EQ(transport->getActiveVoiceCount(1), 0u);
  renderBlocks(*transport, out, block, 1);
  source.reset();
  ASSERT_EQ(transport->unregisterClipAudio(1), SessionGraphError::OK);
}

TEST_F(StreamingSeekMatrixTest, PrepareClipAudioOnLiveLoopKeepsExistingAnchor) {
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  const int64_t far = 5 * page + 100;
  constexpr uint32_t block = 512;
  auto transport = makeStreamingTransport(block);
  SeekCallback callback;
  ASSERT_NO_FATAL_FAILURE(configureStreamingClip(*transport, callback, sourcePath(48000, 1, 48000),
                                                 2 * page - 256, 5 * page + 60000, 1.0, true,
                                                 VoiceMode::MonoWithFadeOverlap));
  ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);
  std::vector<float> out;
  renderBlocks(*transport, out, block, 2);
  ASSERT_EQ(transport->seekClip(1, far), SessionGraphError::OK);
  renderBlocks(*transport, out, block, 4);
  auto source = TransportControllerTestAccess::source(*transport, 1);
  ASSERT_TRUE(source);
  std::vector<std::pair<int64_t, uint32_t>> recorded;
  for (const int64_t start : {page, 2 * page}) {
    const size_t index = StreamingClipSourceTestAccess::residentPageIndex(*source, start);
    if (index < StreamingClipSource::kNumPages) {
      recorded.emplace_back(start, StreamingClipSourceTestAccess::pageGuard(*source, index));
    }
  }
  ASSERT_EQ(recorded.size(), 2u) << "both pages of the first-render window must be resident";
  for (const auto& entry : recorded) {
    ASSERT_GE(entry.second, 1u) << "page " << entry.first << " must be pinned by the anchor";
  }
  ASSERT_EQ(transport->prepareClipAudio(1), SessionGraphError::OK);
  for (const auto& [start, guard] : recorded) {
    const size_t index = StreamingClipSourceTestAccess::residentPageIndex(*source, start);
    ASSERT_LT(index, StreamingClipSource::kNumPages);
    EXPECT_EQ(StreamingClipSourceTestAccess::pageGuard(*source, index), guard) << "page " << start;
  }
}

TEST_F(StreamingSeekMatrixTest, UnconsumedLiveRestartReleasesItsPrimeOnTeardown) {
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  constexpr uint32_t block = 512;
  auto transport = makeStreamingTransport(block);
  SeekCallback callback;
  ASSERT_NO_FATAL_FAILURE(configureStreamingClip(*transport, callback, sourcePath(48000, 1, 48000),
                                                 2 * page - 256, 5 * page + 60000, 1.0, false,
                                                 VoiceMode::MonoWithFadeOverlap));
  ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);
  std::vector<float> out;
  renderBlocks(*transport, out, block, 2);
  ASSERT_EQ(transport->seekClip(1, 5 * page + 100), SessionGraphError::OK);
  renderBlocks(*transport, out, block, 4);
  auto source = TransportControllerTestAccess::source(*transport, 1);
  ASSERT_TRUE(source);

  // The restart pins the first-render window on the control thread; it is never consumed.
  ASSERT_EQ(transport->restartClip(1), SessionGraphError::OK);
  ASSERT_TRUE(source->hasPendingCommandPrimes());
  transport.reset();
  EXPECT_FALSE(source->hasPendingCommandPrimes());
}

TEST_F(StreamingSeekMatrixTest, SegmentAdvanceToDistantPageIsPrefetchedBeforeTheJump) {
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  constexpr uint32_t block = 512;
  for (const double rate : {1.0, 1.04}) {
    for (const int64_t k : {int64_t{1}, int64_t{256}}) {
      SCOPED_TRACE("rate=" + std::to_string(rate) + " k=" + std::to_string(k));
      auto transport = makeStreamingTransport(block);
      SeekCallback callback;
      ASSERT_NO_FATAL_FAILURE(
          configureStreamingClip(*transport, callback, sourcePath(48000, 1, 48000), 0, 432000, rate,
                                 true, VoiceMode::MonoWithFadeOverlap,
                                 {ClipPlaybackSegment{5 * page + 1000, 5 * page + 40000, 1},
                                  ClipPlaybackSegment{page - k, page + 30000, 1}}));
      ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);
      std::vector<float> out;
      renderBlocks(*transport, out, block, 1);
      auto source = TransportControllerTestAccess::source(*transport, 1);
      ASSERT_TRUE(source);
      for (int cycle = 0; cycle < 3; ++cycle) {
        renderBlocks(*transport, out, block, 4);
        ASSERT_TRUE(waitForResident(*source, 0) && waitForResident(*source, page))
            << "segment target not prefetched";
        const auto blocks = static_cast<size_t>(
            std::ceil(static_cast<double>(39000 + 30000 + k) / (512.0 * rate)) + 4);
        renderBlocks(*transport, out, block, blocks);
      }
      EXPECT_EQ(callback.underruns.load(), 0);
      EXPECT_TRUE(callback.classified.empty());
    }
  }
}

TEST_F(StreamingSeekMatrixTest, PageMissInsideBlockRendersResidentPrefixWithClickFreeGap) {
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  constexpr uint32_t block = 512;
  auto transport = makeStreamingTransport(block);
  SeekCallback callback;
  ASSERT_NO_FATAL_FAILURE(configureStreamingClip(*transport, callback, sourcePath(48000, 1, 48000),
                                                 0, 432000, 1.0, false,
                                                 VoiceMode::MonoWithFadeOverlap));
  ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);
  std::vector<float> out;
  renderBlocks(*transport, out, block, 2);
  auto source = TransportControllerTestAccess::source(*transport, 1);
  ASSERT_TRUE(source);
  ASSERT_EQ(transport->seekClip(1, 2 * page - 200 - 8 * 512), SessionGraphError::OK);
  renderBlocks(*transport, out, block, 8);
  ASSERT_TRUE(waitForResident(*source, 2 * page));
  const size_t index = StreamingClipSourceTestAccess::residentPageIndex(*source, 2 * page);
  ASSERT_LT(index, StreamingClipSource::kNumPages);
  ASSERT_TRUE(StreamingClipSourceTestAccess::claimPage(*source, index));
  ASSERT_EQ(callback.underruns.load(), 0);

  // Block A reads [2P - 200, 2P + 313): the first 200 frames are resident, the rest are not.
  renderBlocks(*transport, out, block, 1);
  const float ref = out[0];
  ASSERT_GT(std::abs(ref), 0.1f);
  for (size_t frame = 0; frame <= 134; ++frame) {
    EXPECT_FLOAT_EQ(out[frame], ref) << "frame " << frame;
  }
  for (size_t frame = 136; frame <= 198; ++frame) {
    EXPECT_LE(std::abs(out[frame]), std::abs(out[frame - 1])) << "frame " << frame;
  }
  EXPECT_LT(std::abs(out[198]), 0.05f * std::abs(ref));
  for (size_t frame = 199; frame < block; ++frame) {
    EXPECT_EQ(out[frame], 0.0f) << "frame " << frame;
  }
  EXPECT_EQ(callback.underruns.load(), 1);
  ASSERT_EQ(callback.classified.size(), 1u);
  EXPECT_EQ(callback.classified[0].cause, UnderrunCause::StreamingPageMiss);
  EXPECT_EQ(callback.classified[0].handle, 1u);
  EXPECT_EQ(callback.classified[0].sourceFrame, 2 * page);
  EXPECT_EQ(callback.classified[0].concealedFrames, 313u);
  EXPECT_EQ(callback.classified[0].cumulativeCount, 1u);

  StreamingClipSourceTestAccess::releaseClaim(*source, index);
  renderBlocks(*transport, out, block, 1);
  EXPECT_EQ(out[0], 0.0f);
  for (size_t frame = 1; frame < 64; ++frame) {
    EXPECT_GE(std::abs(out[frame]), std::abs(out[frame - 1])) << "frame " << frame;
  }
  for (size_t frame = 64; frame < block; ++frame) {
    EXPECT_FLOAT_EQ(out[frame], ref) << "frame " << frame;
  }
  EXPECT_EQ(callback.underruns.load(), 1);
}

TEST_F(StreamingSeekMatrixTest, RecoveryThatMissesAgainFadesInFromSilence) {
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  constexpr uint32_t block = 512;
  auto transport = makeStreamingTransport(block);
  SeekCallback callback;
  ASSERT_NO_FATAL_FAILURE(configureStreamingClip(*transport, callback, sourcePath(48000, 1, 48000),
                                                 0, 432000, 1.0, false,
                                                 VoiceMode::MonoWithFadeOverlap));
  ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);
  std::vector<float> out;
  renderBlocks(*transport, out, block, 2);
  auto source = TransportControllerTestAccess::source(*transport, 1);
  ASSERT_TRUE(source);
  // Block A reads [3P - 924, 3P - 412) and block B reads [3P - 412, 3P + 100).
  ASSERT_EQ(transport->seekClip(1, 3 * page - 924 - 8 * 512), SessionGraphError::OK);
  renderBlocks(*transport, out, block, 8);
  ASSERT_TRUE(waitForResident(*source, 2 * page));
  ASSERT_TRUE(waitForResident(*source, 3 * page));
  const size_t pageX = StreamingClipSourceTestAccess::residentPageIndex(*source, 2 * page);
  const size_t pageY = StreamingClipSourceTestAccess::residentPageIndex(*source, 3 * page);
  ASSERT_LT(pageX, StreamingClipSource::kNumPages);
  ASSERT_LT(pageY, StreamingClipSource::kNumPages);
  ASSERT_EQ(callback.underruns.load(), 0);
  const uint32_t previousX = StreamingClipSourceTestAccess::blockReads(*source, pageX);
  const uint32_t previousY = StreamingClipSourceTestAccess::blockReads(*source, pageY);

  // Block A lies wholly inside the blocked page: silent from its first frame.
  renderBlocks(*transport, out, block, 1);
  EXPECT_EQ(callback.underruns.load(), 1);
  for (size_t frame = 0; frame < block; ++frame) {
    EXPECT_EQ(out[frame], 0.0f) << "frame " << frame;
  }

  // Block B has 412 resident frames and then misses again. Its resident part must fade in
  // from silence, then fade out ahead of the new gap: never a step from silence to full level.
  StreamingClipSourceTestAccess::unblockReads(*source, pageX, previousX);
  renderBlocks(*transport, out, block, 1);
  const float ref = out[200];
  ASSERT_GT(std::abs(ref), 0.1f);
  EXPECT_EQ(out[0], 0.0f);
  for (size_t frame = 1; frame < 64; ++frame) {
    EXPECT_GE(std::abs(out[frame]), std::abs(out[frame - 1])) << "frame " << frame;
  }
  for (size_t frame = 64; frame < 347; ++frame) {
    EXPECT_FLOAT_EQ(out[frame], ref) << "frame " << frame;
  }
  for (size_t frame = 348; frame < 411; ++frame) {
    EXPECT_LE(std::abs(out[frame]), std::abs(out[frame - 1])) << "frame " << frame;
  }
  for (size_t frame = 411; frame < block; ++frame) {
    EXPECT_EQ(out[frame], 0.0f) << "frame " << frame;
  }
  EXPECT_EQ(callback.underruns.load(), 2);

  // Block C is fully resident and follows a gap at the end of B, so it fades in as well.
  StreamingClipSourceTestAccess::unblockReads(*source, pageY, previousY);
  renderBlocks(*transport, out, block, 1);
  EXPECT_EQ(out[0], 0.0f);
  for (size_t frame = 1; frame < 64; ++frame) {
    EXPECT_GE(std::abs(out[frame]), std::abs(out[frame - 1])) << "frame " << frame;
  }
  for (size_t frame = 64; frame < block; ++frame) {
    EXPECT_FLOAT_EQ(out[frame], ref) << "frame " << frame;
  }
  EXPECT_EQ(callback.underruns.load(), 2);

  ASSERT_EQ(callback.classified.size(), 2u);
  EXPECT_EQ(callback.classified[0].sourceFrame, 3 * page - 924);
  EXPECT_EQ(callback.classified[0].concealedFrames, 512u);
  EXPECT_EQ(callback.classified[0].cumulativeCount, 1u);
  EXPECT_EQ(callback.classified[1].sourceFrame, 3 * page);
  EXPECT_EQ(callback.classified[1].concealedFrames, 101u);
  EXPECT_EQ(callback.classified[1].cumulativeCount, 2u);
}

TEST_F(StreamingSeekMatrixTest, LoopStartInterpolationMissIsReported) {
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  constexpr uint32_t block = 512;
  const int64_t trimIn = page + 1000;
  const int64_t trimOut = 2 * page + 3000;
  auto transport = makeStreamingTransport(block);
  SeekCallback callback;
  ASSERT_NO_FATAL_FAILURE(configureStreamingClip(*transport, callback, sourcePath(48000, 1, 48000),
                                                 trimIn, trimOut, 1.0, true,
                                                 VoiceMode::MonoWithFadeOverlap));
  ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);
  std::vector<float> out;
  // Stop one block short of the block that straddles trim-out.
  const auto before_straddle = static_cast<size_t>((trimOut - trimIn) / block);
  renderBlocks(*transport, out, block, before_straddle);
  ASSERT_EQ(callback.loops.load(), 0);
  auto source = TransportControllerTestAccess::source(*transport, 1);
  ASSERT_TRUE(source);
  ASSERT_TRUE(waitForResident(*source, 2 * page));
  const int before = callback.underruns.load();
  const size_t trimInPage = StreamingClipSourceTestAccess::residentPageIndex(*source, page);
  ASSERT_LT(trimInPage, StreamingClipSource::kNumPages);
  const uint32_t previous = StreamingClipSourceTestAccess::blockReads(*source, trimInPage);
  renderBlocks(*transport, out, block, 1);
  StreamingClipSourceTestAccess::unblockReads(*source, trimInPage, previous);
  EXPECT_EQ(callback.loops.load(), 1);
  // One interpolation-only miss at the loop start and one post-wrap read miss.
  EXPECT_EQ(callback.underruns.load() - before, 2);
  ASSERT_GE(callback.classified.size(), 2u);
  const auto& interpolation = callback.classified[callback.classified.size() - 2];
  const auto& postWrap = callback.classified.back();
  EXPECT_EQ(interpolation.concealedFrames, 0u);
  EXPECT_EQ(interpolation.sourceFrame, page + 1000);
  EXPECT_GT(postWrap.concealedFrames, 0u);
  EXPECT_EQ(postWrap.cumulativeCount, interpolation.cumulativeCount + 1);
}

// A looping clip's stop fade wraps at trim-out, so it stays inside the edited loop (OCC264).
// The source is a ramp, so every interpolated frame identifies the source position it came from.

constexpr int64_t kFloatWavFrames = 432000;
constexpr float kDcLevel = 0.5f;

float rampSample(int64_t frame) {
  return static_cast<float>(frame) * 1e-6f;
}

std::string writeFloatWav(const std::filesystem::path& directory, const std::string& name,
                          bool ramp) {
  const std::filesystem::path path = directory / name;
  std::ofstream file(path, std::ios::binary);
  const uint32_t dataSize = static_cast<uint32_t>(kFloatWavFrames * sizeof(float));
  const uint32_t fileSize = 36 + dataSize;
  const uint32_t fmtSize = 16;
  const uint16_t audioFormat = 3; // IEEE float
  const uint16_t channels = 1;
  const uint32_t sampleRate = 48000;
  const uint16_t blockAlign = sizeof(float);
  const uint32_t byteRate = sampleRate * blockAlign;
  const uint16_t bitsPerSample = 32;
  file.write("RIFF", 4);
  file.write(reinterpret_cast<const char*>(&fileSize), 4);
  file.write("WAVE", 4);
  file.write("fmt ", 4);
  file.write(reinterpret_cast<const char*>(&fmtSize), 4);
  file.write(reinterpret_cast<const char*>(&audioFormat), 2);
  file.write(reinterpret_cast<const char*>(&channels), 2);
  file.write(reinterpret_cast<const char*>(&sampleRate), 4);
  file.write(reinterpret_cast<const char*>(&byteRate), 4);
  file.write(reinterpret_cast<const char*>(&blockAlign), 2);
  file.write(reinterpret_cast<const char*>(&bitsPerSample), 2);
  file.write("data", 4);
  file.write(reinterpret_cast<const char*>(&dataSize), 4);
  for (int64_t frame = 0; frame < kFloatWavFrames; ++frame) {
    const float value = ramp ? rampSample(frame) : kDcLevel;
    file.write(reinterpret_cast<const char*>(&value), sizeof(value));
  }
  return path.string();
}

struct FadeClipSetup {
  int64_t trimIn{0};
  int64_t trimOut{0};
  double rate{1.0};
  bool loop{false};
  double stopFadeSeconds{0.01};
  double naturalFadeOutSeconds{0.0};
};

void configureFadeClip(TransportController& transport, SeekCallback& callback, ClipHandle handle,
                       const std::string& path, const FadeClipSetup& setup, VoiceMode mode) {
  transport.setPreparedSourceMaxFrames(48000);
  transport.setCallback(&callback);
  ASSERT_EQ(transport.registerClipAudio(handle, path), SessionGraphError::OK);
  ASSERT_EQ(transport.setClipVoiceMode(handle, mode), SessionGraphError::OK);
  auto metadata = transport.getClipMetadata(handle);
  ASSERT_TRUE(metadata.has_value());
  metadata->trimInSamples = setup.trimIn;
  metadata->trimOutSamples = setup.trimOut;
  metadata->loopEnabled = setup.loop;
  metadata->playbackRate = setup.rate;
  metadata->stopFadeOutSeconds = setup.stopFadeSeconds;
  metadata->fadeOutSeconds = setup.naturalFadeOutSeconds;
  ASSERT_EQ(transport.updateClipMetadata(handle, *metadata), SessionGraphError::OK);
}

// Appends every rendered block to `captured`. One millisecond per 64-frame block is the real-time
// pace (1.33 ms), so the stream worker keeps the pages the program reads ahead of the voice.
void renderPaced(TransportController& transport, std::vector<float>& captured, uint32_t block,
                 size_t blocks) {
  std::vector<float> buffer(block);
  float* buffers[] = {buffer.data()};
  for (size_t index = 0; index < blocks; ++index) {
    std::fill(buffer.begin(), buffer.end(), 0.0f);
    transport.processAudio(buffers, 1, block);
    transport.processCallbacks();
    captured.insert(captured.end(), buffer.begin(), buffer.end());
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

enum class StopAction { StopClip, StopOthers, GroupChoke, StopAll };

const char* stopActionName(StopAction action) {
  switch (action) {
  case StopAction::StopClip:
    return "stopClip";
  case StopAction::StopOthers:
    return "stopOtherClips";
  case StopAction::GroupChoke:
    return "startClipWithGroupChoke";
  case StopAction::StopAll:
    return "stopAllClips";
  }
  return "";
}

TEST_F(StreamingSeekMatrixTest, StopFadeCrossingTrimOutWrapsWithoutPageMiss) {
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  constexpr uint32_t block = 64;
  const int64_t trimIn = 2 * page - 256;
  const int64_t lead = 9280; // source frames: 0.2 s of output at rate 0.967
  // Trim-out on a page boundary, mid-page, and 3 frames short of a page end.
  for (const int64_t trimOut : {5 * page, 5 * page + 7000, 6 * page - 3}) {
    for (const StopAction action : {StopAction::StopClip, StopAction::StopOthers,
                                    StopAction::GroupChoke, StopAction::StopAll}) {
      SCOPED_TRACE("trimOut=" + std::to_string(trimOut) + " " + stopActionName(action));
      auto transport = makeStreamingTransport(block);
      SeekCallback callback;
      FadeClipSetup setup;
      setup.trimIn = trimIn;
      setup.trimOut = trimOut;
      setup.rate = 0.967;
      setup.loop = true;
      setup.stopFadeSeconds = 0.6;
      ASSERT_NO_FATAL_FAILURE(configureFadeClip(*transport, callback, 1,
                                                sourcePath(48000, 1, 48000), setup,
                                                VoiceMode::MonoWithFadeOverlap));
      ASSERT_EQ(transport->registerClipAudio(2, sourcePath(48000, 1, 48000)),
                SessionGraphError::OK);
      ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);
      std::vector<float> out;
      renderBlocks(*transport, out, block, 2);
      auto source = TransportControllerTestAccess::source(*transport, 1);
      ASSERT_TRUE(source);

      ASSERT_EQ(transport->seekClip(1, trimOut - lead), SessionGraphError::OK);
      switch (action) {
      case StopAction::StopClip:
        ASSERT_EQ(transport->stopClip(1), SessionGraphError::OK);
        break;
      case StopAction::StopOthers:
        ASSERT_EQ(transport->stopOtherClips(2), SessionGraphError::OK);
        break;
      case StopAction::GroupChoke:
        ASSERT_EQ(transport->startClipWithGroupChoke(2), SessionGraphError::OK);
        break;
      case StopAction::StopAll:
        ASSERT_EQ(transport->stopAllClips(), SessionGraphError::OK);
        break;
      }
      renderBlocks(*transport, out, block, 1);
      ASSERT_TRUE(waitForResident(*source, trimOut / page * page));
      ASSERT_TRUE(waitForResident(*source, trimIn / page * page));
      std::vector<float> captured;
      renderPaced(*transport, captured, block, 480);

      const auto clipOneMisses =
          std::count_if(callback.classified.begin(), callback.classified.end(),
                        [](const TransportUnderrun& miss) { return miss.handle == 1; });
      EXPECT_EQ(clipOneMisses, 0);
      if (action == StopAction::StopClip || action == StopAction::StopAll) {
        EXPECT_EQ(callback.underruns.load(), 0);
      }
      // The 0.6 s fade outlasts the 0.2 s that remained, so it wrapped once and then retired.
      EXPECT_EQ(callback.loops.load(), 1);
      EXPECT_EQ(transport->getClipState(1), PlaybackState::Stopped);
    }
  }
}

TEST_F(StreamingSeekMatrixTest, LoopEnabledDuringNaturalEndTailStaysSilentWithoutPageMiss) {
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  constexpr uint32_t block = 64;
  constexpr double rate = 0.967;
  const int64_t trimIn = 2 * page - 256;
  const int64_t trimOut = 5 * page + 7000;
  const int64_t lead = 3000;
  auto transport = makeStreamingTransport(block);
  SeekCallback callback;
  FadeClipSetup setup;
  setup.trimIn = trimIn;
  setup.trimOut = trimOut;
  setup.rate = rate;
  setup.loop = false;
  setup.naturalFadeOutSeconds = 0.75;
  ASSERT_NO_FATAL_FAILURE(configureFadeClip(*transport, callback, 1,
                                            writeFloatWav(s_directory, "dc.wav", false), setup,
                                            VoiceMode::Polyphonic));
  ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);
  std::vector<float> out;
  renderBlocks(*transport, out, block, 2);
  auto source = TransportControllerTestAccess::source(*transport, 1);
  ASSERT_TRUE(source);

  ASSERT_EQ(transport->seekClip(1, trimOut - lead), SessionGraphError::OK);
  std::vector<float> captured;
  renderPaced(*transport, captured, block, 1);
  ASSERT_TRUE(waitForResident(*source, trimOut / page * page));
  // The first output frame at or past trim-out begins the natural end.
  const auto naturalEndFrame = static_cast<size_t>(std::ceil(static_cast<double>(lead) / rate));
  // 0.3 s into the tail, a metadata update switches looping on.
  renderPaced(*transport, captured, block, (naturalEndFrame + 14400) / block);
  auto metadata = transport->getClipMetadata(1);
  ASSERT_TRUE(metadata.has_value());
  metadata->loopEnabled = true;
  ASSERT_EQ(transport->updateClipMetadata(1, *metadata), SessionGraphError::OK);
  renderPaced(*transport, captured, block, 400);

  ASSERT_GT(captured.size(), naturalEndFrame + 36000 + block);
  EXPECT_GT(captured[0], 0.01f) << "the clip plays, in its natural-end fade, before trim-out";
  for (size_t frame = naturalEndFrame; frame < captured.size(); ++frame) {
    ASSERT_EQ(captured[frame], 0.0f) << "audio past trim-out at output frame " << frame;
  }
  EXPECT_EQ(callback.underruns.load(), 0);
  EXPECT_TRUE(callback.classified.empty());
  EXPECT_EQ(callback.loops.load(), 0) << "a natural-end tail never wraps";
  EXPECT_EQ(transport->getClipState(1), PlaybackState::Stopped);
}

// Renders the same schedule for one source: seek `lead` source frames short of trim-out and stop.
void renderStopAcrossTrimOut(const std::string& path, const FadeClipSetup& setup, int64_t lead,
                             size_t blocks, SeekCallback& callback, std::vector<float>& captured,
                             PlaybackState& finalState) {
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  constexpr uint32_t block = 64;
  auto transport = makeStreamingTransport(block);
  ASSERT_NO_FATAL_FAILURE(
      configureFadeClip(*transport, callback, 1, path, setup, VoiceMode::Polyphonic));
  ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);
  std::vector<float> scratch;
  renderBlocks(*transport, scratch, block, 2);
  auto source = TransportControllerTestAccess::source(*transport, 1);
  ASSERT_TRUE(source);
  ASSERT_EQ(transport->seekClip(1, setup.trimOut - lead), SessionGraphError::OK);
  ASSERT_EQ(transport->stopClip(1), SessionGraphError::OK);
  renderPaced(*transport, captured, block, 1);
  ASSERT_TRUE(waitForResident(*source, setup.trimOut / page * page));
  ASSERT_TRUE(waitForResident(*source, setup.trimIn / page * page));
  renderPaced(*transport, captured, block, blocks - 1);
  finalState = transport->getClipState(1);
}

// Expected render of the ramp: linear interpolation along the voice's wrapped source positions,
// times the stop-fade gain read off a constant source rendered on the same schedule.
void expectWrappedLoopTail(const FadeClipSetup& setup, int64_t lead, double fadeSeconds,
                           size_t minimumWraps, const std::string& rampPath,
                           const std::string& dcPath) {
  constexpr uint32_t block = 64;
  const size_t fadeFrames = static_cast<size_t>(std::llround(fadeSeconds * 48000.0));
  const size_t blocks = fadeFrames / block + 40;
  SeekCallback dcCallback;
  SeekCallback rampCallback;
  std::vector<float> dc;
  std::vector<float> ramp;
  PlaybackState dcState = PlaybackState::Playing;
  PlaybackState rampState = PlaybackState::Playing;
  ASSERT_NO_FATAL_FAILURE(
      renderStopAcrossTrimOut(dcPath, setup, lead, blocks, dcCallback, dc, dcState));
  ASSERT_NO_FATAL_FAILURE(
      renderStopAcrossTrimOut(rampPath, setup, lead, blocks, rampCallback, ramp, rampState));
  ASSERT_EQ(dc.size(), ramp.size());
  ASSERT_EQ(dc.size(), blocks * block);
  EXPECT_EQ(dcCallback.underruns.load(), 0);
  EXPECT_EQ(rampCallback.underruns.load(), 0);
  EXPECT_EQ(rampState, PlaybackState::Stopped) << "the voice retires when the fade ends";
  EXPECT_GT(dc[0], 0.45f);

  double position = static_cast<double>(setup.trimOut - lead);
  size_t wraps = 0;
  size_t wrappedFramesCompared = 0;
  for (size_t frame = 0; frame < ramp.size(); ++frame) {
    if (position >= static_cast<double>(setup.trimOut)) {
      position = static_cast<double>(setup.trimIn);
      ++wraps;
    }
    const auto first = static_cast<int64_t>(std::floor(position));
    const auto fraction = static_cast<float>(position - static_cast<double>(first));
    const float a = rampSample(first);
    // The frame after trim-out is the loop start.
    const float b = rampSample(first + 1 >= setup.trimOut ? setup.trimIn : first + 1);
    const float gain = dc[frame] / kDcLevel;
    const float expected = (a + (b - a) * fraction) * gain;
    ASSERT_NEAR(ramp[frame], expected, 2e-6f) << "output frame " << frame << " wraps " << wraps;
    if (wraps > 0 && gain > 0.0f) {
      ++wrappedFramesCompared;
    }
    position += setup.rate;
  }
  EXPECT_GE(wraps, minimumWraps);
  EXPECT_EQ(static_cast<size_t>(rampCallback.loops.load()), wraps);
  EXPECT_GT(wrappedFramesCompared, 1000u) << "the fade must continue audibly after the wrap";
  // The fade is over: nothing is rendered after it.
  for (size_t frame = fadeFrames + block; frame < ramp.size(); ++frame) {
    ASSERT_EQ(ramp[frame], 0.0f) << "output frame " << frame;
  }
}

TEST_F(StreamingSeekMatrixTest, StopFadeTailPastTrimOutIsTheWrappedLoopAtRate0967) {
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  const std::string rampPath = writeFloatWav(s_directory, "ramp.wav", true);
  const std::string dcPath = writeFloatWav(s_directory, "dc.wav", false);
  FadeClipSetup setup;
  setup.trimIn = 2 * page - 256;
  setup.trimOut = 5 * page + 7000;
  setup.rate = 0.967;
  setup.loop = true;
  setup.stopFadeSeconds = 0.6;
  // 0.6 s fade, stop pressed 0.2 s before OUT: 0.2 s to OUT, then the remaining 0.4 s from IN.
  ASSERT_NO_FATAL_FAILURE(expectWrappedLoopTail(setup, 9280, 0.6, 1, rampPath, dcPath));
}

TEST_F(StreamingSeekMatrixTest, StopFadeLongerThanTheLoopWrapsRepeatedlyAndRetires) {
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  const std::string rampPath = writeFloatWav(s_directory, "ramp.wav", true);
  const std::string dcPath = writeFloatWav(s_directory, "dc.wav", false);
  FadeClipSetup setup;
  setup.trimIn = 2 * page - 256;
  setup.trimOut = setup.trimIn + 40000;
  // A stop fade may not exceed the clip window in seconds, so a fade outlasts the loop only when
  // the loop plays faster than its source rate: 0.42 s per loop at rate 2.0 against a 0.8 s fade.
  setup.rate = 2.0;
  setup.loop = true;
  setup.stopFadeSeconds = 0.8;
  ASSERT_NO_FATAL_FAILURE(expectWrappedLoopTail(setup, 4800, 0.8, 2, rampPath, dcPath));
}

TEST_F(StreamingSeekMatrixTest, PageMissReportsWhetherTheVoiceWasAlreadyFadingOut) {
  constexpr uint32_t block = 512;
  auto transport = makeStreamingTransport(block);
  SeekCallback callback;
  ASSERT_NO_FATAL_FAILURE(configureStreamingClip(*transport, callback, sourcePath(48000, 1, 48000),
                                                 0, 9 * 48000, 1.0, false, VoiceMode::Polyphonic));
  auto metadata = transport->getClipMetadata(1);
  ASSERT_TRUE(metadata.has_value());
  metadata->stopFadeOutSeconds = 0.5;
  ASSERT_EQ(transport->updateClipMetadata(1, *metadata), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);
  std::vector<float> out;
  renderBlocks(*transport, out, block, 4);
  auto source = TransportControllerTestAccess::source(*transport, 1);
  ASSERT_TRUE(source);
  const size_t pageIndex = StreamingClipSourceTestAccess::residentPageIndex(*source, 0);
  ASSERT_LT(pageIndex, StreamingClipSource::kNumPages);

  uint32_t previous = StreamingClipSourceTestAccess::blockReads(*source, pageIndex);
  renderBlocks(*transport, out, block, 1);
  StreamingClipSourceTestAccess::unblockReads(*source, pageIndex, previous);
  ASSERT_FALSE(callback.classified.empty());
  EXPECT_FALSE(callback.classified.back().voiceStopping);

  const size_t missesWhilePlaying = callback.classified.size();
  ASSERT_EQ(transport->stopClip(1), SessionGraphError::OK);
  renderBlocks(*transport, out, block, 1);
  previous = StreamingClipSourceTestAccess::blockReads(*source, pageIndex);
  renderBlocks(*transport, out, block, 1);
  StreamingClipSourceTestAccess::unblockReads(*source, pageIndex, previous);
  ASSERT_GT(callback.classified.size(), missesWhilePlaying);
  EXPECT_TRUE(callback.classified.back().voiceStopping);
}

TEST_F(StreamingSeekMatrixTest,
       StopFadeAcrossTheEndOfANonLoopingSegmentProgramStaysAnOperatorStop) {
  // The last window of a program that does not loop has nothing to wrap to: the fade reads
  // through it and keeps its own length. Treating the window end as a natural end would restart
  // the fade with the (much longer) natural-end fade-out.
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  constexpr uint32_t block = 64;
  auto transport = makeStreamingTransport(block);
  SeekCallback callback;
  const std::vector<ClipPlaybackSegment> segments = {{2 * page, 2 * page + 2000, 1},
                                                     {4 * page, 4 * page + 3000, 1}};
  ASSERT_NO_FATAL_FAILURE(configureStreamingClip(*transport, callback, sourcePath(48000, 1, 48000),
                                                 0, 9 * 48000, 1.0, false, VoiceMode::Polyphonic,
                                                 segments));
  auto metadata = transport->getClipMetadata(1);
  ASSERT_TRUE(metadata.has_value());
  metadata->stopFadeOutSeconds = 0.4;
  metadata->fadeOutSeconds = 1.5;
  ASSERT_EQ(transport->updateClipMetadata(1, *metadata), SessionGraphError::OK);
  ASSERT_EQ(transport->startClip(1), SessionGraphError::OK);
  std::vector<float> captured;
  renderPaced(*transport, captured, block, 20);
  auto source = TransportControllerTestAccess::source(*transport, 1);
  ASSERT_TRUE(source);
  ASSERT_TRUE(waitForResident(*source, 4 * page));
  // 2000 frames of the first window, then 1328 of the last 3000-frame window: 1672 frames short
  // of the end of the program when the stop arrives.
  renderPaced(*transport, captured, block, 32);
  ASSERT_EQ(transport->getClipState(1), PlaybackState::Playing);
  ASSERT_EQ(transport->stopClip(1), SessionGraphError::OK);

  // The 0.4 s fade is 300 blocks of 64 frames and crosses the program end; the natural-end
  // fade-out would run 1.5 s.
  int stoppedAfterBlocks = -1;
  for (int blocks = 1; blocks <= 400; ++blocks) {
    renderPaced(*transport, captured, block, 1);
    if (transport->getClipState(1) == PlaybackState::Stopped) {
      stoppedAfterBlocks = blocks;
      break;
    }
  }
  EXPECT_GE(stoppedAfterBlocks, 300);
  EXPECT_LE(stoppedAfterBlocks, 302);
  EXPECT_EQ(callback.underruns.load(), 0);
  EXPECT_EQ(callback.loops.load(), 0);
}

TEST(StreamingClipSourceDemandTest, ResidentFrameReadMovesNeitherDemandNorPages) {
  const int64_t page = static_cast<int64_t>(StreamingClipSource::kPageFrames);
  auto reader = std::make_shared<FaultReader>(8 * page);
  ASSERT_TRUE(reader->open("").isOk());
  StreamingClipSource source(reader, 1, 8 * page);
  ASSERT_EQ(source.prefill(0, 1), SessionGraphError::OK);
  ASSERT_LT(StreamingClipSourceTestAccess::residentPageIndex(source, 0),
            StreamingClipSource::kNumPages);

  float expected = 0.0f;
  size_t framesRead = 0;
  ASSERT_TRUE(source.read(37, &expected, 1, framesRead));
  ASSERT_EQ(framesRead, 1u);

  // A voice whose cursor is far from page 0 asks for a frame of page 0.
  std::array<int64_t, kVoiceDemandTestCapacity> positions{};
  positions[0] = 6 * page;
  StreamingClipSourceTestAccess::publishDemands(source, positions, 1);
  float frame = -1.0f;
  EXPECT_TRUE(StreamingClipSourceTestAccess::readResidentFrame(source, 37, &frame));
  EXPECT_EQ(frame, expected);
  EXPECT_LT(StreamingClipSourceTestAccess::residentPageIndex(source, 0),
            StreamingClipSource::kNumPages)
      << "an auxiliary read must not retire the page it copied from";
  StreamingClipSourceTestAccess::PublishedDemandSnapshot snapshot{};
  ASSERT_TRUE(StreamingClipSourceTestAccess::readPublishedDemands(source, snapshot));
  ASSERT_EQ(snapshot.count, 1u);
  EXPECT_EQ(snapshot.positions[0], 6 * page);

  EXPECT_FALSE(StreamingClipSourceTestAccess::readResidentFrame(source, 3 * page, &frame));
  EXPECT_FALSE(StreamingClipSourceTestAccess::readResidentFrame(source, 8 * page, &frame));
  EXPECT_FALSE(StreamingClipSourceTestAccess::readResidentFrame(source, -1, &frame));
}

TEST(ResamplingSeekPrimeTest, WrappedReaderErrorIsNotConvertedToEof) {
  auto reader = std::make_shared<FaultReader>(48000);
  ResamplingAudioFileReader resampling(reader, 44100);
  ASSERT_TRUE(resampling.open("").isOk());
  std::vector<float> samples(512, 0.0f);
  reader->failRead.store(true, std::memory_order_relaxed);
  auto failedRead = resampling.readSamples(samples.data(), samples.size());
  EXPECT_FALSE(failedRead.isOk());
  EXPECT_EQ(failedRead.error, SessionGraphError::NotReady);
  EXPECT_EQ(failedRead.value, 0u);
  const int64_t position = resampling.getCurrentPosition();
  reader->failSeek.store(true, std::memory_order_relaxed);
  EXPECT_EQ(resampling.seek(100), SessionGraphError::NotReady);
  EXPECT_EQ(resampling.getCurrentPosition(), position);
  reader->failSeek.store(false, std::memory_order_relaxed);
  reader->failRead.store(false, std::memory_order_relaxed);
  auto recovered = resampling.readSamples(samples.data(), samples.size());
  EXPECT_TRUE(recovered.isOk());
  EXPECT_GT(recovered.value, 0u);
}

} // namespace

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
