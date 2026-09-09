# Treefall SDK Examples

**SDK Version:** 0.9.0

Practical applications demonstrating the installed Treefall SDK public APIs. The repository contains two runnable examples; offline rendering is an installed-package fixture recipe rather than an example executable. Existing Orpheus package and API names remain compatible.

## Quick start

```bash
cd /path/to/orpheus-sdk
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DORPHEUS_BUILD_EXAMPLES=ON
cmake --build build --target simple_player multi_clip_trigger
```

Run simple playback:

```bash
./build/examples/simple_player/simple_player audio.wav
```

Run interactive triggering:

```bash
./build/examples/multi_clip_trigger/multi_clip_trigger sound1.wav sound2.wav sound3.wav
```

## Available examples

### Simple Clip Player (`simple_player/`)

A beginner example that opens one audio file, creates a transport, registers and prepares one clip, and connects it to the selected audio driver.

[View documentation](simple_player/README.md)

### Multi-Clip Trigger (`multi_clip_trigger/`)

An interactive soundboard-style example that loads multiple files, prepares clips, triggers them from the keyboard, and stops all clips with `s`.

[View documentation](multi_clip_trigger/README.md)

## Offline rendering

There is no `IOfflineRenderer`, `OfflineRenderJob`, or `offline_renderer` executable in the SDK. Compose the public `IAudioFileReader`, `IAudioFileWriter`, and `ITransportController` APIs on a control/background thread, keeping `processAudio()` limited to preallocated audio buffers. The installed-only deterministic reference is documented in [the offline rendering recipe](offline_renderer/README.md) and exercised by the `orpheus_find_package_offline_render` package fixture.

## Shared guidance

- Reader/writer open, close, probing, and file hashing are control/background operations; never call them from an audio callback.
- `processAudio()` receives exactly the configured output-channel buffers and no more than `maxBlockFrames` frames.
- Always check factory pointers and every returned `SessionGraphError`/`Result` before proceeding.
- The real-time examples use CoreAudio where enabled on macOS and the existing Dummy driver where hardware backends are disabled on other platforms. The Dummy driver is not part of the offline recipe.
- Supported file-format policy is WAV, AIFF, and FLAC; MP3 and OGG encoding are not supported.

## Learning path

1. Start with `simple_player` for one prepared clip and a callback.
2. Continue with `multi_clip_trigger` for multiple handles and interactive control.
3. Read the offline recipe for deterministic, non-hardware batch composition through installed targets.

See the individual READMEs for build commands, controls, troubleshooting, and API examples.
