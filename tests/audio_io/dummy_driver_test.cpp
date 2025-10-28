// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>
#include <orpheus/audio_driver.h>

#include <atomic>
#include <chrono>
#include <thread>

using namespace orpheus;

// Test callback that counts invocations
class TestCallback : public IAudioCallback {
public:
  void processAudio(const float** input_buffers, float** output_buffers, size_t num_channels,
                    size_t num_frames) override {
    m_call_count.fetch_add(1, std::memory_order_relaxed);
    m_last_num_channels = num_channels;
    m_last_num_frames = num_frames;

    // Fill output with a simple pattern for verification
    for (size_t ch = 0; ch < num_channels; ++ch) {
      for (size_t i = 0; i < num_frames; ++i) {
        output_buffers[ch][i] = 0.5f; // Simple constant
      }
    }
  }

  int getCallCount() const {
    return m_call_count.load(std::memory_order_relaxed);
  }

  void resetCallCount() {
    m_call_count.store(0, std::memory_order_relaxed);
  }

  size_t getLastNumChannels() const {
    return m_last_num_channels;
  }
  size_t getLastNumFrames() const {
    return m_last_num_frames;
  }

private:
  std::atomic<int> m_call_count{0};
  size_t m_last_num_channels{0};
  size_t m_last_num_frames{0};
};

// Test fixture
class DummyDriverTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_driver = createDummyAudioDriver();
    m_callback = std::make_unique<TestCallback>();
  }

  void TearDown() override {
    m_driver->stop();
    m_driver.reset();
    m_callback.reset();
  }

  std::unique_ptr<IAudioDriver> m_driver;
  std::unique_ptr<TestCallback> m_callback;
};

// Basic Tests

TEST_F(DummyDriverTest, InitialState) {
  // Initially, driver should not be running
  EXPECT_FALSE(m_driver->isRunning());
  EXPECT_EQ(m_driver->getDriverName(), "Dummy");
}

TEST_F(DummyDriverTest, InitializeWithValidConfig) {
  AudioDriverConfig config;
  config.sample_rate = 48000;
  config.buffer_size = 512;
  config.num_outputs = 2;

  auto error = m_driver->initialize(config);
  EXPECT_EQ(error, SessionGraphError::OK);
  EXPECT_EQ(m_driver->getConfig().sample_rate, 48000u);
  EXPECT_EQ(m_driver->getConfig().buffer_size, 512u);
  EXPECT_EQ(m_driver->getConfig().num_outputs, 2u);
}

TEST_F(DummyDriverTest, InitializeRejectsInvalidConfig) {
  AudioDriverConfig config;
  config.sample_rate = 0; // Invalid
  config.buffer_size = 512;

  auto error = m_driver->initialize(config);
  EXPECT_EQ(error, SessionGraphError::InvalidParameter);
}

TEST_F(DummyDriverTest, StartWithoutInitialize) {
  auto error = m_driver->start(m_callback.get());
  EXPECT_EQ(error, SessionGraphError::NotReady);
}

TEST_F(DummyDriverTest, StartWithNullCallback) {
  AudioDriverConfig config;
  config.sample_rate = 48000;
  config.buffer_size = 512;
  m_driver->initialize(config);

  auto error = m_driver->start(nullptr);
  EXPECT_EQ(error, SessionGraphError::InvalidParameter);
}

TEST_F(DummyDriverTest, StartAndStop) {
  AudioDriverConfig config;
  config.sample_rate = 48000;
  config.buffer_size = 512;
  config.num_outputs = 2;

  ASSERT_EQ(m_driver->initialize(config), SessionGraphError::OK);

  auto error = m_driver->start(m_callback.get());
  EXPECT_EQ(error, SessionGraphError::OK);
  EXPECT_TRUE(m_driver->isRunning());

  error = m_driver->stop();
  EXPECT_EQ(error, SessionGraphError::OK);
  EXPECT_FALSE(m_driver->isRunning());
}

TEST_F(DummyDriverTest, CallbackIsInvoked) {
  AudioDriverConfig config;
  config.sample_rate = 48000;
  config.buffer_size = 512;
  config.num_outputs = 2;

  ASSERT_EQ(m_driver->initialize(config), SessionGraphError::OK);
  ASSERT_EQ(m_driver->start(m_callback.get()), SessionGraphError::OK);

  // Wait for a few callbacks
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  // Should have been called at least once
  EXPECT_GT(m_callback->getCallCount(), 0);

  // Verify callback parameters
  EXPECT_EQ(m_callback->getLastNumChannels(), config.num_outputs);
  EXPECT_EQ(m_callback->getLastNumFrames(), config.buffer_size);

  m_driver->stop();
}

TEST_F(DummyDriverTest, CallbackIsNotInvokedAfterStop) {
  AudioDriverConfig config;
  config.sample_rate = 48000;
  config.buffer_size = 512;
  config.num_outputs = 2;

  ASSERT_EQ(m_driver->initialize(config), SessionGraphError::OK);
  ASSERT_EQ(m_driver->start(m_callback.get()), SessionGraphError::OK);

  // Wait for callbacks
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  int count_while_running = m_callback->getCallCount();
  EXPECT_GT(count_while_running, 0);

  // Stop and reset count
  m_driver->stop();
  m_callback->resetCallCount();

  // Wait and verify no new callbacks
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  EXPECT_EQ(m_callback->getCallCount(), 0);
}

TEST_F(DummyDriverTest, GetLatency) {
  AudioDriverConfig config;
  config.sample_rate = 48000;
  config.buffer_size = 512;

  ASSERT_EQ(m_driver->initialize(config), SessionGraphError::OK);

  // Dummy driver reports buffer size as latency
  EXPECT_EQ(m_driver->getLatencySamples(), config.buffer_size);
}

TEST_F(DummyDriverTest, CannotStartTwice) {
  AudioDriverConfig config;
  config.sample_rate = 48000;
  config.buffer_size = 512;

  ASSERT_EQ(m_driver->initialize(config), SessionGraphError::OK);
  ASSERT_EQ(m_driver->start(m_callback.get()), SessionGraphError::OK);

  // Try to start again
  auto error = m_driver->start(m_callback.get());
  EXPECT_EQ(error, SessionGraphError::InternalError);

  m_driver->stop();
}

TEST_F(DummyDriverTest, StopWhenNotRunning) {
  // Should not crash
  auto error = m_driver->stop();
  EXPECT_EQ(error, SessionGraphError::OK);
}
