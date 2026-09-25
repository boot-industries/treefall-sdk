# Offline rendering with the Treefall SDK

**SDK Version:** 0.9.1

The SDK does not ship an `IOfflineRenderer` or `OfflineRenderJob` API, and it does not provide an `offline_renderer` executable. Offline work is a normal control-side composition of the installed public interfaces:

1. Query `getAudioFileCapabilities()` and call `preflightAudioFileWrite()` before creating media objects.
2. Create an `IAudioFileWriter` and write the source (or use an existing file).
3. Call `probeAudioFile()` and open an `IAudioFileReader` on the control/background thread.
4. Create a `TransportConfig`, register and prepare the source with `ITransportController`, and configure its routing with `setGroupOutputBus()`.
5. Call `processAudio()` with preallocated planar buffers. This is the renderer's audio operation; it must not perform file I/O, callbacks, sleeping, or hardware-driver work.
6. Interleave each completed block and write it with `IAudioFileWriter` on the control/background thread, then close the file and inspect its metadata.

The installed package fixture `tests/cmake/find_package/offline_render.cpp` is the executable reference for this recipe. It links only `Orpheus::transport` and `Orpheus::audio_utils`; it includes no SDK private headers or source files and does not use the Dummy driver.

## Deterministic fixture contract

The reference fixture writes 8,193 stereo 48 kHz PCM16 source frames:

```text
qL(n) = ((17*n) % 16384) - 8192
qR(n) = ((29*n) % 16384) - 8192
```

It renders exactly 8,450 frames (the final 257 frames are silence), using a fresh transport for each block size `256`, `512`, `1024`, and `2048`, and repeats every size twice. The transport uses two outputs, one active voice, one group, two source channels, `SourceChannelPolicy::Discrete`, zero fades/delay, unity gain, centered pan, playback rate 1, unmuted, and non-looping playback.

The fixture verifies all of the following before emitting `offline-render-hashes.json` in its binary directory:

- preflight, writer open/write/close, probe, reader open/read/close, and transport status results;
- output metadata, exact frame count, both channel samples, trailing silence, and an explicit EOF read;
- RIFF little-endian bounds, chunk padding, PCM16 format, and a 33,800-byte `data` payload;
- the payload SHA-256 `e59bff0bbf328128a96992e19b1047688227778480642251b1fa1f27f6b85eaa` using public `sha256File()`;
- equality of complete WAV hashes across all four block sizes and both repetitions.

The payload digest is the portable golden. Complete-container equality is additionally recorded for this provider/build; codec headers are not presented as a cross-provider portability guarantee.

## Building the installed fixture

The fixture is registered by the existing `tests/cmake/find_package/CMakeLists.txt` as `orpheus_find_package_offline_render` and is run by the clean-prefix `cmake_find_package` test. Build and run the package tests from a configured SDK build; do not copy the fixture into an application or link private implementation targets.

The source and output files are fixture-owned artifacts in the executable's binary directory. A missing writer/reader provider is a prerequisite failure, not a passing offline result.
