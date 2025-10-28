# Simple Clip Player Example

**Purpose:** Demonstrates basic audio file playback using the Orpheus SDK

This is the simplest possible example of using the Orpheus SDK for audio playback. It loads a single audio file and plays it back through the system's audio output.

## Features Demonstrated

- Opening audio files with `IAudioFileReader`
- Creating and configuring `ITransportController`
- Registering audio clips for playback
- Starting and stopping audio playback
- Using platform audio drivers (CoreAudio on macOS)
- Basic audio callback implementation

## Building

```bash
cd /path/to/orpheus-sdk
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DORPHEUS_BUILD_EXAMPLES=ON
cmake --build build --target simple_player
```

## Usage

```bash
./build/examples/simple_player/simple_player <audio_file.wav>
```

**Example:**

```bash
./build/examples/simple_player/simple_player /path/to/music.wav
```

**Expected Output:**

```
Loaded: /path/to/music.wav
Duration: 3.5 seconds
Sample rate: 48000 Hz
Channels: 2

Playing...
Playback complete!
```

## Supported Audio Formats

- **WAV** (PCM, IEEE float)
- **AIFF** (PCM)
- **FLAC** (lossless)

Formats are handled automatically by `libsndfile` - no format-specific code needed.

## Code Structure

The example follows a simple 7-step pattern:

1. **Open audio file** - Use `createAudioFileReader()` and `open()`
2. **Create transport** - Use `createTransportController()` with config
3. **Register clip** - Use `registerClipAudio()` with file path and trim points
4. **Create driver** - Platform-specific (CoreAudio on macOS)
5. **Set up callback** - Implement `IAudioCallback` to connect driver to transport
6. **Start playback** - Use `startClip()` to begin audio
7. **Clean up** - Stop driver when done

## Platform Support

- **macOS:** CoreAudio driver (real-time audio output)
- **Windows:** Dummy driver (no audio output - WASAPI coming soon)
- **Linux:** Dummy driver (no audio output - ALSA/JACK coming soon)

## Key Concepts

### Audio Callback

The `SimpleAudioCallback` class connects the audio driver to the transport controller:

```cpp
class SimpleAudioCallback : public orpheus::IAudioCallback {
public:
  void processAudio(const float** inputs, float** outputs,
                    size_t num_channels, size_t num_frames) override {
    transport_->processAudio(outputs, num_channels, num_frames);
  }
};
```

This callback runs on a **real-time audio thread** - no allocations, no locks, no I/O!

### Clip Registration

Before playing audio, you must register it with the transport:

```cpp
orpheus::ClipRegistration clip_reg;
clip_reg.audio_file_path = "/path/to/audio.wav";
clip_reg.trim_in_samples = 0;
clip_reg.trim_out_samples = duration_samples;

auto clip_handle = transport->registerClipAudio(clip_reg);
```

The returned `clip_handle` is used to control playback.

### Error Handling

All SDK operations return error codes or result objects:

```cpp
auto result = reader->open(file_path);
if (!result.isOk()) {
  std::cerr << "Error: " << result.errorMessage << std::endl;
  return 1;
}
```

Always check return values before proceeding!

## Limitations

- Plays entire file from start to finish (no seeking)
- Single clip playback (no mixing multiple clips)
- No gain/fade controls
- No loop support
- Blocks until playback finishes

See `multi_clip_trigger` example for multi-clip playback and `offline_renderer` for non-real-time rendering.

## Troubleshooting

**Problem: "Failed to open audio file"**

- Check file path is correct
- Verify file format is supported (WAV/AIFF/FLAC)
- Ensure file is not corrupted

**Problem: "Failed to initialize audio driver"**

- macOS: Check audio output device in System Settings → Sound
- Windows/Linux: Dummy driver is used (no audio output)

**Problem: No audio output**

- macOS: Check volume and output device
- macOS: Grant audio permissions in System Settings → Privacy & Security
- Verify sample rate matches your audio interface (48kHz is standard)

**Problem: Audio glitches/dropouts**

- Increase buffer size from 512 to 1024 samples in `TransportConfig`
- Close other audio applications
- Check CPU usage isn't near 100%

## Next Steps

**For more advanced examples:**

- `multi_clip_trigger` - Play multiple clips simultaneously with triggering
- `offline_renderer` - Non-real-time rendering to WAV files

**For SDK documentation:**

- [Quick Start Guide](../../docs/QUICK_START.md)
- [Transport Integration Guide](../../docs/integration/TRANSPORT_INTEGRATION.md)
- [API Reference](../../docs/api/html/index.html)

## Related Files

- `simple_player.cpp` - Main source code
- `CMakeLists.txt` - Build configuration

---

**Last Updated:** October 26, 2025
**SDK Version:** M2 (Milestone 2)
