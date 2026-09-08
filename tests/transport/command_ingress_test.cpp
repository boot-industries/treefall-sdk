// SPDX-License-Identifier: MIT
#include "../../src/core/transport/transport_controller.h"
#include <gtest/gtest.h>
#include <orpheus/audio_file_capabilities.h>
#include <orpheus/audio_file_writer.h>

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace orpheus {
class TransportControllerTestAccess {
public:
  using Hook = void (*)(void*, uint32_t, uint32_t, bool) noexcept;
  static void hook(TransportController& t, Hook callback, void* state) {
    t.m_commandPublicationTestHook = callback;
    t.m_commandPublicationTestState = state;
  }
  static std::shared_ptr<StreamingClipSource> source(TransportController& t, ClipHandle handle) {
    std::lock_guard lock(t.m_audioFilesMutex);
    return std::dynamic_pointer_cast<StreamingClipSource>(t.m_audioFiles.at(handle).source);
  }
  static void seed(TransportController& t, uint64_t count) {
    t.m_ingressAttempted.store(count);
  }
  static uint64_t raw(TransportController& t) {
    return t.m_ingressAttempted.load();
  }
  static void increment(TransportController& t) {
    t.incrementIngressCounter(t.m_ingressAttempted);
  }
  static void excess(TransportController& t) {
    t.m_ingressAttempted.fetch_add(1);
  }
  static void compensate(TransportController& t) {
    t.m_ingressAttempted.fetch_sub(1);
  }
  static void preparationException(TransportController& t) {
    const auto index = t.acquireCommandNode();
    if (index == UINT32_MAX)
      throw std::logic_error("test needs a free node");
    TransportController::CommandReservation reservation(t, index);
    throw std::runtime_error("interrupted preparation");
  }
  static void retainPayload(TransportController& t, std::shared_ptr<ClipPlaybackContext> context) {
    const auto index = t.acquireCommandNode();
    if (index == UINT32_MAX)
      throw std::logic_error("test needs a free node");
    TransportController::CommandReservation reservation(t, index);
    auto& command = t.m_commandNodes[index].command;
    command.type = TransportCommand::Type::Stop;
    command.handle = 999;
    command.startContext = std::move(context);
    if (reservation.publish() != SessionGraphError::OK)
      throw std::logic_error("test publication failed");
  }
};
} // namespace orpheus

namespace {
using namespace orpheus;
using Access = TransportControllerTestAccess;
constexpr uint32_t kCapacity = kTransportCommandIngressCapacity;
std::unique_ptr<TransportController> makeTransport() {
  return std::make_unique<TransportController>(
      nullptr, TransportConfig{.sampleRate = 48000, .outputChannels = 2, .maxBlockFrames = 64});
}
void drain(TransportController& transport) {
  std::array<float, 64> left{}, right{};
  float* output[]{left.data(), right.data()};
  transport.processAudio(output, 2, 64);
}
void checkPartition(const TransportController& transport) {
  const auto t = transport.getCommandIngressTelemetry();
  EXPECT_EQ(t.attemptedCount, t.admittedCount + t.slotUnavailableCount +
                                  t.publicationContentionCount + t.preparationRejectedCount);
}

// Setup/teardown of hook fields is externally serialized. Only the hook's
// own state is shared while threads run; callbacks never run on the consumer.
struct Pause {
  std::mutex mutex;
  std::condition_variable condition;
  bool afterPublication{false};
  bool reached{false};
  bool release{false};
  uint32_t index{UINT32_MAX};
  uint32_t reusedIndex{UINT32_MAX};
};
void pausePublication(void* opaque, uint32_t index, uint32_t, bool published) noexcept {
  auto& state = *static_cast<Pause*>(opaque);
  std::unique_lock lock(state.mutex);
  if (state.reached) {
    if (published)
      state.reusedIndex = index;
    return;
  }
  if (published != state.afterPublication)
    return;
  state.index = index;
  state.reached = true;
  state.condition.notify_all();
  state.condition.wait(lock, [&] { return state.release; });
}
bool awaitPause(Pause& state) {
  std::unique_lock lock(state.mutex);
  return state.condition.wait_for(lock, std::chrono::seconds(10), [&] { return state.reached; });
}
void releasePause(Pause& state) {
  std::lock_guard lock(state.mutex);
  state.release = true;
  state.condition.notify_all();
}

struct Contention {
  TransportController* transport;
  uint32_t target{UINT32_MAX};
  uint32_t attempts{0};
  bool peerFailed{false};
  bool consumeOlder{false};
};
void forceContention(void* opaque, uint32_t index, uint32_t, bool published) noexcept {
  auto& state = *static_cast<Contention*>(opaque);
  if (published)
    return;
  if (state.target == UINT32_MAX)
    state.target = index;
  if (index != state.target)
    return; // Recursive competitor is a different owned node.
  if (state.consumeOlder && state.attempts == 0) {
    drain(*state.transport);
    // Reusing only the old head is benign ABA and permits publication.
    // Push an additional competitor so the compared head really changes.
    state.peerFailed |= state.transport->stopClip(999) != SessionGraphError::OK;
  }
  ++state.attempts;
  state.peerFailed |= state.transport->stopClip(999) != SessionGraphError::OK;
}

TEST(CommandIngressTest, CapacityRecoveryAndEligibilityPrecedence) {
  auto transport = makeTransport();
  for (uint32_t i = 0; i < kCapacity; ++i)
    ASSERT_EQ(transport->stopClip(1), SessionGraphError::OK);
  EXPECT_EQ(transport->stopClip(1), SessionGraphError::NotReady);
  const auto full = transport->getCommandIngressTelemetry();
  EXPECT_EQ(full.attemptedCount, 256u);
  EXPECT_EQ(full.admittedCount, 255u);
  EXPECT_EQ(full.slotUnavailableCount, 1u);
  EXPECT_EQ(transport->stopClip(0), SessionGraphError::InvalidHandle);
  EXPECT_EQ(transport->startClipWithGroupChoke(88, 99), SessionGraphError::ClipNotRegistered);
  EXPECT_EQ(transport->updateClipGain(88, -6), SessionGraphError::ClipNotRegistered);
  EXPECT_EQ(transport->getCommandIngressTelemetry().attemptedCount, full.attemptedCount);
  drain(*transport);
  EXPECT_EQ(transport->getCommandIngressTelemetry().processedCount, 255u);
  EXPECT_EQ(transport->startClip(1, 99), SessionGraphError::OK);
  drain(*transport);
  EXPECT_EQ(transport->getStartSettlementSnapshot().entries[0].requestTag, 99u);
  checkPartition(*transport);
}

TEST(CommandIngressTest, DetachedBatchPreservesEachProducersSettlementOrder) {
  auto transport = makeTransport();
  std::array<std::thread, 8> producers;
  std::array<std::array<SessionGraphError, 4>, 8> results{};
  for (size_t p = 0; p < producers.size(); ++p)
    producers[p] = std::thread([&, p] {
      for (size_t i = 0; i < 4; ++i)
        results[p][i] = transport->startClip(p + 1, (uint64_t(p + 1) << 32) | (i + 1));
    });
  for (auto& producer : producers)
    producer.join();
  for (const auto& row : results)
    for (auto result : row)
      ASSERT_EQ(result, SessionGraphError::OK);
  drain(*transport);
  const auto snapshot = transport->getStartSettlementSnapshot();
  ASSERT_EQ(snapshot.entryCount, 32u);
  std::array<uint32_t, 8> ordinal{};
  for (uint32_t i = 0; i < snapshot.entryCount; ++i) {
    const auto& entry = snapshot.entries[i];
    const size_t p = static_cast<size_t>(entry.requestTag >> 32) - 1;
    ASSERT_LT(p, ordinal.size());
    EXPECT_EQ(static_cast<uint32_t>(entry.requestTag), ++ordinal[p]);
    EXPECT_EQ(entry.sequence, i + 1);
  }
  for (auto count : ordinal)
    EXPECT_EQ(count, 4u);
}

TEST(CommandIngressTest, PausedUnpublishedProducerAllowsHeadReuseWithoutFifoHole) {
  auto transport = makeTransport();
  ASSERT_EQ(transport->startClip(1, 111), SessionGraphError::OK); // head H
  Pause state;
  Access::hook(*transport, pausePublication, &state);
  SessionGraphError result = SessionGraphError::InternalError;
  std::thread publisher([&] { result = transport->startClip(2, 333); });
  const bool paused = awaitPause(state);
  if (paused) {
    drain(*transport); // Free H while the unpublished producer still remembers it.
    EXPECT_EQ(transport->panic(), SessionGraphError::OK); // Reuse H as the new head.
  }
  releasePause(state);
  publisher.join();
  Access::hook(*transport, nullptr, nullptr);
  ASSERT_TRUE(paused);
  ASSERT_EQ(result, SessionGraphError::OK);
  drain(*transport); // FIFO must panic first, then start handle 2.
  const auto snapshot = transport->getActiveVoiceSnapshot();
  ASSERT_EQ(snapshot.totalActiveVoiceCount, 1u);
  EXPECT_EQ(snapshot.entries[0].handle, 2u);
  EXPECT_EQ(snapshot.entries[0].newestStartRequestTag, 333u);
  checkPartition(*transport);
}

TEST(CommandIngressTest, PublishedNodeCanBeConsumedAndReusedBeforePublisherReturns) {
  auto transport = makeTransport();
  Pause state;
  state.afterPublication = true;
  Access::hook(*transport, pausePublication, &state);
  SessionGraphError result = SessionGraphError::InternalError;
  std::thread publisher([&] { result = transport->startClip(1, 41); });
  const bool paused = awaitPause(state);
  if (paused) {
    drain(*transport);
    EXPECT_EQ(transport->getCommandIngressTelemetry().processedCount, 1u);
    EXPECT_EQ(transport->stopClip(999), SessionGraphError::OK);
    drain(*transport);
  }
  releasePause(state);
  publisher.join();
  Access::hook(*transport, nullptr, nullptr);
  ASSERT_TRUE(paused);
  EXPECT_EQ(state.reusedIndex, state.index);
  EXPECT_EQ(result, SessionGraphError::OK);
  const auto telemetry = transport->getCommandIngressTelemetry();
  EXPECT_EQ(telemetry.admittedCount, 2u);
  EXPECT_EQ(telemetry.processedCount, 2u);
  EXPECT_EQ(transport->getStartSettlementSnapshot().latestSequence, 1u);
  checkPartition(*transport);
}

TEST(CommandIngressTest, RetainedPayloadIsDestroyedOnlyByProducerReuse) {
  auto transport = makeTransport();
  struct Destruction {
    bool rendering{false};
    bool onAudio{false};
    unsigned count{0};
  };
  auto state = std::make_shared<Destruction>();
  auto context =
      std::shared_ptr<ClipPlaybackContext>(new ClipPlaybackContext{}, [state](auto* value) {
        state->onAudio |= state->rendering;
        ++state->count;
        delete value;
      });
  Access::retainPayload(*transport, std::move(context));
  state->rendering = true;
  drain(*transport);
  state->rendering = false;
  EXPECT_EQ(state->count, 0u);
  ASSERT_EQ(transport->stopClip(999), SessionGraphError::OK);
  EXPECT_EQ(state->count, 1u);
  EXPECT_FALSE(state->onAudio);
}

TEST(CommandIngressTest, ExceptionalPreparationCancelsAndCountsExactlyOnce) {
  auto transport = makeTransport();
  EXPECT_THROW(Access::preparationException(*transport), std::runtime_error);
  const auto rejected = transport->getCommandIngressTelemetry();
  EXPECT_EQ(rejected.attemptedCount, 1u);
  EXPECT_EQ(rejected.preparationRejectedCount, 1u);
  for (uint32_t i = 0; i < kCapacity; ++i)
    ASSERT_EQ(transport->stopClip(1), SessionGraphError::OK);
  checkPartition(*transport);
}

TEST(CommandIngressTest, ConcurrentSaturationSurvivesSuspendedCompensation) {
  auto transport = makeTransport();
  Access::seed(*transport, UINT32_MAX - 2u);
  std::array<std::thread, 8> producers;
  const auto incrementConcurrently = [&] {
    for (auto& producer : producers)
      producer = std::thread([&] {
        for (size_t i = 0; i < 1000; ++i)
          Access::increment(*transport);
      });
    for (auto& producer : producers)
      producer.join();
  };
  incrementConcurrently();
  ASSERT_EQ(Access::raw(*transport), UINT32_MAX);
  Access::excess(*transport); // A real fetch_add paused before its compensating fetch_sub.
  incrementConcurrently();
  EXPECT_EQ(transport->getCommandIngressTelemetry().attemptedCount, UINT32_MAX);
  EXPECT_EQ(Access::raw(*transport), uint64_t(UINT32_MAX) + 1);
  Access::compensate(*transport);
  EXPECT_EQ(Access::raw(*transport), UINT32_MAX);
}

class RegisteredIngressTest : public ::testing::Test {
protected:
  void SetUp() override {
    if (!getAudioFileCapabilities().file_io_available)
      GTEST_SKIP() << "real file provider required";
    directory = std::filesystem::temp_directory_path() /
                ("command-ingress-" +
                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ASSERT_TRUE(std::filesystem::create_directory(directory));
    const auto path = directory / "source.wav";
    auto writer = createAudioFileWriter();
    ASSERT_NE(writer, nullptr);
    ASSERT_EQ(
        writer->open(path.string(), {AudioFileFormat::WAV, 48000, 2, AudioSampleFormat::Int16}),
        SessionGraphError::OK);
    std::array<float, 2048> samples;
    samples.fill(0.25f);
    constexpr size_t frames = 3 * StreamingClipSource::kPageFrames + 1024;
    for (size_t written = 0; written < frames; written += 1024) {
      const auto result = writer->writeSamples(samples.data(), 1024);
      ASSERT_EQ(result.error, SessionGraphError::OK);
      ASSERT_EQ(result.value, 1024u);
    }
    ASSERT_EQ(writer->close(), SessionGraphError::OK);
    transport = std::make_unique<TransportController>(
        nullptr, TransportConfig{.sampleRate = 48000, .outputChannels = 2, .maxBlockFrames = 64});
    transport->setPreparedSourceMaxFrames(1);
    for (ClipHandle handle : {ClipHandle{1}, ClipHandle{2}}) {
      ASSERT_EQ(transport->registerClipAudio(handle, path.string()), SessionGraphError::OK);
      ASSERT_EQ(transport->prepareClipAudio(handle), SessionGraphError::OK);
    }
  }
  void TearDown() override {
    transport.reset();
    if (!directory.empty()) {
      std::error_code error;
      std::filesystem::remove_all(directory, error);
    }
  }
  template <class Operation>
  void refusePublication(Operation operation, bool consumeOlder = false) {
    Contention state{transport.get()};
    state.consumeOlder = consumeOlder;
    Access::hook(*transport, forceContention, &state);
    const auto result = operation();
    Access::hook(*transport, nullptr, nullptr);
    EXPECT_EQ(result, SessionGraphError::NotReady);
    EXPECT_EQ(state.attempts, 32u);
    EXPECT_FALSE(state.peerFailed);
    drain(*transport);
    checkPartition(*transport);
  }
  std::filesystem::path directory;
  std::unique_ptr<TransportController> transport;
};

TEST_F(RegisteredIngressTest, BoundedContentionPreservesGainFadesChokeAndSourceLeases) {
  ASSERT_EQ(transport->startClip(1, 7), SessionGraphError::OK);
  drain(*transport);
  const auto before = transport->getClipMetadata(2);
  ASSERT_TRUE(before.has_value());
  refusePublication([&] { return transport->updateClipGain(2, -12); });
  refusePublication([&] {
    return transport->updateClipFades(2, 0.1, 0.2, FadeCurve::Linear, FadeCurve::EqualPower);
  });
  const auto settlementSequence = transport->getStartSettlementSnapshot().latestSequence;
  refusePublication([&] { return transport->startClipWithGroupChoke(2, 99); });
  const auto after = transport->getClipMetadata(2);
  ASSERT_TRUE(after.has_value());
  EXPECT_EQ(after->gainDb, before->gainDb);
  EXPECT_EQ(after->fadeInSeconds, before->fadeInSeconds);
  EXPECT_EQ(after->fadeOutSeconds, before->fadeOutSeconds);
  EXPECT_EQ(transport->getStartSettlementSnapshot().latestSequence, settlementSequence);
  EXPECT_EQ(transport->getActiveVoiceCount(1), 1u);
  EXPECT_EQ(transport->getActiveVoiceCount(2), 0u);
  EXPECT_FALSE(Access::source(*transport, 2)->hasPendingCommandPrimes());
  EXPECT_EQ(transport->unregisterClipAudio(2), SessionGraphError::OK);
  const auto telemetry = transport->getCommandIngressTelemetry();
  EXPECT_EQ(telemetry.publicationContentionCount, 3u);
  EXPECT_EQ(telemetry.slotUnavailableCount, 0u);
  EXPECT_EQ(telemetry.preparationRejectedCount, 0u);
}

TEST_F(RegisteredIngressTest, FullQueueRefusalDoesNotPersistGainOrFadeChanges) {
  const auto before = transport->getClipMetadata(1);
  ASSERT_TRUE(before.has_value());
  for (uint32_t i = 0; i < kCapacity; ++i)
    ASSERT_EQ(transport->stopClip(999), SessionGraphError::OK);
  EXPECT_EQ(transport->updateClipGain(1, -12), SessionGraphError::NotReady);
  EXPECT_EQ(transport->updateClipFades(1, 0.1, 0.2, FadeCurve::Linear, FadeCurve::EqualPower),
            SessionGraphError::NotReady);
  const auto after = transport->getClipMetadata(1);
  ASSERT_TRUE(after.has_value());
  EXPECT_EQ(after->gainDb, before->gainDb);
  EXPECT_EQ(after->fadeInSeconds, before->fadeInSeconds);
  EXPECT_EQ(after->fadeOutSeconds, before->fadeOutSeconds);
  EXPECT_EQ(after->stopFadeOutSeconds, before->stopFadeOutSeconds);
  EXPECT_EQ(transport->getCommandIngressTelemetry().slotUnavailableCount, 2u);
  drain(*transport);
  checkPartition(*transport);
}

TEST_F(RegisteredIngressTest, MetadataCommitSurvivesReusedNodeAndOrdersConcurrentStart) {
  const auto before = transport->getClipMetadata(1);
  ASSERT_TRUE(before.has_value());
  auto metadata = *before;
  metadata.trimInSamples = 480;
  metadata.trimOutSamples = 24000;
  metadata.gainDb = -12;
  Pause state;
  state.afterPublication = true;
  Access::hook(*transport, pausePublication, &state);
  SessionGraphError metadataResult = SessionGraphError::InternalError;
  SessionGraphError startResult = SessionGraphError::InternalError;
  std::thread publisher([&] { metadataResult = transport->updateClipMetadata(1, metadata); });
  const bool paused = awaitPause(state);
  std::thread starter;
  if (paused) {
    starter = std::thread([&] { startResult = transport->startClip(1, 42); });
    drain(*transport);
    EXPECT_EQ(transport->stopClip(999), SessionGraphError::OK); // Reuse published metadata node.
    drain(*transport);
  }
  releasePause(state);
  publisher.join();
  if (starter.joinable())
    starter.join();
  Access::hook(*transport, nullptr, nullptr);
  ASSERT_TRUE(paused);
  ASSERT_EQ(metadataResult, SessionGraphError::OK);
  ASSERT_EQ(startResult, SessionGraphError::OK);
  drain(*transport);
  const auto persisted = transport->getClipMetadata(1);
  ASSERT_TRUE(persisted.has_value());
  EXPECT_EQ(persisted->gainDb, metadata.gainDb);
  const auto active = transport->getActiveVoiceSnapshot();
  ASSERT_EQ(active.entryCount, 1u);
  EXPECT_EQ(active.entries[0].newestTrimInSamples, metadata.trimInSamples);
  EXPECT_EQ(active.entries[0].newestTrimOutSamples, metadata.trimOutSamples);
  EXPECT_EQ(active.entries[0].newestStartRequestTag, 42u);
  checkPartition(*transport);
}

TEST_F(RegisteredIngressTest, PendingTrimControlsFadeValidationAndNextStartSnapshot) {
  ASSERT_EQ(transport->updateClipTrimPoints(1, 480, 960), SessionGraphError::OK);
  const auto before = transport->getCommandIngressTelemetry();
  EXPECT_EQ(transport->updateClipFades(1, 1, 1, FadeCurve::Linear, FadeCurve::Linear),
            SessionGraphError::InvalidFadeDuration);
  EXPECT_EQ(transport->getCommandIngressTelemetry().attemptedCount, before.attemptedCount);
  ASSERT_EQ(transport->startClip(1, 42), SessionGraphError::OK);
  drain(*transport);
  const auto active = transport->getActiveVoiceSnapshot();
  ASSERT_EQ(active.entryCount, 1u);
  EXPECT_EQ(active.entries[0].newestTrimInSamples, 480);
  EXPECT_EQ(active.entries[0].newestTrimOutSamples, 960);
  EXPECT_EQ(active.entries[0].newestStartRequestTag, 42u);
}

TEST_F(RegisteredIngressTest, RejectedLoopTransitionRollsBackAfterOlderCommandIsConsumed) {
  auto source = Access::source(*transport, 1);
  ASSERT_NE(source, nullptr);
  ASSERT_EQ(transport->setClipLoopMode(1, true), SessionGraphError::OK);
  const auto before = transport->getClipMetadata(1);
  ASSERT_TRUE(before.has_value());
  refusePublication(
      [&] {
        return transport->updateClipTrimPoints(1, 2 * StreamingClipSource::kPageFrames,
                                               before->trimOutSamples);
      },
      true);
  EXPECT_EQ(transport->getClipMetadata(1)->trimInSamples, before->trimInSamples);
  StreamingClipSource::LoopAnchorTransition inspection{};
  ASSERT_EQ(source->prepareLoopAnchorTransition(0, false, inspection), SessionGraphError::OK);
  EXPECT_EQ(inspection.previousStart, 0);
  source->rollbackLoopAnchorTransition(inspection);
  ASSERT_EQ(transport->setClipLoopMode(1, false), SessionGraphError::OK);
  drain(*transport);
  EXPECT_FALSE(source->hasPendingCommandPrimes());
  EXPECT_EQ(transport->unregisterClipAudio(1), SessionGraphError::OK);
}

TEST_F(RegisteredIngressTest, UnreadLoopTransitionsRollBackNewestFirstDuringTeardown) {
  auto source = Access::source(*transport, 1);
  ASSERT_NE(source, nullptr);
  const auto metadata = transport->getClipMetadata(1);
  ASSERT_TRUE(metadata.has_value());
  ASSERT_EQ(transport->setClipLoopMode(1, true), SessionGraphError::OK);
  ASSERT_EQ(transport->updateClipTrimPoints(1, StreamingClipSource::kPageFrames,
                                            metadata->trimOutSamples),
            SessionGraphError::OK);
  ASSERT_EQ(transport->updateClipTrimPoints(1, 2 * StreamingClipSource::kPageFrames,
                                            metadata->trimOutSamples),
            SessionGraphError::OK);
  transport.reset(); // No command has been consumed.
  EXPECT_FALSE(source->hasPendingCommandPrimes());
  StreamingClipSource::LoopAnchorTransition inspection{};
  ASSERT_EQ(source->prepareLoopAnchorTransition(0, false, inspection), SessionGraphError::OK);
  EXPECT_EQ(inspection.previousPageIndex,
            StreamingClipSource::LoopAnchorTransition::kInactivePageIndex);
  EXPECT_EQ(inspection.previousStart, -1);
  source->rollbackLoopAnchorTransition(inspection);
}
} // namespace

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
