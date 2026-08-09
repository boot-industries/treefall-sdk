// SPDX-License-Identifier: MIT
#include "coreaudio_sample_rate_transaction.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace orpheus {

CoreAudioSampleRateTransaction::CoreAudioSampleRateTransaction(
    ICoreAudioPropertyApi& property_api, const detail::ResolvedCoreAudioRoute& route,
    uint32_t requested_rate, std::chrono::milliseconds timeout) noexcept
    : property_api_(property_api), requested_rate_(static_cast<Float64>(requested_rate)),
      timeout_(timeout) {
  if (route.output_device_id != 0) {
    endpoints_[endpoint_count_++].device_id = route.output_device_id;
  }
  if (route.input_device_id != 0 && route.input_device_id != route.output_device_id &&
      endpoint_count_ < endpoints_.size()) {
    endpoints_[endpoint_count_++].device_id = route.input_device_id;
  }
}

CoreAudioSampleRateTransaction::~CoreAudioSampleRateTransaction() {
  if (!committed_ && !rollback_done_) {
    rollback();
  }
}

AudioRouteRuntimeOutcome CoreAudioSampleRateTransaction::begin() noexcept {
  if (begun_ || committed_ || endpoint_count_ == 0 || requested_rate_ <= 0.0) {
    return fail(AudioRouteRuntimeOutcome::BackendFailure);
  }
  begun_ = true;

  const AudioObjectPropertyAddress address = nominalRateAddress();
  bool any_change = false;
  for (size_t index = 0; index < endpoint_count_; ++index) {
    Endpoint& endpoint = endpoints_[index];
    Float64 current_rate = 0.0;
    if (!readRate(endpoint, current_rate)) {
      return fail(AudioRouteRuntimeOutcome::BackendFailure);
    }
    endpoint.previous_rate = current_rate;
    endpoint.needs_change = current_rate != requested_rate_;
    endpoint.confirmed = !endpoint.needs_change;
    any_change = any_change || endpoint.needs_change;
  }

  // A same-rate activation has no listener or nominal-rate write side effect.
  if (!any_change) {
    return AudioRouteRuntimeOutcome::Healthy;
  }

  for (size_t index = 0; index < endpoint_count_; ++index) {
    Endpoint& endpoint = endpoints_[index];
    if (!endpoint.needs_change) {
      continue;
    }
    Boolean settable = 0;
    if (property_api_.isPropertySettable(endpoint.device_id, &address, &settable) != noErr) {
      return fail(AudioRouteRuntimeOutcome::BackendFailure);
    }
    if (settable == 0) {
      return fail(AudioRouteRuntimeOutcome::SampleRateChangeFailed);
    }
  }

  for (size_t index = 0; index < endpoint_count_; ++index) {
    Endpoint& endpoint = endpoints_[index];
    if (!endpoint.needs_change) {
      continue;
    }
    if (property_api_.addPropertyListener(endpoint.device_id, &address, &propertyChanged, this) !=
        noErr) {
      return fail(AudioRouteRuntimeOutcome::BackendFailure);
    }
    endpoint.registered = true;
  }

  for (size_t index = 0; index < endpoint_count_; ++index) {
    Endpoint& endpoint = endpoints_[index];
    if (!endpoint.needs_change) {
      continue;
    }
    endpoint.write_started = true;
    if (property_api_.setPropertyData(endpoint.device_id, &address, sizeof(requested_rate_),
                                      &requested_rate_) != noErr) {
      return fail(AudioRouteRuntimeOutcome::SampleRateChangeFailed);
    }
  }

  const auto deadline = std::chrono::steady_clock::now() + timeout_;
  while (!allConfirmed()) {
    const uint32_t notifications = notification_bits_.exchange(0, std::memory_order_acq_rel);
    if (notifications != 0) {
      for (size_t index = 0; index < endpoint_count_; ++index) {
        Endpoint& endpoint = endpoints_[index];
        if (!endpoint.needs_change || endpoint.confirmed ||
            (notifications & (uint32_t{1} << index)) == 0) {
          continue;
        }
        Float64 observed_rate = 0.0;
        if (!readRate(endpoint, observed_rate)) {
          return fail(AudioRouteRuntimeOutcome::BackendFailure);
        }
        if (observed_rate == requested_rate_) {
          endpoint.confirmed = true;
        }
      }
      continue;
    }

    std::unique_lock<std::mutex> lock(notification_mutex_);
    const bool notified = notification_changed_.wait_until(
        lock, deadline, [&] { return notification_bits_.load(std::memory_order_acquire) != 0; });
    if (!notified) {
      return fail(AudioRouteRuntimeOutcome::SampleRateChangeFailed);
    }
  }

  removeListeners();
  return AudioRouteRuntimeOutcome::Healthy;
}

void CoreAudioSampleRateTransaction::commit() noexcept {
  if (committed_) {
    return;
  }
  removeListeners();
  committed_ = true;
}

OSStatus CoreAudioSampleRateTransaction::propertyChanged(AudioObjectID object_id, UInt32,
                                                         const AudioObjectPropertyAddress*,
                                                         void* context) noexcept {
  auto* transaction = static_cast<CoreAudioSampleRateTransaction*>(context);
  for (size_t index = 0; index < transaction->endpoint_count_; ++index) {
    if (transaction->endpoints_[index].device_id == object_id) {
      transaction->notification_bits_.fetch_or(uint32_t{1} << index, std::memory_order_release);
      transaction->notification_changed_.notify_one();
      break;
    }
  }
  return noErr;
}

AudioObjectPropertyAddress CoreAudioSampleRateTransaction::nominalRateAddress() noexcept {
  return {kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal,
          kAudioObjectPropertyElementMain};
}
bool CoreAudioSampleRateTransaction::readRate(const Endpoint& endpoint, Float64& rate) noexcept {
  const AudioObjectPropertyAddress address = nominalRateAddress();
  UInt32 size = sizeof(rate);
  return property_api_.getPropertyData(endpoint.device_id, &address, &size, &rate) == noErr &&
         size == sizeof(rate) && std::isfinite(rate) && rate > 0.0;
}

bool CoreAudioSampleRateTransaction::allConfirmed() const noexcept {
  for (size_t index = 0; index < endpoint_count_; ++index) {
    if (!endpoints_[index].confirmed) {
      return false;
    }
  }
  return true;
}

void CoreAudioSampleRateTransaction::removeListeners() noexcept {
  const AudioObjectPropertyAddress address = nominalRateAddress();
  for (size_t index = endpoint_count_; index != 0; --index) {
    Endpoint& endpoint = endpoints_[index - 1];
    if (endpoint.registered) {
      property_api_.removePropertyListener(endpoint.device_id, &address, &propertyChanged, this);
      endpoint.registered = false;
    }
  }
}

void CoreAudioSampleRateTransaction::rollback() noexcept {
  rollback_done_ = true;
  removeListeners();
  const AudioObjectPropertyAddress address = nominalRateAddress();
  for (size_t index = endpoint_count_; index != 0; --index) {
    Endpoint& endpoint = endpoints_[index - 1];
    if (!endpoint.write_started) {
      continue;
    }
    Float64 current_rate = 0.0;
    UInt32 size = sizeof(current_rate);
    if (property_api_.getPropertyData(endpoint.device_id, &address, &size, &current_rate) !=
            noErr ||
        size != sizeof(current_rate) || current_rate != requested_rate_) {
      continue;
    }
    property_api_.setPropertyData(endpoint.device_id, &address, sizeof(endpoint.previous_rate),
                                  &endpoint.previous_rate);
  }
}

AudioRouteRuntimeOutcome
CoreAudioSampleRateTransaction::fail(AudioRouteRuntimeOutcome outcome) noexcept {
  rollback();
  return outcome;
}

} // namespace orpheus
