# OCC145 Audio Device Selection Fix

**Status:** Complete
**Date:** 2026-02-27
**Author:** Claude Code

## Problem

Clip Composer showed audio meters with signal but no sound was heard. The Audio Settings
dialog device dropdown listed only "Default Device" — no real devices — and switching the
system default output had no effect on playback.

## Root Cause Chain

1. `AudioEngine::getAvailableDevices()` returned a hardcoded `{"Default Device"}` stub
   (`AudioEngine.cpp`, marked TODO).
2. `AudioEngine::setAudioDevice()` created a CoreAudio driver but left `config.device_name`
   empty, so the driver always bound to the system default at init time.
3. `CoreAudioDriver::findDevice("")` (empty name) called
   `kAudioHardwarePropertyDefaultOutputDevice`, capturing device ID 60 at startup and
   holding it for the lifetime of the driver instance.
4. Changing the system default after launch had no effect because the driver never
   re-queried it.

## Fix

### Files Modified

| File | Change |
|------|--------|
| `apps/clip-composer/Source/Audio/AudioEngine.cpp` | Real device enumeration + device name passthrough |
| `apps/clip-composer/CMakeLists.txt` | Link `orpheus_audio_driver_manager` |

### `AudioEngine.cpp` — Added include

```cpp
#include <orpheus/audio_driver_manager.h>
```

### `AudioEngine::getAvailableDevices()` — Real enumeration

Replaced hardcoded stub with a call to `orpheus::createAudioDriverManager()`. "Default
Device" is always first so users can opt back to following the system default. Only
`driverType == "CoreAudio"` entries are listed (forward-compatible with ASIO/WASAPI on
other platforms).

```cpp
std::vector<std::string> AudioEngine::getAvailableDevices() const {
  auto manager = orpheus::createAudioDriverManager();
  if (!manager) return {"Default Device"};

  std::vector<std::string> names;
  names.push_back("Default Device");
  for (const auto& device : manager->enumerateDevices()) {
    if (device.driverType == "CoreAudio")
      names.push_back(device.name);
  }
  return names;
}
```

### `AudioEngine::setAudioDevice()` — Device name passthrough

Empty string and "Default Device" both map to the CoreAudio default. Any other name is
passed as `config.device_name`, triggering `CoreAudioDriver::findDevice(name)` which does
an exact-string match against `kAudioDevicePropertyDeviceNameCFString`.

```cpp
if (deviceName != "Default Device") {
  config.device_name = deviceName;
}
```

### `CMakeLists.txt` — Linker fix

`orpheus::createAudioDriverManager()` lives in `liborpheus_audio_driver_manager.a`, which
was not previously linked to the app target:

```cmake
orpheus_audio_driver_manager    # Device enumeration (createAudioDriverManager)
```

## Verification

- Build succeeded (all SDK libs + app, no warnings beyond pre-existing JUCE CMake notices)
- App launched successfully (PID 5269)
- Audio is audible — confirmed by user
- Audio Settings device dropdown verification pending (noted for next session)

## What Already Worked (Unchanged)

- `IAudioDriverManager::enumerateDevices()` — queries `kAudioHardwarePropertyDevices`,
  returns `AudioDeviceInfo` structs with human-readable `name` fields
- `CoreAudioDriver::findDevice(name)` — exact-string match against CoreAudio device names
- `AudioSettingsDialog` — already passed combo-box text directly to
  `AudioEngine::setAudioDevice()`, no UI changes needed
