// SPDX-License-Identifier: MIT

#include <orpheus/transport_controller.h>

#include <type_traits>

namespace {

class LegacyTransportImplementation final : public orpheus::ITransportController {
public:
  orpheus::SessionGraphError startClip(orpheus::ClipHandle) override {
    return orpheus::SessionGraphError::OK;
  }
  orpheus::SessionGraphError stopClip(orpheus::ClipHandle) override {
    return orpheus::SessionGraphError::OK;
  }
  orpheus::SessionGraphError stopAllClips() override {
    return orpheus::SessionGraphError::OK;
  }
  orpheus::SessionGraphError panic() override {
    return orpheus::SessionGraphError::OK;
  }
  orpheus::SessionGraphError stopAllInGroup(uint8_t) override {
    return orpheus::SessionGraphError::NotSupported;
  }
  orpheus::SessionGraphError stopOtherClips(orpheus::ClipHandle) override {
    return orpheus::SessionGraphError::OK;
  }
  orpheus::SessionGraphError setMaxVoicesPerClip(uint32_t) override {
    return orpheus::SessionGraphError::OK;
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
    return orpheus::SessionGraphError::OK;
  }
  orpheus::SessionGraphError prepareClipAudio(orpheus::ClipHandle) override {
    return orpheus::SessionGraphError::OK;
  }
  orpheus::SessionGraphError unregisterClipAudio(orpheus::ClipHandle) override {
    return orpheus::SessionGraphError::OK;
  }
  orpheus::SessionGraphError updateClipTrimPoints(orpheus::ClipHandle, int64_t, int64_t) override {
    return orpheus::SessionGraphError::OK;
  }
  orpheus::SessionGraphError updateClipFades(orpheus::ClipHandle, double, double,
                                             orpheus::FadeCurve, orpheus::FadeCurve) override {
    return orpheus::SessionGraphError::OK;
  }
  orpheus::SessionGraphError getClipTrimPoints(orpheus::ClipHandle, int64_t&,
                                               int64_t&) const override {
    return orpheus::SessionGraphError::OK;
  }
  orpheus::SessionGraphError updateClipGain(orpheus::ClipHandle, float) override {
    return orpheus::SessionGraphError::OK;
  }
  orpheus::SessionGraphError setClipLoopMode(orpheus::ClipHandle, bool) override {
    return orpheus::SessionGraphError::OK;
  }
  int64_t getClipPosition(orpheus::ClipHandle) const override {
    return -1;
  }
  orpheus::SessionGraphError setClipStopOthersMode(orpheus::ClipHandle, bool) override {
    return orpheus::SessionGraphError::OK;
  }
  bool getClipStopOthersMode(orpheus::ClipHandle) const override {
    return false;
  }
  orpheus::SessionGraphError updateClipMetadata(orpheus::ClipHandle,
                                                const orpheus::ClipMetadata&) override {
    return orpheus::SessionGraphError::OK;
  }
  std::optional<orpheus::ClipMetadata> getClipMetadata(orpheus::ClipHandle) const override {
    return std::nullopt;
  }
  orpheus::SessionGraphError setGroupOutputBus(orpheus::RoutingGroupIndex,
                                               const orpheus::OutputBusRoute&) override {
    return orpheus::SessionGraphError::OK;
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
    return orpheus::SessionGraphError::OK;
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
    return orpheus::SessionGraphError::OK;
  }
  orpheus::SessionGraphError seekClip(orpheus::ClipHandle, int64_t) override {
    return orpheus::SessionGraphError::OK;
  }
  int addCuePoint(orpheus::ClipHandle, int64_t, const std::string&, uint32_t) override {
    return -1;
  }
  std::vector<orpheus::CuePoint> getCuePoints(orpheus::ClipHandle) const override {
    return {};
  }
  orpheus::SessionGraphError seekToCuePoint(orpheus::ClipHandle, uint32_t) override {
    return orpheus::SessionGraphError::NotSupported;
  }
  orpheus::SessionGraphError removeCuePoint(orpheus::ClipHandle, uint32_t) override {
    return orpheus::SessionGraphError::NotSupported;
  }
  orpheus::RealtimeTelemetry* getRealtimeTelemetry() noexcept override {
    return nullptr;
  }
  orpheus::TransportConfig getRenderConfig() const noexcept override {
    return {};
  }
  void processAudio(float* const*, size_t, size_t) noexcept override {}
  void processCallbacks() override {}

  // Deliberately no startClipWithGroupChoke() override: this models a custom
  // implementation written against the preceding public interface.
};

static_assert(!std::is_abstract_v<LegacyTransportImplementation>);

} // namespace

int main() {
  LegacyTransportImplementation transport;
  return transport.startClipWithGroupChoke(1) == orpheus::SessionGraphError::NotSupported ? 0 : 1;
}
