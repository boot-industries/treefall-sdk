// SPDX-License-Identifier: MIT
#pragma once

#include "coreaudio/coreaudio_property_api.h"

#include <CoreFoundation/CoreFoundation.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace orpheus::test_support {

struct CoreAudioPropertyKey {
  AudioObjectID object_id = 0;
  AudioObjectPropertySelector selector = 0;
  AudioObjectPropertyScope scope = 0;
  AudioObjectPropertyElement element = 0;

  bool operator<(const CoreAudioPropertyKey& other) const noexcept {
    return std::tie(object_id, selector, scope, element) <
           std::tie(other.object_id, other.selector, other.scope, other.element);
  }
};

/// Thread-safe CoreAudio property fake. Listener callbacks are always invoked
/// after the fake releases its mutex, so callbacks may safely re-enter it.
class FakeCoreAudioPropertyApi final : public ICoreAudioPropertyApi {
public:
  struct Listener {
    AudioObjectID object_id = 0;
    AudioObjectPropertyAddress address{};
    AudioObjectPropertyListenerProc callback = nullptr;
    void* context = nullptr;
  };

  struct Write {
    AudioObjectID object_id = 0;
    AudioObjectPropertyAddress address{};
    UInt32 size = 0;
    std::vector<uint8_t> bytes;
  };

  using Value =
      std::variant<UInt32, Float64, AudioStreamBasicDescription, std::string,
                   std::vector<AudioStreamID>, AudioValueRange, std::vector<AudioValueRange>>;

  void setValue(AudioObjectID object_id, const AudioObjectPropertyAddress& address, Value value) {
    std::lock_guard<std::mutex> lock(mutex_);
    values_[key(object_id, address)] = std::move(value);
  }

  void setAlive(AudioObjectID object_id, UInt32 value) {
    setValue(object_id, address(kAudioDevicePropertyDeviceIsAlive), value);
  }
  void setRate(AudioObjectID object_id, Float64 value) {
    setValue(object_id, address(kAudioDevicePropertyNominalSampleRate), value);
  }
  void setBuffer(AudioObjectID object_id, UInt32 value) {
    setValue(object_id, address(kAudioDevicePropertyBufferFrameSize), value);
  }
  void setBufferRange(AudioObjectID object_id, Float64 minimum, Float64 maximum,
                      AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal) {
    setValue(object_id, address(kAudioDevicePropertyBufferFrameSizeRange, scope),
             AudioValueRange{minimum, maximum});
  }
  void setClockDomain(AudioObjectID object_id, UInt32 value) {
    setValue(object_id, address(kAudioDevicePropertyClockDomain), value);
  }
  void setLatency(AudioObjectID object_id, AudioObjectPropertyScope scope, UInt32 value) {
    setValue(object_id, address(kAudioDevicePropertyLatency, scope), value);
  }
  void setSafetyOffset(AudioObjectID object_id, AudioObjectPropertyScope scope, UInt32 value) {
    setValue(object_id, address(kAudioDevicePropertySafetyOffset, scope), value);
  }
  void setDeviceUID(AudioObjectID object_id, std::string value) {
    setValue(object_id, address(kAudioDevicePropertyDeviceUID), std::move(value));
  }
  void setStreams(AudioObjectID object_id, AudioObjectPropertyScope scope,
                  std::vector<AudioStreamID> value) {
    setValue(object_id, address(kAudioDevicePropertyStreams, scope), std::move(value));
  }
  void setFormat(AudioStreamID stream_id, AudioObjectPropertySelector selector,
                 const AudioStreamBasicDescription& value) {
    setValue(stream_id, address(selector), value);
  }
  void setStreamLatency(AudioStreamID stream_id, UInt32 value) {
    setValue(stream_id, address(kAudioStreamPropertyLatency), value);
  }
  void setChannelCount(AudioDeviceID object_id, AudioObjectPropertyScope scope, UInt32 channels) {
    std::lock_guard<std::mutex> lock(mutex_);
    channel_counts_[{object_id, kAudioDevicePropertyStreamConfiguration, scope,
                     kAudioObjectPropertyElementMain}] = channels;
  }
  void setHogModeAllowed(UInt32 value) {
    setValue(kAudioObjectSystemObject, address(kAudioHardwarePropertyHogModeIsAllowed), value);
  }
  void setHogModeSettable(bool settable) {
    setSettable(kAudioObjectSystemObject, address(kAudioHardwarePropertyHogModeIsAllowed),
                settable);
  }

  /// Replace the value visible to reads without generating a property event.
  void thirdPartyMutation(AudioObjectID object_id, const AudioObjectPropertyAddress& property,
                          Value value) {
    setValue(object_id, property, std::move(value));
  }
  void thirdPartyRateMutation(AudioObjectID object_id, Float64 value) {
    thirdPartyMutation(object_id, address(kAudioDevicePropertyNominalSampleRate), value);
  }

  /// Queue values returned by subsequent reads for deterministic readback tests.
  void queueReadback(AudioObjectID object_id, const AudioObjectPropertyAddress& property,
                     Value value) {
    std::lock_guard<std::mutex> lock(mutex_);
    readbacks_[key(object_id, property)].push_back(std::move(value));
  }
  void setControlledReadback(AudioObjectID object_id, Float64 value) {
    queueReadback(object_id, address(kAudioDevicePropertyNominalSampleRate), value);
  }

  void setAutomaticListenerDelivery(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    automatic_listener_delivery_ = enabled;
  }
  void suppressListenerDelivery() {
    setAutomaticListenerDelivery(false);
  }
  void enableAutomaticListenerDelivery() {
    setAutomaticListenerDelivery(true);
  }

  void notify(AudioObjectID object_id, AudioObjectPropertySelector selector) {
    const AudioObjectPropertyAddress property = address(selector);
    deliver(object_id, property);
  }
  void notifyRateChange(AudioObjectID object_id) {
    notify(object_id, kAudioDevicePropertyNominalSampleRate);
  }
  void deliverPendingListeners() {
    for (;;) {
      PendingNotification notification;
      {
        std::lock_guard<std::mutex> lock(mutex_);
        if (pending_notifications_.empty()) {
          return;
        }
        notification = pending_notifications_.front();
        pending_notifications_.pop_front();
      }
      deliver(notification.object_id, notification.address);
    }
  }

  void setSettable(AudioObjectID object_id, const AudioObjectPropertyAddress& property,
                   bool settable) {
    std::lock_guard<std::mutex> lock(mutex_);
    settable_[key(object_id, property)] = settable;
  }
  void setRateSettable(AudioObjectID object_id, bool settable) {
    setSettable(object_id, address(kAudioDevicePropertyNominalSampleRate), settable);
  }

  void failGet(OSStatus error) {
    std::lock_guard<std::mutex> lock(mutex_);
    get_error_ = error;
  }
  void failGetSize(OSStatus error) {
    std::lock_guard<std::mutex> lock(mutex_);
    get_size_error_ = error;
  }
  void failSet(OSStatus error) {
    std::lock_guard<std::mutex> lock(mutex_);
    set_error_ = error;
  }
  void failSettable(OSStatus error) {
    std::lock_guard<std::mutex> lock(mutex_);
    settable_error_ = error;
  }
  void failAdd(OSStatus error) {
    std::lock_guard<std::mutex> lock(mutex_);
    add_error_ = error;
  }
  void failRemove(OSStatus error) {
    std::lock_guard<std::mutex> lock(mutex_);
    remove_error_ = error;
  }
  void clearErrors() {
    std::lock_guard<std::mutex> lock(mutex_);
    get_error_ = noErr;
    get_size_error_ = noErr;
    set_error_ = noErr;
    settable_error_ = noErr;
    add_error_ = noErr;
    remove_error_ = noErr;
  }

  std::vector<Listener> listenerLedger() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return listeners_ledger_;
  }
  std::vector<Write> writeLedger() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return writes_;
  }
  size_t listenerCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return listeners_.size();
  }
  size_t callbackDeliveries() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return callback_deliveries_;
  }
  void clearLedgers() {
    std::lock_guard<std::mutex> lock(mutex_);
    listeners_ledger_.clear();
    writes_.clear();
    callback_deliveries_ = 0;
  }

  OSStatus addPropertyListener(AudioObjectID object_id, const AudioObjectPropertyAddress* property,
                               AudioObjectPropertyListenerProc callback,
                               void* context) noexcept override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (add_error_ != noErr) {
      return add_error_;
    }
    listeners_.push_back({object_id, *property, callback, context});
    listeners_ledger_.push_back({object_id, *property, callback, context});
    return noErr;
  }

  OSStatus removePropertyListener(AudioObjectID object_id,
                                  const AudioObjectPropertyAddress* property,
                                  AudioObjectPropertyListenerProc callback,
                                  void* context) noexcept override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (remove_error_ != noErr) {
      return remove_error_;
    }
    const auto it =
        std::find_if(listeners_.begin(), listeners_.end(), [&](const Listener& listener) {
          return listener.object_id == object_id && listener.callback == callback &&
                 listener.context == context && sameAddress(listener.address, *property);
        });
    if (it == listeners_.end()) {
      return -1;
    }
    listeners_.erase(it);
    return noErr;
  }

  OSStatus getPropertyData(AudioObjectID object_id, const AudioObjectPropertyAddress* property,
                           UInt32* size, void* data) noexcept override {
    Value value;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (get_error_ != noErr) {
        return get_error_;
      }
      const CoreAudioPropertyKey property_key = key(object_id, *property);
      auto queued = readbacks_.find(property_key);
      if (queued != readbacks_.end() && !queued->second.empty()) {
        value = queued->second.front();
        queued->second.pop_front();
      } else {
        const auto found = values_.find(property_key);
        if (found == values_.end()) {
          return readFromSpecialStorage(object_id, *property, *size, data);
        }
        value = found->second;
      }
    }
    return copyValue(*property, value, size, data);
  }

  OSStatus getPropertyDataSize(AudioObjectID object_id, const AudioObjectPropertyAddress* property,
                               UInt32* size) noexcept override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (get_size_error_ != noErr) {
      return get_size_error_;
    }
    const auto found = values_.find(key(object_id, *property));
    if (found != values_.end()) {
      *size = valueSize(found->second);
      return noErr;
    }
    const auto channels = channel_counts_.find(key(object_id, *property));
    if (channels != channel_counts_.end()) {
      *size = sizeof(AudioBufferList);
      return noErr;
    }
    return -1;
  }

  OSStatus setPropertyData(AudioObjectID object_id, const AudioObjectPropertyAddress* property,
                           UInt32 size, const void* data) noexcept override {
    Value value;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (set_error_ != noErr) {
        return set_error_;
      }
      if (!decodeValue(*property, size, data, value)) {
        return -1;
      }
      values_[key(object_id, *property)] = value;
      Write write{object_id, *property, size, std::vector<uint8_t>(size)};
      if (size != 0 && data != nullptr) {
        std::memcpy(write.bytes.data(), data, size);
      }
      writes_.push_back(std::move(write));
    }
    deliverIfAutomatic(object_id, *property);
    return noErr;
  }

  OSStatus isPropertySettable(AudioObjectID object_id, const AudioObjectPropertyAddress* property,
                              Boolean* settable) noexcept override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (settable_error_ != noErr) {
      return settable_error_;
    }
    const auto found = settable_.find(key(object_id, *property));
    *settable = found == settable_.end() ? 1 : (found->second ? 1 : 0);
    return noErr;
  }

private:
  struct PendingNotification {
    AudioObjectID object_id = 0;
    AudioObjectPropertyAddress address{};
  };

  static AudioObjectPropertyAddress
  address(AudioObjectPropertySelector selector,
          AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal,
          AudioObjectPropertyElement element = kAudioObjectPropertyElementMain) noexcept {
    return {selector, scope, element};
  }
  static CoreAudioPropertyKey key(AudioObjectID object_id,
                                  const AudioObjectPropertyAddress& property) noexcept {
    return {object_id, property.mSelector, property.mScope, property.mElement};
  }
  static bool sameAddress(const AudioObjectPropertyAddress& lhs,
                          const AudioObjectPropertyAddress& rhs) noexcept {
    return lhs.mSelector == rhs.mSelector && lhs.mScope == rhs.mScope &&
           lhs.mElement == rhs.mElement;
  }

  static UInt32 valueSize(const Value& value) noexcept {
    return std::visit(
        [](const auto& item) -> UInt32 {
          using T = std::decay_t<decltype(item)>;
          if constexpr (std::is_same_v<T, std::string>) {
            return sizeof(CFStringRef);
          } else if constexpr (std::is_same_v<T, std::vector<AudioStreamID>>) {
            return static_cast<UInt32>(item.size() * sizeof(AudioStreamID));
          } else if constexpr (std::is_same_v<T, std::vector<AudioValueRange>>) {
            return static_cast<UInt32>(item.size() * sizeof(AudioValueRange));
          } else {
            return sizeof(T);
          }
        },
        value);
  }

  static bool decodeValue(const AudioObjectPropertyAddress& property, UInt32 size, const void* data,
                          Value& value) noexcept {
    if (data == nullptr) {
      return false;
    }
    if (property.mSelector == kAudioDevicePropertyNominalSampleRate && size == sizeof(Float64)) {
      value = *static_cast<const Float64*>(data);
      return true;
    }
    if ((property.mSelector == kAudioDevicePropertyDeviceIsAlive ||
         property.mSelector == kAudioDevicePropertyBufferFrameSize ||
         property.mSelector == kAudioHardwarePropertyHogModeIsAllowed) &&
        size == sizeof(UInt32)) {
      value = *static_cast<const UInt32*>(data);
      return true;
    }
    return false;
  }

  static OSStatus copyValue(const AudioObjectPropertyAddress& property, const Value& value,
                            UInt32* size, void* data) noexcept {
    if (data == nullptr || size == nullptr) {
      return -1;
    }
    return std::visit(
        [&](const auto& item) -> OSStatus {
          using T = std::decay_t<decltype(item)>;
          if constexpr (std::is_same_v<T, std::string>) {
            if (*size < sizeof(CFStringRef)) {
              return -1;
            }
            auto* result = static_cast<CFStringRef*>(data);
            *result =
                CFStringCreateWithCString(kCFAllocatorDefault, item.c_str(), kCFStringEncodingUTF8);
            *size = sizeof(CFStringRef);
            return *result == nullptr ? -1 : noErr;
          } else if constexpr (std::is_same_v<T, std::vector<AudioStreamID>>) {
            const UInt32 bytes = static_cast<UInt32>(item.size() * sizeof(AudioStreamID));
            if (*size < bytes) {
              return -1;
            }
            std::memcpy(data, item.data(), bytes);
            *size = bytes;
            return noErr;
          } else if constexpr (std::is_same_v<T, std::vector<AudioValueRange>>) {
            const UInt32 bytes = static_cast<UInt32>(item.size() * sizeof(AudioValueRange));
            if (*size < bytes) {
              return -1;
            }
            std::memcpy(data, item.data(), bytes);
            *size = bytes;
            return noErr;
          } else {
            if (*size < sizeof(T)) {
              return -1;
            }
            std::memcpy(data, &item, sizeof(T));
            *size = sizeof(T);
            return noErr;
          }
        },
        value);
  }

  OSStatus readFromSpecialStorage(AudioObjectID object_id,
                                  const AudioObjectPropertyAddress& property, UInt32 size,
                                  void* data) const noexcept {
    if (property.mSelector != kAudioDevicePropertyStreamConfiguration || data == nullptr ||
        size < sizeof(AudioBufferList)) {
      return -1;
    }
    const auto found = channel_counts_.find(key(object_id, property));
    if (found == channel_counts_.end()) {
      return -1;
    }
    auto* buffers = static_cast<AudioBufferList*>(data);
    buffers->mNumberBuffers = 1;
    buffers->mBuffers[0].mNumberChannels = found->second;
    buffers->mBuffers[0].mDataByteSize = 0;
    buffers->mBuffers[0].mData = nullptr;
    return noErr;
  }

  void deliverIfAutomatic(AudioObjectID object_id, const AudioObjectPropertyAddress& property) {
    bool automatic = false;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      automatic = automatic_listener_delivery_;
      if (!automatic) {
        pending_notifications_.push_back({object_id, property});
      }
    }
    if (automatic) {
      deliver(object_id, property);
    }
  }

  void deliver(AudioObjectID object_id, const AudioObjectPropertyAddress& property) {
    std::vector<Listener> callbacks;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      for (const Listener& listener : listeners_) {
        if (listener.object_id == object_id && sameAddress(listener.address, property)) {
          callbacks.push_back(listener);
        }
      }
    }
    for (const Listener& listener : callbacks) {
      {
        std::lock_guard<std::mutex> lock(mutex_);
        ++callback_deliveries_;
      }
      if (listener.callback != nullptr) {
        listener.callback(object_id, 1, &property, listener.context);
      }
    }
  }

  mutable std::mutex mutex_;
  std::map<CoreAudioPropertyKey, Value> values_;
  std::map<CoreAudioPropertyKey, std::deque<Value>> readbacks_;
  std::map<CoreAudioPropertyKey, bool> settable_;
  std::map<CoreAudioPropertyKey, UInt32> channel_counts_;
  std::vector<Listener> listeners_;
  std::vector<Listener> listeners_ledger_;
  std::vector<Write> writes_;
  std::deque<PendingNotification> pending_notifications_;
  bool automatic_listener_delivery_{true};
  size_t callback_deliveries_{0};
  OSStatus get_error_{noErr};
  OSStatus get_size_error_{noErr};
  OSStatus set_error_{noErr};
  OSStatus settable_error_{noErr};
  OSStatus add_error_{noErr};
  OSStatus remove_error_{noErr};
};

} // namespace orpheus::test_support
