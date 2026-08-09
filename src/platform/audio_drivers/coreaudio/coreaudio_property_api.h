// SPDX-License-Identifier: MIT
#pragma once

#include <CoreAudio/CoreAudio.h>

namespace orpheus {

/// Injectable access to the CoreAudio object-property API.
class ICoreAudioPropertyApi {
public:
  virtual ~ICoreAudioPropertyApi() = default;

  virtual OSStatus addPropertyListener(AudioObjectID, const AudioObjectPropertyAddress*,
                                       AudioObjectPropertyListenerProc, void*) noexcept = 0;
  virtual OSStatus removePropertyListener(AudioObjectID, const AudioObjectPropertyAddress*,
                                          AudioObjectPropertyListenerProc, void*) noexcept = 0;
  virtual OSStatus getPropertyData(AudioObjectID, const AudioObjectPropertyAddress*, UInt32*,
                                   void*) noexcept = 0;
  virtual OSStatus getPropertyDataSize(AudioObjectID, const AudioObjectPropertyAddress*,
                                       UInt32*) noexcept = 0;
  virtual OSStatus setPropertyData(AudioObjectID, const AudioObjectPropertyAddress*, UInt32,
                                   const void*) noexcept = 0;
  virtual OSStatus isPropertySettable(AudioObjectID, const AudioObjectPropertyAddress*,
                                      Boolean*) noexcept = 0;
};

/// Production adapter for CoreAudio's process-global object-property functions.
class CoreAudioPropertyApi final : public ICoreAudioPropertyApi {
public:
  OSStatus addPropertyListener(AudioObjectID, const AudioObjectPropertyAddress*,
                               AudioObjectPropertyListenerProc, void*) noexcept override;
  OSStatus removePropertyListener(AudioObjectID, const AudioObjectPropertyAddress*,
                                  AudioObjectPropertyListenerProc, void*) noexcept override;
  OSStatus getPropertyData(AudioObjectID, const AudioObjectPropertyAddress*, UInt32*,
                           void*) noexcept override;
  OSStatus getPropertyDataSize(AudioObjectID, const AudioObjectPropertyAddress*,
                               UInt32*) noexcept override;
  OSStatus setPropertyData(AudioObjectID, const AudioObjectPropertyAddress*, UInt32,
                           const void*) noexcept override;
  OSStatus isPropertySettable(AudioObjectID, const AudioObjectPropertyAddress*,
                              Boolean*) noexcept override;
};

} // namespace orpheus
