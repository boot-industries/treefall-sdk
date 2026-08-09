// SPDX-License-Identifier: MIT
#include "coreaudio_property_api.h"

namespace orpheus {

OSStatus CoreAudioPropertyApi::addPropertyListener(AudioObjectID object_id,
                                                   const AudioObjectPropertyAddress* address,
                                                   AudioObjectPropertyListenerProc listener,
                                                   void* context) noexcept {
  return AudioObjectAddPropertyListener(object_id, address, listener, context);
}

OSStatus CoreAudioPropertyApi::removePropertyListener(AudioObjectID object_id,
                                                      const AudioObjectPropertyAddress* address,
                                                      AudioObjectPropertyListenerProc listener,
                                                      void* context) noexcept {
  return AudioObjectRemovePropertyListener(object_id, address, listener, context);
}

OSStatus CoreAudioPropertyApi::getPropertyData(AudioObjectID object_id,
                                               const AudioObjectPropertyAddress* address,
                                               UInt32* size, void* data) noexcept {
  return AudioObjectGetPropertyData(object_id, address, 0, nullptr, size, data);
}

OSStatus CoreAudioPropertyApi::getPropertyDataSize(AudioObjectID object_id,
                                                   const AudioObjectPropertyAddress* address,
                                                   UInt32* size) noexcept {
  return AudioObjectGetPropertyDataSize(object_id, address, 0, nullptr, size);
}

OSStatus CoreAudioPropertyApi::setPropertyData(AudioObjectID object_id,
                                               const AudioObjectPropertyAddress* address,
                                               UInt32 size, const void* data) noexcept {
  return AudioObjectSetPropertyData(object_id, address, 0, nullptr, size, data);
}

OSStatus CoreAudioPropertyApi::isPropertySettable(AudioObjectID object_id,
                                                  const AudioObjectPropertyAddress* address,
                                                  Boolean* settable) noexcept {
  return AudioObjectIsPropertySettable(object_id, address, settable);
}

} // namespace orpheus
