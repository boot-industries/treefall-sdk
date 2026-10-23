// SPDX-License-Identifier: MIT
// ORP168: rendered first-block streaming seek and command-prime transactions.

#include "../support/rt_guard.hpp"

#include "audio_io/resampling_audio_file_reader.h"
#include "transport/transport_controller.h"

#include <orpheus/clip_source.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <barrier>
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
      source.addVoiceDemand(positions[index]);
    }
    source.publishVoiceDemands();
  }

  static bool readPublishedDemands(StreamingClipSource& source, PublishedDemandSnapshot& result) {
    StreamingClipSource::VoiceDemandSnapshot snapshot{};
    if (!source.readWorkerVoiceDemands(snapshot)) {
      return false;
    }
    result.count = snapshot.count;
    result.positions = snapshot.positions;
    return true;
  }

  static bool readForVoice(StreamingClipSource& source, size_t demandIndex, int64_t position,
                           float* destination, size_t frames, size_t& framesRead) {
    return source.readForVoice(demandIndex, position, destination, frames, framesRead);
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
        streaming->prepareLoopAnchorTransition(trimIn, true, command.loopAnchorTransition);
    if (result != SessionGraphError::OK) {
      return result;
    }
    command.loopAnchorSource = streaming;
    replacementIndex = command.loopAnchorTransition.replacementPageIndex;
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

  void onClipStarted(ClipHandle, orpheus::StartRequestTag requestTag, uint32_t,
                     TransportPosition) override {
    starts.fetch_add(1, std::memory_order_relaxed);
    if (requestTag == 2) {
      secondRequestStarts.fetch_add(1, std::memory_order_relaxed);
    }
  }
  void onClipStopped(ClipHandle, orpheus::StartRequestTag, uint32_t, TransportPosition) override {}
  void onClipLooped(ClipHandle, orpheus::StartRequestTag, uint32_t, TransportPosition) override {}
  void onBufferUnderrun(TransportPosition) override {
    underruns.fetch_add(1, std::memory_order_relaxed);
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
    s_directory = std::filesystem::temp_directory_path() / "orp168_streaming_seek";
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
  ASSERT_EQ(StreamingClipSourceTestAccess::pageGuard(*source, oldIndex), 1u);

  ASSERT_EQ(transport->updateClipTrimPoints(1, 3 * page + 100, trimOut), SessionGraphError::OK);
  const size_t intermediateIndex =
      StreamingClipSourceTestAccess::residentPageIndex(*source, 3 * page);
  ASSERT_LT(intermediateIndex, StreamingClipSource::kNumPages);
  // Preparing a later command must not mutate the already-admitted transition.
  ASSERT_EQ(transport->updateClipTrimPoints(1, 4 * page + 100, trimOut), SessionGraphError::OK);
  const size_t finalIndex = StreamingClipSourceTestAccess::residentPageIndex(*source, 4 * page);
  ASSERT_LT(finalIndex, StreamingClipSource::kNumPages);
  EXPECT_EQ(StreamingClipSourceTestAccess::pageGuard(*source, oldIndex), 1u);
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
  EXPECT_EQ(StreamingClipSourceTestAccess::pageGuard(*source, oldIndex), 0u);
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
  ASSERT_EQ(source->prepareLoopAnchorTransition(0, false, inspection), SessionGraphError::OK);
  EXPECT_EQ(inspection.previousPageIndex, oldIndex);
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
  ASSERT_EQ(source->prepareLoopAnchorTransition(0, true, anchor), SessionGraphError::OK);
  const size_t anchorIndex = anchor.replacementPageIndex;
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
  ASSERT_EQ(source->prepareLoopAnchorTransition(0, false, disable), SessionGraphError::OK);
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
    ASSERT_EQ(source->prepareLoopAnchorTransition(0, true, anchor), SessionGraphError::OK);
    const size_t anchorIndex = anchor.replacementPageIndex;
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
    ASSERT_EQ(source->prepareLoopAnchorTransition(0, false, disable), SessionGraphError::OK);
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
  ASSERT_EQ(source.prepareLoopAnchorTransition(0, true, anchor), SessionGraphError::OK);
  const size_t pageIndex = anchor.replacementPageIndex;
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
  ASSERT_EQ(source.prepareLoopAnchorTransition(0, false, disabled), SessionGraphError::OK);
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
  ASSERT_EQ(source.prepareLoopAnchorTransition(0, true, anchor), SessionGraphError::OK);
  const size_t oldIndex = anchor.replacementPageIndex;
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
  EXPECT_EQ(source.prepareLoopAnchorTransition(10 * page, true, failed),
            SessionGraphError::NotReady);
  EXPECT_EQ(failed.previousPageIndex,
            StreamingClipSource::LoopAnchorTransition::kInactivePageIndex);
  EXPECT_EQ(failed.replacementPageIndex,
            StreamingClipSource::LoopAnchorTransition::kInactivePageIndex);
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
  ASSERT_EQ(source.prepareLoopAnchorTransition(0, false, released), SessionGraphError::OK);
  source.commitLoopAnchorTransition(released);

  StreamingClipSource::PrimeReservation recovered{};
  ASSERT_EQ(source.primeForCommand(10 * page, 32, recovered), SessionGraphError::OK);
  source.releaseCommandPrime(recovered);
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
  ASSERT_EQ(source.prepareLoopAnchorTransition(6 * page, true, anchor), SessionGraphError::OK);
  source.commitLoopAnchorTransition(anchor);
  StreamingClipSource::LoopAnchorTransition released{};
  ASSERT_EQ(source.prepareLoopAnchorTransition(0, false, released), SessionGraphError::OK);
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
