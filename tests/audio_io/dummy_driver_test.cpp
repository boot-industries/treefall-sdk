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
  void processAudio(const AudioProcessBlock& block) noexcept override {
    auto output_buffers = block.output_buffers;
    const size_t num_channels = block.num_output_channels;
    const size_t num_frames = block.num_frames;
    m_call_count.fetch_add(1, std::memory_order_relaxed);
    m_last_num_channels.store(num_channels, std::memory_order_relaxed);
    m_last_num_frames.store(num_frames, std::memory_order_relaxed);

    // Fill output with a simple pattern for verification
    for (size_t ch = 0; ch < num_channels; ++ch) {
      for (size_t i = 0; i < num_frames; ++i) {
        output_buffers[ch][i] = 0.5f; // Simple constant
      }
    }
    bool distinct = true;
    bool patternVerified = num_frames > 0;
    for (size_t ch = 0; ch < num_channels; ++ch) {
      for (size_t other = 0; other < ch; ++other) {
        distinct = distinct && output_buffers[ch] != output_buffers[other];
      }
      if (num_frames > 0) {
        output_buffers[ch][0] = static_cast<float>(ch + 1) / 16.0f;
        patternVerified =
            patternVerified && output_buffers[ch][0] == static_cast<float>(ch + 1) / 16.0f;
      }
    }
    m_distinct_output_channels.store(distinct, std::memory_order_relaxed);
    m_channel_pattern_verified.store(patternVerified, std::memory_order_relaxed);
  }

  int getCallCount() const {
    return m_call_count.load(std::memory_order_relaxed);
  }

  void resetCallCount() {
    m_call_count.store(0, std::memory_order_relaxed);
  }

  size_t getLastNumChannels() const {
    return m_last_num_channels.load(std::memory_order_relaxed);
  }
  size_t getLastNumFrames() const {
    return m_last_num_frames.load(std::memory_order_relaxed);
  }
  bool hasDistinctOutputChannels() const {
    return m_distinct_output_channels.load(std::memory_order_relaxed);
  }
  bool isChannelPatternVerified() const {
    return m_channel_pattern_verified.load(std::memory_order_relaxed);
  }

private:
  std::atomic<int> m_call_count{0};
  std::atomic<size_t> m_last_num_channels{0};
  std::atomic<size_t> m_last_num_frames{0};
  std::atomic<bool> m_distinct_output_channels{false};
  std::atomic<bool> m_channel_pattern_verified{false};
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

TEST_F(DummyDriverTest, ReportsCapabilities) {
  AudioDriverConfig config;
  config.sample_rate = 96000;
  config.buffer_size = 128;
  config.num_inputs = 1;
  config.num_outputs = 8;

  ASSERT_EQ(m_driver->initialize(config), SessionGraphError::OK);

  const auto caps = m_driver->getCapabilities();
  EXPECT_EQ(caps.backend, AudioBackend::Dummy);
  EXPECT_EQ(caps.native_sample_rates.size(), 1u);
  EXPECT_EQ(caps.native_sample_rates.front(), 96000u);
  EXPECT_EQ(caps.native_buffer_sizes.front(), 128u);
  EXPECT_TRUE(caps.supports_input);
  EXPECT_TRUE(caps.supports_multichannel_output);
  EXPECT_FALSE(caps.reports_hardware_latency);
}

TEST_F(DummyDriverTest, RetainsRequestedChannelMapWithoutFabricatingPhysicalRoute) {
  AudioDriverConfig config;
  config.sample_rate = 48000;
  config.buffer_size = 128;
  config.num_outputs = 3;
  config.channel_map.output_channels = {2, 0, 1};

  ASSERT_EQ(m_driver->initialize(config), SessionGraphError::OK);
  EXPECT_EQ(m_driver->getConfig().channel_map.output_channels, config.channel_map.output_channels);

  const auto active_route = m_driver->getActiveRoute();
  EXPECT_TRUE(active_route.input_device_id.empty());
  EXPECT_TRUE(active_route.output_device_id.empty());
  EXPECT_TRUE(active_route.input_channels.empty());
  EXPECT_TRUE(active_route.output_channels.empty());
  EXPECT_FALSE(active_route.input_alive);
  EXPECT_FALSE(active_route.output_alive);
  EXPECT_FALSE(active_route.latency.complete);
}

TEST_F(DummyDriverTest, OutputRouteRequestPreservesMapAndLifecycleState) {
  AudioOutputRouteRequest request;
  request.output_device_id = "dummy";
  request.output_channel_map = {2, 0};
  request.requested_sample_rate = 48000;
  request.requested_buffer_size = 256;

  ASSERT_EQ(m_driver->initializeAudioOutput(request), SessionGraphError::OK);
  auto state = m_driver->getAudioIoRouteState();
  EXPECT_EQ(state.state, AudioRouteState::Inactive);
  EXPECT_EQ(state.selected_output_device_id, "dummy");
  EXPECT_EQ(state.active_output_channel_map, request.output_channel_map);
  EXPECT_EQ(state.requested_sample_rate, 48000u);
  EXPECT_EQ(state.actual_sample_rate, 48000u);
  EXPECT_EQ(state.requested_buffer_size, 256u);
  EXPECT_EQ(state.actual_buffer_size, 256u);

  ASSERT_EQ(m_driver->start(m_callback.get()), SessionGraphError::OK);
  state = m_driver->getAudioIoRouteState();
  EXPECT_EQ(state.state, AudioRouteState::Running);
  ASSERT_EQ(m_driver->stop(), SessionGraphError::OK);
  EXPECT_EQ(m_driver->getAudioIoRouteState().state, AudioRouteState::Inactive);
}

TEST_F(DummyDriverTest, OutputRouteRequestRejectsDuplicateMap) {
  AudioOutputRouteRequest request;
  request.output_channel_map = {1, 1};
  EXPECT_EQ(m_driver->initializeAudioOutput(request), SessionGraphError::InvalidParameter);
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

TEST_F(DummyDriverTest, EightOutputCallbackReceivesDistinctWritableLanes) {
  AudioDriverConfig config;
  config.sample_rate = 48000;
  config.buffer_size = 64;
  config.num_outputs = 8;

  ASSERT_EQ(m_driver->initialize(config), SessionGraphError::OK);
  ASSERT_EQ(m_driver->start(m_callback.get()), SessionGraphError::OK);
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  ASSERT_EQ(m_driver->stop(), SessionGraphError::OK);

  EXPECT_GT(m_callback->getCallCount(), 0);
  EXPECT_EQ(m_callback->getLastNumChannels(), 8u);
  EXPECT_EQ(m_callback->getLastNumFrames(), 64u);
  EXPECT_TRUE(m_callback->hasDistinctOutputChannels());
  EXPECT_TRUE(m_callback->isChannelPatternVerified());
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

TEST_F(DummyDriverTest, TelemetryDefaultsToZero) {
  EXPECT_EQ(m_driver->getTelemetry().input_render_failures, 0u);
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

TEST_F(DummyDriverTest, ProbeValidConfigUsesProbeOnlyDummyIdentity) {
  AudioDriverConfig config;
  config.sample_rate = 48000;
  config.buffer_size = 256;
  config.num_inputs = 1;
  config.num_outputs = 2;
  config.input_device_id = "dummy";
  config.output_device_id = "dummy";

  const auto compatibility = m_driver->probeRoute(config);

  EXPECT_EQ(compatibility.status, AudioRouteCompatibilityStatus::Compatible);
  EXPECT_EQ(compatibility.resolved_input_device_id, "dummy");
  EXPECT_EQ(compatibility.resolved_output_device_id, "dummy");
  EXPECT_EQ(compatibility.current_input_sample_rate, 48000u);
  EXPECT_EQ(compatibility.current_output_sample_rate, 48000u);
  EXPECT_FALSE(compatibility.input_rate_change_required);
  EXPECT_FALSE(compatibility.output_rate_change_required);
  EXPECT_FALSE(compatibility.input_is_running_somewhere);
  EXPECT_FALSE(compatibility.output_is_running_somewhere);
  EXPECT_TRUE(compatibility.detail.empty());
}

TEST_F(DummyDriverTest, ProbeReportsUnavailableExplicitIdsWithoutFallback) {
  AudioDriverConfig config;
  config.sample_rate = 48000;
  config.buffer_size = 256;
  config.num_inputs = 1;
  config.num_outputs = 2;
  config.output_device_id = "not-dummy";
  auto compatibility = m_driver->probeRoute(config);
  EXPECT_EQ(compatibility.status, AudioRouteCompatibilityStatus::OutputUnavailable);
  EXPECT_EQ(compatibility.detail, "resolve:output");

  config.output_device_id.clear();
  config.input_device_id = "not-dummy";
  compatibility = m_driver->probeRoute(config);
  EXPECT_EQ(compatibility.status, AudioRouteCompatibilityStatus::InputUnavailable);
  EXPECT_EQ(compatibility.detail, "resolve:input");
}

TEST_F(DummyDriverTest, ProbeReportsInvalidMapsAndStaticPrecedence) {
  AudioDriverConfig config;
  config.sample_rate = 48000;
  config.buffer_size = 256;
  config.num_outputs = 2;
  config.channel_map.output_channels = {0, 0};
  auto compatibility = m_driver->probeRoute(config);
  EXPECT_EQ(compatibility.status, AudioRouteCompatibilityStatus::InvalidChannelMap);
  EXPECT_EQ(compatibility.detail, "map:output");

  config.channel_map.output_channels.clear();
  config.num_outputs = 0;
  config.sample_rate = 0;
  config.buffer_size = 0;
  compatibility = m_driver->probeRoute(config);
  EXPECT_EQ(compatibility.status, AudioRouteCompatibilityStatus::OutputUnavailable);

  config.num_outputs = 2;
  compatibility = m_driver->probeRoute(config);
  EXPECT_EQ(compatibility.status, AudioRouteCompatibilityStatus::BackendFailure);
  EXPECT_EQ(compatibility.detail, "config:output");
}

TEST_F(DummyDriverTest, ProbeDoesNotChangeDriverState) {
  AudioDriverConfig initialized;
  initialized.sample_rate = 48000;
  initialized.buffer_size = 128;
  initialized.num_inputs = 1;
  initialized.num_outputs = 2;
  initialized.channel_map.output_channels = {1, 0};
  ASSERT_EQ(m_driver->initialize(initialized), SessionGraphError::OK);

  const auto before_config = m_driver->getConfig();
  const auto before_route = m_driver->getActiveRoute();
  const auto before_state = m_driver->getAudioIoRouteState();

  AudioDriverConfig request = initialized;
  request.sample_rate = 96000;
  request.output_device_id = "dummy";
  request.input_device_id = "dummy";
  request.channel_map.output_channels = {0, 1};
  ASSERT_EQ(m_driver->probeRoute(request).status, AudioRouteCompatibilityStatus::Compatible);

  const auto& after_config = m_driver->getConfig();
  EXPECT_EQ(after_config.sample_rate, before_config.sample_rate);
  EXPECT_EQ(after_config.buffer_size, before_config.buffer_size);
  EXPECT_EQ(after_config.num_inputs, before_config.num_inputs);
  EXPECT_EQ(after_config.num_outputs, before_config.num_outputs);
  EXPECT_EQ(after_config.channel_map.output_channels, before_config.channel_map.output_channels);
  const auto after_route = m_driver->getActiveRoute();
  EXPECT_EQ(after_route.input_device_id, before_route.input_device_id);
  EXPECT_EQ(after_route.output_device_id, before_route.output_device_id);
  EXPECT_EQ(after_route.input_channels, before_route.input_channels);
  EXPECT_EQ(after_route.output_channels, before_route.output_channels);
  const auto after_state = m_driver->getAudioIoRouteState();
  EXPECT_EQ(after_state.state, before_state.state);
  EXPECT_EQ(after_state.selected_input_device_id, before_state.selected_input_device_id);
  EXPECT_EQ(after_state.selected_output_device_id, before_state.selected_output_device_id);
  EXPECT_EQ(after_state.active_input_channel_map, before_state.active_input_channel_map);
  EXPECT_EQ(after_state.active_output_channel_map, before_state.active_output_channel_map);
  EXPECT_EQ(after_state.actual_sample_rate, before_state.actual_sample_rate);
}