# Multi-Clip Trigger Example

**Purpose:** Demonstrates soundboard-style playback with multiple simultaneous clips

This example shows how to use the Orpheus SDK for real-time multi-clip triggering, similar to professional soundboard applications like SpotOn, QLab, or Clip Composer.

## Features Demonstrated

- Loading multiple audio files simultaneously
- Registering multiple clips with transport
- Interactive keyboard-based triggering (1-9 keys)
- Playing multiple clips at once (mixing)
- Stopping all clips (panic button)
- Handling different sample rates across clips
- Real-time user interaction loop

## Building

```bash
cd /path/to/orpheus-sdk
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DORPHEUS_BUILD_EXAMPLES=ON
cmake --build build --target multi_clip_trigger
```

## Usage

```bash
./build/examples/multi_clip_trigger/multi_clip_trigger <audio1.wav> <audio2.wav> ... <audio9.wav>
```

**Example:**

```bash
./build/examples/multi_clip_trigger/multi_clip_trigger \
  sounds/kick.wav \
  sounds/snare.wav \
  sounds/hihat.wav \
  sounds/clap.wav
```

**Expected Output:**

```
Loading audio files...
  [1] Loaded: kick.wav (0.5s, 48000Hz, 2ch)
  [2] Loaded: snare.wav (0.8s, 48000Hz, 2ch)
  [3] Loaded: hihat.wav (0.3s, 48000Hz, 2ch)
  [4] Loaded: clap.wav (0.6s, 48000Hz, 2ch)

Registering clips with transport...

Loaded 4 clips:
---------------------
  [1] kick.wav (0.5s)
  [2] snare.wav (0.8s)
  [3] hihat.wav (0.3s)
  [4] clap.wav (0.6s)

Press 1-4 to trigger clips, 's' to stop all, 'q' to quit

> 1
Triggered: kick.wav
> 2
Triggered: snare.wav
> s
Stopped all clips
> q
Quitting...
Shutdown complete.
```

## Interactive Controls

Once running, use these keyboard commands:

- **1-9** - Trigger clips 1-9
- **s** - Stop all playing clips (panic button)
- **q** - Quit application
- **h** - Show help

## Supported Scenarios

### Drum Pad / Beat Making

```bash
./multi_clip_trigger kick.wav snare.wav hihat.wav tom1.wav tom2.wav crash.wav
```

Press keys 1-6 to create drum patterns in real-time!

### Soundboard for Theater/Broadcast

```bash
./multi_clip_trigger \
  applause.wav \
  laugh_track.wav \
  doorbell.wav \
  phone_ring.wav \
  background_music.wav
```

Trigger sound effects during live performance or broadcast.

### Music Production Sketching

```bash
./multi_clip_trigger \
  bass_loop.wav \
  drums_loop.wav \
  synth_pad.wav \
  vocal_sample.wav
```

Layer loops and samples to sketch musical ideas.

## Key Concepts

### Multi-Clip Mixing

The Orpheus SDK automatically mixes all playing clips:

```cpp
// All clips are mixed together by the transport
transport->startClip(clip1_handle, 0);  // Start first clip
transport->startClip(clip2_handle, 0);  // Start second clip (mixed with first)
transport->startClip(clip3_handle, 0);  // Start third clip (mixed with both)
```

No manual mixing code needed - the SDK handles it!

### Sample Rate Handling

Different audio files may have different sample rates:

```cpp
// Find highest sample rate across all clips
uint32_t max_sample_rate = 48000;
for (auto& file : audio_files) {
  if (file.sample_rate > max_sample_rate) {
    max_sample_rate = file.sample_rate;
  }
}

// Configure transport with max sample rate
config.sample_rate = max_sample_rate;
```

**Note:** Current SDK version (M2) doesn't resample - all clips must match transport sample rate. Future versions will add automatic resampling.

### Stop All Clips (Panic Button)

Essential for live performance - instantly stop all audio:

```cpp
transport->stopAllClips();
```

This is **sample-accurate** and **instant** - no fade-outs or delays.

## Platform Support

- **macOS:** CoreAudio driver (real-time audio output)
- **Windows:** Dummy driver (no audio output - WASAPI coming soon)
- **Linux:** Dummy driver (no audio output - ALSA/JACK coming soon)

## Performance Characteristics

**Tested on:** MacBook Pro M1 Pro, 16GB RAM, macOS 14.6

- **Latency:** ~10ms (512 sample buffer at 48kHz)
- **Max Simultaneous Clips:** 16+ (depends on CPU and file I/O)
- **CPU Usage:** <15% with 8 simultaneous clips
- **Memory Usage:** ~50MB + (clip_size × num_clips)

**Tips for low latency:**

- Use smaller buffer sizes (256 or 128 samples)
- Pre-load all clips before starting interactive loop
- Use SSD storage for faster file I/O
- Keep clip count reasonable (<20 clips for best performance)

## Limitations

- Maximum 9 clips (keyboard 1-9 limitation)
- No gain/fade controls per clip
- No loop support
- No clip groups or routing
- Simple blocking console I/O (not ideal for real-time)

For advanced features (routing, gain, fades, looping), see the **Orpheus Clip Composer** application.

## Troubleshooting

**Problem: "Failed to load <file>"**

- Check file exists and path is correct
- Verify file format is WAV/AIFF/FLAC
- Check file isn't corrupted (try opening in audio editor)

**Problem: Clips play at wrong speed**

- All clips must have same sample rate (48kHz recommended)
- Check with: `afinfo <file>` (macOS) or `mediainfo <file>` (Windows/Linux)
- Convert mismatched files: `ffmpeg -i input.wav -ar 48000 output.wav`

**Problem: No audio output**

- macOS: Check System Settings → Sound → Output
- macOS: Grant audio permissions in Privacy & Security
- Increase volume on audio interface
- Try with headphones to rule out speaker issues

**Problem: High CPU usage**

- Reduce number of simultaneous clips
- Use lower sample rate files (44.1kHz instead of 96kHz)
- Increase buffer size from 512 to 1024 samples
- Close other CPU-intensive applications

**Problem: "Failed to register clip"**

- Transport may be out of memory for clip slots
- Try with fewer clips
- Check clip file isn't corrupted

## Code Structure

The example follows this pattern:

1. **Load all audio files** - Validate and extract metadata
2. **Find max sample rate** - Determine transport configuration
3. **Create transport** - Initialize with config
4. **Register all clips** - Get handles for triggering
5. **Create audio driver** - Platform-specific (CoreAudio on macOS)
6. **Start audio callback** - Begin real-time processing
7. **Interactive loop** - Read keyboard input and trigger clips
8. **Clean up** - Stop all clips and audio driver

## Next Steps

**For more examples:**

- `simple_player` - Basic single-clip playback
- `offline_renderer` - Non-real-time rendering to WAV files

**For production-ready soundboard:**

- [Orpheus Clip Composer](../../apps/clip-composer/) - Professional soundboard with 960 clips, routing, GUI

**For SDK documentation:**

- [Quick Start Guide](../../docs/QUICK_START.md)
- [Transport Integration Guide](../../docs/integration/TRANSPORT_INTEGRATION.md)
- [API Reference](../../docs/api/html/index.html)

## Related Files

- `multi_clip_trigger.cpp` - Main source code
- `CMakeLists.txt` - Build configuration

---

**Last Updated:** October 26, 2025
**SDK Version:** M2 (Milestone 2)
