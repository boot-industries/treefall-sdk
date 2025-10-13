// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>
#include <orpheus/audio_driver.h>
#include <orpheus/transport_controller.h>

// Access implementation for processAudio
#include "transport/transport_controller.h"

#include <atomic>
#include <chrono>
#include <thread>

using namespace orpheus;

// Adapter to connect TransportController to IAudioCallback
class TransportAudioAdapter : public IAudioCallback {
public:
    explicit TransportAudioAdapter(TransportController* transport)
        : m_transport(transport)
    {
    }

    void processAudio(
        const float** input_buffers,
        float** output_buffers,
        size_t num_channels,
        size_t num_frames) override
    {
        (void)input_buffers;  // Not used for playback

        if (m_transport) {
            m_transport->processAudio(output_buffers, num_channels, num_frames);
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
    void onClipStarted(ClipHandle handle, TransportPosition position) override {
        m_start_count.fetch_add(1, std::memory_order_relaxed);
        m_last_started_handle = handle;
        (void)position;
    }

    void onClipStopped(ClipHandle handle, TransportPosition position) override {
        m_stop_count.fetch_add(1, std::memory_order_relaxed);
        m_last_stopped_handle = handle;
        (void)position;
    }

    void onClipLooped(ClipHandle handle, TransportPosition position) override {
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

    ClipHandle getLastStartedHandle() const { return m_last_started_handle; }
    ClipHandle getLastStoppedHandle() const { return m_last_stopped_handle; }

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
        m_transport = std::make_unique<TransportController>(nullptr, 48000);

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

    std::unique_ptr<TransportController> m_transport;
    std::unique_ptr<TestTransportCallback> m_transport_callback;
    std::unique_ptr<TransportAudioAdapter> m_adapter;
    std::unique_ptr<IAudioDriver> m_driver;
};

// Integration Tests

TEST_F(TransportIntegrationTest, DriverCallsTransportProcessAudio) {
    // Start driver with transport adapter
    ASSERT_EQ(m_driver->start(m_adapter.get()), SessionGraphError::OK);

    // Wait for a few callbacks
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Verify transport's processAudio was called via adapter
    EXPECT_GT(m_adapter->getCallbackCount(), 0);

    m_driver->stop();
}

TEST_F(TransportIntegrationTest, StartClipTriggersCallback) {
    // Start driver
    ASSERT_EQ(m_driver->start(m_adapter.get()), SessionGraphError::OK);

    // Start a clip
    ClipHandle handle = 42;
    ASSERT_EQ(m_transport->startClip(handle), SessionGraphError::OK);

    // Wait for audio callbacks to process the command
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Process UI callbacks
    m_transport->processCallbacks();

    // Verify clip started callback was triggered
    EXPECT_EQ(m_transport_callback->getStartCount(), 1);
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
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    m_transport->processCallbacks();
    ASSERT_EQ(m_transport_callback->getStartCount(), 1);

    // Stop the clip
    ASSERT_EQ(m_transport->stopClip(handle), SessionGraphError::OK);

    // Wait for fade-out (10ms + margin)
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    m_transport->processCallbacks();

    // Verify clip stopped callback was triggered
    EXPECT_EQ(m_transport_callback->getStopCount(), 1);
    EXPECT_EQ(m_transport_callback->getLastStoppedHandle(), handle);

    m_driver->stop();
}

TEST_F(TransportIntegrationTest, TransportPositionAdvances) {
    // Start driver
    ASSERT_EQ(m_driver->start(m_adapter.get()), SessionGraphError::OK);

    // Get initial position
    auto pos1 = m_transport->getCurrentPosition();

    // Wait for audio processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

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

    // Wait for clips to start
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    m_transport->processCallbacks();

    // Verify all clips started
    EXPECT_EQ(m_transport_callback->getStartCount(), 3);
    EXPECT_EQ(m_transport->getClipState(h1), PlaybackState::Playing);
    EXPECT_EQ(m_transport->getClipState(h2), PlaybackState::Playing);
    EXPECT_EQ(m_transport->getClipState(h3), PlaybackState::Playing);

    m_driver->stop();
}

TEST_F(TransportIntegrationTest, StopAllClipsWorks) {
    // Start driver
    ASSERT_EQ(m_driver->start(m_adapter.get()), SessionGraphError::OK);

    // Start multiple clips
    ASSERT_EQ(m_transport->startClip(1), SessionGraphError::OK);
    ASSERT_EQ(m_transport->startClip(2), SessionGraphError::OK);
    ASSERT_EQ(m_transport->startClip(3), SessionGraphError::OK);

    // Wait for clips to start
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    m_transport->processCallbacks();
    ASSERT_EQ(m_transport_callback->getStartCount(), 3);

    // Stop all clips
    ASSERT_EQ(m_transport->stopAllClips(), SessionGraphError::OK);

    // Wait for fade-out
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    m_transport->processCallbacks();

    // Verify all clips stopped
    EXPECT_EQ(m_transport_callback->getStopCount(), 3);
    EXPECT_EQ(m_transport->getClipState(1), PlaybackState::Stopped);
    EXPECT_EQ(m_transport->getClipState(2), PlaybackState::Stopped);
    EXPECT_EQ(m_transport->getClipState(3), PlaybackState::Stopped);

    m_driver->stop();
}
