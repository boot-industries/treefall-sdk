// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>
#include <orpheus/audio_driver.h>
#include <orpheus/transport_controller.h>

// Access implementation for processAudio
#include "transport/transport_controller.h"
#include "../support/unique_temp_dir.hpp"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <thread>

using namespace orpheus;

// Adapter to connect TransportController to IAudioCallback
class TransportAudioAdapter : public IAudioCallback {
public:
  explicit TransportAudioAdapter(TransportController* transport) : m_transport(transport) {}

  void processAudio(const AudioProcessBlock& block) noexcept override {
    if (m_transport) {
      TransportTimeReference reference;
      reference.device_sample_position = block.device_sample_position;
      reference.host_time_nanoseconds = block.host_time_nanoseconds;
      reference.discontinuity = block.discontinuity;
      m_transport->processAudioWithTimeReference(block.output_buffers, block.num_output_channels,
                                                 block.num_frames, reference);
    }

    m_callback_count.fetch_add(1, std::memory_order_relaxed);
  }

  int getCallbackCount() const {
    return m_callback_count.load(std::memory_order_relaxed);
  }

private:
  TransportController* m_transport;
  std::atomic<int> m_callback_count{0};
};

// Test callback to track transport events
class TestTransportCallback : public ITransportCallback {
public:
  void onClipStarted(ClipHandle handle, orpheus::StartRequestTag, uint32_t,
                     TransportPosition position) override {
    m_start_count.fetch_add(1, std::memory_order_relaxed);
    m_last_started_handle = handle;
    (void)position;
  }

  void onClipStopped(ClipHandle handle, orpheus::StartRequestTag, uint32_t,
                     TransportPosition position) override {
    m_stop_count.fetch_add(1, std::memory_order_relaxed);
    m_last_stopped_handle = handle;
    (void)position;
  }

  void onClipLooped(ClipHandle handle, orpheus::StartRequestTag, uint32_t,
                    TransportPosition position) override {
    // Not tested yet
    (void)handle;
    (void)position;
  }

  void onBufferUnderrun(TransportPosition position) override {
    // Not tested yet
    (void)position;
  }

  int getStartCount() const {
    return m_start_count.load(std::memory_order_relaxed);
  }

  int getStopCount() const {
    return m_stop_count.load(std::memory_order_relaxed);
  }

  ClipHandle getLastStartedHandle() const {
    return m_last_started_handle;
  }
  ClipHandle getLastStoppedHandle() const {
    return m_last_stopped_handle;
  }

private:
  std::atomic<int> m_start_count{0};
  std::atomic<int> m_stop_count{0};
  ClipHandle m_last_started_handle{0};
  ClipHandle m_last_stopped_handle{0};
};

// Test fixture
class TransportIntegrationTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Create transport controller (no SessionGraph for now)
    m_transport = std::make_unique<TransportController>(
        nullptr, TransportConfig{.sampleRate = static_cast<uint32_t>(48000)});

    // Create transport callback
    m_transport_callback = std::make_unique<TestTransportCallback>();
    m_transport->setCallback(m_transport_callback.get());

    // Create adapter
    m_adapter = std::make_unique<TransportAudioAdapter>(m_transport.get());

    // Create audio driver
    m_driver = createDummyAudioDriver();

    // Initialize driver
    AudioDriverConfig config;
    config.sample_rate = 48000;
    config.buffer_size = 512;
    config.num_outputs = 2;
    m_driver->initialize(config);
  }

  void TearDown() override {
    m_driver->stop();
    m_driver.reset();
    m_adapter.reset();
    m_transport.reset();
    m_transport_callback.reset();
  }

  // Poll until `pred` holds or the deadline elapses, draining UI callbacks each
  // tick. The dummy driver fires processAudio from a background thread on a
  // timer; a fixed sleep is flaky on loaded CI runners (the first tick can be
  // delayed past any single sleep), so wait *up to* a generous deadline and
  // return as soon as the condition is met. Returns pred()'s final value so
  // callers can assert on it.
  template <typename Pred>
  bool waitForCallback(Pred pred, std::chrono::milliseconds timeout = std::chrono::seconds(2)) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    do {
      m_transport->processCallbacks();
      if (pred()) {
        return true;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    } while (std::chrono::steady_clock::now() < deadline);
    m_transport->processCallbacks();
    return pred();
  }

  std::unique_ptr<TransportController> m_transport;
  std::unique_ptr<TestTransportCallback> m_transport_callback;
  std::unique_ptr<TransportAudioAdapter> m_adapter;
  std::unique_ptr<IAudioDriver> m_driver;
};

// Integration Tests

TEST_F(TransportIntegrationTest, DriverCallsTransportProcessAudio) {
  // Start driver with transport adapter
  ASSERT_EQ(m_driver->start(m_adapter.get()), SessionGraphError::OK);

  // Verify transport's processAudio was called via adapter
  EXPECT_TRUE(waitForCallback([&] { return m_adapter->getCallbackCount() > 0; }));

  m_driver->stop();
}

TEST_F(TransportIntegrationTest, DummyRoutesStereoAudioToLastPairOfSixtyFourOutputs) {
  constexpr uint32_t sampleRate = 48000;
  constexpr uint32_t frames = 512;
  constexpr uint16_t channels = 2;
  constexpr uint16_t bitsPerSample = 16;
  constexpr uint16_t blockAlign = channels * (bitsPerSample / 8);
  constexpr uint32_t dataSize = frames * blockAlign;
  constexpr uint32_t riffSize = 36 + dataSize;
  constexpr uint32_t byteRate = sampleRate * blockAlign;
  constexpr uint32_t fmtSize = 16;
  constexpr uint16_t pcmFormat = 1;
  tests::support::UniqueTempDir scratch("orpheus_dummy_last_output_pair");
  const auto audioPath = scratch / "stereo.wav";
  {
    std::ofstream file(audioPath, std::ios::binary);
    ASSERT_TRUE(file.is_open());
    file.write("RIFF", 4);
    file.write(reinterpret_cast<const char*>(&riffSize), sizeof(riffSize));
    file.write("WAVEfmt ", 8);
    file.write(reinterpret_cast<const char*>(&fmtSize), sizeof(fmtSize));
    file.write(reinterpret_cast<const char*>(&pcmFormat), sizeof(pcmFormat));
    file.write(reinterpret_cast<const char*>(&channels), sizeof(channels));
    file.write(reinterpret_cast<const char*>(&sampleRate), sizeof(sampleRate));
    file.write(reinterpret_cast<const char*>(&byteRate), sizeof(byteRate));
    file.write(reinterpret_cast<const char*>(&blockAlign), sizeof(blockAlign));
    file.write(reinterpret_cast<const char*>(&bitsPerSample), sizeof(bitsPerSample));
    file.write("data", 4);
    file.write(reinterpret_cast<const char*>(&dataSize), sizeof(dataSize));
    for (uint32_t frame = 0; frame < frames; ++frame) {
      for (uint16_t channel = 0; channel < channels; ++channel) {
        const int16_t sample = static_cast<int16_t>((channel + 1) * 2048);
        file.write(reinterpret_cast<const char*>(&sample), sizeof(sample));
      }
    }
    ASSERT_TRUE(file.good());
  }

  TransportConfig config;
  config.sampleRate = sampleRate;
  config.outputChannels = 64;
  config.maxBlockFrames = frames;
  m_transport = std::make_unique<TransportController>(nullptr, config);
  m_transport->setCallback(m_transport_callback.get());
  m_adapter = std::make_unique<TransportAudioAdapter>(m_transport.get());
  ASSERT_EQ(m_transport->registerClipAudio(2, audioPath.string()), SessionGraphError::OK);
  ASSERT_EQ(m_transport->setGroupOutputBus(3, OutputBusRoute{62, 2}), SessionGraphError::OK);
  auto metadata = m_transport->getClipMetadata(2);
  ASSERT_TRUE(metadata.has_value());
  metadata->routingGroup = 3;
  metadata->loopEnabled = true;
  ASSERT_EQ(m_transport->updateClipMetadata(2, *metadata), SessionGraphError::OK);
  ASSERT_EQ(m_transport->startClip(2), SessionGraphError::OK);

  AudioOutputRouteRequest request;
  request.output_device_id = "dummy";
  request.requested_sample_rate = sampleRate;
  request.requested_buffer_size = frames;
  for (uint16_t channel = 0; channel < 64; ++channel) {
    request.output_channel_map.push_back(channel);
  }
  ASSERT_EQ(m_driver->initializeAudioOutput(request), SessionGraphError::OK);
  EXPECT_EQ(m_driver->getAudioIoRouteState().active_output_channel_map,
            request.output_channel_map);

  class AudioCapture final : public IAudioCallback {
  public:
    explicit AudioCapture(TransportController* transport) : adapter(transport) {}

    void processAudio(const AudioProcessBlock& block) noexcept override {
      adapter.processAudio(block);
      if (block.num_output_channels != 64 || block.num_frames != 512) {
        return;
      }
      bool matches = true;
      for (size_t channel = 0; channel < block.num_output_channels; ++channel) {
        const float expected = channel < 62 ? 0.0f
                                           : static_cast<float>(channel - 61) * 2048.0f / 32768.0f;
        for (size_t frame = 0; frame < block.num_frames; ++frame) {
          matches = matches && std::abs(block.output_buffers[channel][frame] - expected) < 1.0e-4f;
        }
      }
      if (matches) {
        observed.store(true, std::memory_order_release);
      }
    }

    TransportAudioAdapter adapter;
    std::atomic<bool> observed{false};
  } capture(m_transport.get());

  ASSERT_EQ(m_driver->start(&capture), SessionGraphError::OK);
  const bool observed =
      waitForCallback([&] { return capture.observed.load(std::memory_order_acquire); });
  // Join before assertions can destroy the stack-owned callback or scratch audio.
  EXPECT_EQ(m_driver->stop(), SessionGraphError::OK);
  EXPECT_TRUE(observed) << "Expected real callback audio on outputs 62/63 and silence on 0-61";
  EXPECT_GT(capture.adapter.getCallbackCount(), 0);
  m_adapter.reset();
  m_transport.reset(); // Close audio readers before the scratch directory is removed.
}

TEST_F(TransportIntegrationTest, StartClipTriggersCallback) {
  // Start driver
  ASSERT_EQ(m_driver->start(m_adapter.get()), SessionGraphError::OK);

  // Start a clip
  ClipHandle handle = 42;
  ASSERT_EQ(m_transport->startClip(handle), SessionGraphError::OK);

  // Verify clip started callback was triggered
  EXPECT_TRUE(waitForCallback([&] { return m_transport_callback->getStartCount() == 1; }));
  EXPECT_EQ(m_transport_callback->getLastStartedHandle(), handle);

  m_driver->stop();
}

TEST_F(TransportIntegrationTest, StopClipTriggersCallback) {
  // Start driver
  ASSERT_EQ(m_driver->start(m_adapter.get()), SessionGraphError::OK);

  // Start a clip
  ClipHandle handle = 123;
  ASSERT_EQ(m_transport->startClip(handle), SessionGraphError::OK);

  // Wait for clip to start
  ASSERT_TRUE(waitForCallback([&] { return m_transport_callback->getStartCount() == 1; }));

  // Stop the clip
  ASSERT_EQ(m_transport->stopClip(handle), SessionGraphError::OK);

  // Verify clip stopped callback was triggered (after fade-out)
  EXPECT_TRUE(waitForCallback([&] { return m_transport_callback->getStopCount() == 1; }));
  EXPECT_EQ(m_transport_callback->getLastStoppedHandle(), handle);

  m_driver->stop();
}

TEST_F(TransportIntegrationTest, TransportPositionAdvances) {
  // Start driver
  ASSERT_EQ(m_driver->start(m_adapter.get()), SessionGraphError::OK);

  // Get initial position
  auto pos1 = m_transport->getCurrentPosition();

  // Wait for audio processing. The dummy driver fires processAudio from a
  // background thread on a timer, so a fixed sleep can miss the first tick on
  // a loaded runner; wait up to a generous deadline instead, the same way the
  // callback waits above do.
  ASSERT_TRUE(
      waitForCallback([&] { return m_transport->getCurrentPosition().samples > pos1.samples; }));

  // Get new position
  auto pos2 = m_transport->getCurrentPosition();

  // Position should have advanced
  EXPECT_GT(pos2.samples, pos1.samples);
  EXPECT_GT(pos2.seconds, pos1.seconds);

  m_driver->stop();
}

TEST_F(TransportIntegrationTest, MultipleClipsCanStart) {
  // Start driver
  ASSERT_EQ(m_driver->start(m_adapter.get()), SessionGraphError::OK);

  // Start multiple clips
  ClipHandle h1 = 1;
  ClipHandle h2 = 2;
  ClipHandle h3 = 3;

  ASSERT_EQ(m_transport->startClip(h1), SessionGraphError::OK);
  ASSERT_EQ(m_transport->startClip(h2), SessionGraphError::OK);
  ASSERT_EQ(m_transport->startClip(h3), SessionGraphError::OK);

  // Verify all clips started. Wait on the published voice snapshot itself, not
  // the onClipStarted callback count: the callback fires when the start command
  // is consumed, but getClipState() reads a separately-published voice snapshot
  // that can lag the callback by a tick, so polling the state is the correct
  // condition (and implies the callbacks fired).
  EXPECT_TRUE(waitForCallback([&] {
    return m_transport->getClipState(h1) == PlaybackState::Playing &&
           m_transport->getClipState(h2) == PlaybackState::Playing &&
           m_transport->getClipState(h3) == PlaybackState::Playing;
  }));
  EXPECT_EQ(m_transport_callback->getStartCount(), 3);

  m_driver->stop();
}

TEST_F(TransportIntegrationTest, StopAllClipsWorks) {
  // Start driver
  ASSERT_EQ(m_driver->start(m_adapter.get()), SessionGraphError::OK);

  // Start multiple clips
  ASSERT_EQ(m_transport->startClip(1), SessionGraphError::OK);
  ASSERT_EQ(m_transport->startClip(2), SessionGraphError::OK);
  ASSERT_EQ(m_transport->startClip(3), SessionGraphError::OK);

  // Wait for clips to start (poll the published snapshot, per above).
  ASSERT_TRUE(waitForCallback([&] {
    return m_transport->getClipState(1) == PlaybackState::Playing &&
           m_transport->getClipState(2) == PlaybackState::Playing &&
           m_transport->getClipState(3) == PlaybackState::Playing;
  }));

  // Stop all clips
  ASSERT_EQ(m_transport->stopAllClips(), SessionGraphError::OK);

  // Verify all clips stopped (after fade-out).
  EXPECT_TRUE(waitForCallback([&] {
    return m_transport->getClipState(1) == PlaybackState::Stopped &&
           m_transport->getClipState(2) == PlaybackState::Stopped &&
           m_transport->getClipState(3) == PlaybackState::Stopped;
  }));
  EXPECT_EQ(m_transport_callback->getStopCount(), 3);

  m_driver->stop();
}
