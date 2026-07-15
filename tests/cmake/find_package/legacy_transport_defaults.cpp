// SPDX-License-Identifier: MIT
#include <orpheus/transport_controller.h>

#include <type_traits>

namespace {

class LegacyTransportImplementation final : public orpheus::ITransportController {
public:
  orpheus::SessionGraphError startClip(orpheus::ClipHandle) override {
    return ok;
  }
  orpheus::SessionGraphError stopClip(orpheus::ClipHandle) override {
    return ok;
  }
  orpheus::SessionGraphError stopAllClips() override {
    return ok;
  }
  orpheus::SessionGraphError panic() override {
    return ok;
  }
  orpheus::SessionGraphError stopAllInGroup(uint8_t) override {
    return ok;
  }
  orpheus::SessionGraphError stopOtherClips(orpheus::ClipHandle) override {
    return ok;
  }
  orpheus::SessionGraphError setMaxVoicesPerClip(uint32_t) override {
    return ok;
  }
  uint32_t getMaxVoicesPerClip() const override {
    return 1;
  }
  orpheus::PlaybackState getClipState(orpheus::ClipHandle) const override {
    return orpheus::PlaybackState::Stopped;
  }
  bool isClipPlaying(orpheus::ClipHandle) const override {
    return false;
  }
  orpheus::TransportPosition getCurrentPosition() const override {
    return {};
  }
  orpheus::IRoutingMatrix* getRoutingMatrix() const override {
    return nullptr;
  }
  void setCallback(orpheus::ITransportCallback*) override {}
  orpheus::SessionGraphError registerClipAudio(orpheus::ClipHandle, const std::string&) override {
    return ok;
  }
  orpheus::SessionGraphError prepareClipAudio(orpheus::ClipHandle) override {
    return ok;
  }
  orpheus::SessionGraphError unregisterClipAudio(orpheus::ClipHandle) override {
    return ok;
  }
  orpheus::SessionGraphError updateClipTrimPoints(orpheus::ClipHandle, int64_t, int64_t) override {
    return ok;
  }
  orpheus::SessionGraphError updateClipFades(orpheus::ClipHandle, double, double,
                                             orpheus::FadeCurve, orpheus::FadeCurve) override {
    return ok;
  }
  orpheus::SessionGraphError getClipTrimPoints(orpheus::ClipHandle, int64_t&,
                                               int64_t&) const override {
    return ok;
  }
  orpheus::SessionGraphError updateClipGain(orpheus::ClipHandle, float) override {
    return ok;
  }
  orpheus::SessionGraphError setClipLoopMode(orpheus::ClipHandle, bool) override {
    return ok;
  }
  int64_t getClipPosition(orpheus::ClipHandle) const override {
    return -1;
  }
  orpheus::SessionGraphError setClipStopOthersMode(orpheus::ClipHandle, bool) override {
    return ok;
  }
  bool getClipStopOthersMode(orpheus::ClipHandle) const override {
    return false;
  }
  orpheus::SessionGraphError updateClipMetadata(orpheus::ClipHandle,
                                                const orpheus::ClipMetadata&) override {
    return ok;
  }
  std::optional<orpheus::ClipMetadata> getClipMetadata(orpheus::ClipHandle) const override {
    return std::nullopt;
  }
  orpheus::SessionGraphError setGroupOutputBus(orpheus::RoutingGroupIndex,
                                               const orpheus::OutputBusRoute&) override {
    return ok;
  }
  std::optional<orpheus::OutputBusRoute>
  getGroupOutputBus(orpheus::RoutingGroupIndex) const override {
    return std::nullopt;
  }
  void setSessionDefaults(const orpheus::SessionDefaults&) override {}
  orpheus::SessionDefaults getSessionDefaults() const override {
    return {};
  }
  bool isClipLooping(orpheus::ClipHandle) const override {
    return false;
  }
  orpheus::SessionGraphError setClipVoiceMode(orpheus::ClipHandle, orpheus::VoiceMode) override {
    return ok;
  }
  orpheus::VoiceMode getClipVoiceMode(orpheus::ClipHandle) const override {
    return orpheus::VoiceMode::Polyphonic;
  }
  size_t getActiveVoiceCount(orpheus::ClipHandle) const override {
    return 0;
  }
  size_t getTotalActiveVoiceCount() const override {
    return 0;
  }
  orpheus::SessionGraphError restartClip(orpheus::ClipHandle) override {
    return ok;
  }
  orpheus::SessionGraphError seekClip(orpheus::ClipHandle, int64_t) override {
    return ok;
  }
  int addCuePoint(orpheus::ClipHandle, int64_t, const std::string&, uint32_t) override {
    return 0;
  }
  std::vector<orpheus::CuePoint> getCuePoints(orpheus::ClipHandle) const override {
    return {};
  }
  orpheus::SessionGraphError seekToCuePoint(orpheus::ClipHandle, uint32_t) override {
    return ok;
  }
  orpheus::SessionGraphError removeCuePoint(orpheus::ClipHandle, uint32_t) override {
    return ok;
  }
  orpheus::RealtimeTelemetry* getRealtimeTelemetry() noexcept override {
    return nullptr;
  }
  orpheus::TransportConfig getRenderConfig() const noexcept override {
    return {};
  }
  void processAudio(float* const*, size_t, size_t) noexcept override {}
  void processCallbacks() override {}

private:
  static constexpr auto ok = orpheus::SessionGraphError::OK;
};

static_assert(!std::is_abstract_v<LegacyTransportImplementation>);

} // namespace

int main() {
  LegacyTransportImplementation legacy;
  const auto telemetry = legacy.getCallbackDeliveryTelemetry();
  const auto snapshot = legacy.getActiveVoiceSnapshot();
  return telemetry.lastAttemptedSequence == 0 && telemetry.cumulativeDroppedCount == 0 &&
                 telemetry.activeVoiceSnapshotSequence == 0 && snapshot.entryCount == 0 &&
                 snapshot.totalActiveVoiceCount == 0
             ? 0
             : 1;
}
