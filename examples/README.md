# Orpheus SDK Examples

**Learn by doing** - Practical code examples demonstrating Orpheus SDK features

This directory contains example applications that demonstrate how to use the Orpheus SDK for various audio processing tasks. Each example is a complete, working application with detailed documentation.

## Quick Start

### Build All Examples

```bash
cd /path/to/orpheus-sdk
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DORPHEUS_BUILD_EXAMPLES=ON
cmake --build build
```

### Run an Example

```bash
# Simple playback
./build/examples/simple_player/simple_player audio.wav

# Interactive triggering
./build/examples/multi_clip_trigger/multi_clip_trigger sound1.wav sound2.wav sound3.wav

# Offline rendering
./build/examples/offline_renderer/offline_renderer --output mix.wav input1.wav input2.wav
```

## Available Examples

### 1. Simple Clip Player (`simple_player/`)

**Purpose:** Learn the basics of SDK audio playback

**What it demonstrates:**

- Opening audio files
- Creating transport controller
- Registering clips
- Starting playback
- Basic audio callback implementation

**Complexity:** ⭐ Beginner
**Code size:** ~150 lines
**Run time:** Single file playback to completion

[View Documentation →](simple_player/README.md)

---

### 2. Multi-Clip Trigger (`multi_clip_trigger/`)

**Purpose:** Build a simple soundboard with real-time triggering

**What it demonstrates:**

- Loading multiple audio files
- Interactive keyboard control (1-9 keys)
- Playing multiple clips simultaneously
- Mixing audio in real-time
- Stop all clips (panic button)

**Complexity:** ⭐⭐ Intermediate
**Code size:** ~300 lines
**Run time:** Interactive session (until user quits)

[View Documentation →](multi_clip_trigger/README.md)

---

### 3. Offline Renderer (`offline_renderer/`)

**Purpose:** Render audio files without real-time constraints

**What it demonstrates:**

- Non-real-time (offline) rendering
- Mixing multiple files to single output
- Progress reporting
- Deterministic output (bit-identical)
- Custom sample rate and bit depth
- Command-line argument parsing

**Complexity:** ⭐⭐ Intermediate
**Code size:** ~250 lines
**Run time:** Fast as CPU allows (faster than real-time)

[View Documentation →](offline_renderer/README.md)

---

## Example Comparison

| Feature                 | simple_player | multi_clip_trigger | offline_renderer |
| ----------------------- | ------------- | ------------------ | ---------------- |
| **Single clip**         | ✅            | ✅                 | ✅               |
| **Multiple clips**      | ❌            | ✅                 | ✅               |
| **Real-time playback**  | ✅            | ✅                 | ❌               |
| **Interactive control** | ❌            | ✅                 | ❌               |
| **Offline rendering**   | ❌            | ❌                 | ✅               |
| **Audio output**        | Yes           | Yes                | File only        |
| **Platform support**    | macOS         | macOS              | All              |
| **Complexity**          | Low           | Medium             | Medium           |

## Learning Path

### Beginner Path

1. **Start here:** `simple_player`
   - Learn basic SDK concepts
   - Understand transport and audio callbacks
   - Get comfortable with error handling

2. **Next:** `multi_clip_trigger`
   - Add interactive control
   - Learn multi-clip mixing
   - Understand real-time constraints

3. **Then:** `offline_renderer`
   - Learn non-real-time processing
   - Understand deterministic output
   - Practice batch processing

### Use Case Path

**Building a soundboard?** → Start with `multi_clip_trigger`
**Doing batch processing?** → Start with `offline_renderer`
**Just learning the SDK?** → Start with `simple_player`

## Common Code Patterns

### 1. Opening an Audio File

```cpp
auto reader = orpheus::createAudioFileReader();
auto result = reader->open(file_path);

if (!result.isOk()) {
  std::cerr << "Error: " << result.errorMessage << std::endl;
  return 1;
}

const auto& metadata = *result;
std::cout << "Duration: " << metadata.durationSeconds() << " seconds\n";
```

### 2. Creating Transport Controller

```cpp
orpheus::TransportConfig config;
config.sample_rate = 48000;
config.buffer_size = 512;
config.num_outputs = 2;

auto transport = orpheus::createTransportController(nullptr, config.sample_rate);

if (transport->initialize(config) != orpheus::SessionGraphError::OK) {
  std::cerr << "Failed to initialize transport\n";
  return 1;
}
```

### 3. Registering a Clip

```cpp
orpheus::ClipRegistration clip_reg;
clip_reg.audio_file_path = "/path/to/audio.wav";
clip_reg.trim_in_samples = 0;
clip_reg.trim_out_samples = metadata.duration_samples;

auto clip_handle = transport->registerClipAudio(clip_reg);

if (!clip_handle.isValid()) {
  std::cerr << "Failed to register clip\n";
  return 1;
}
```

### 4. Audio Callback (Real-Time)

```cpp
class SimpleAudioCallback : public orpheus::IAudioCallback {
public:
  explicit SimpleAudioCallback(orpheus::ITransportController* transport)
      : transport_(transport) {}

  void processAudio(const float** inputs, float** outputs,
                    size_t num_channels, size_t num_frames) override {
    transport_->processAudio(outputs, num_channels, num_frames);
  }

private:
  orpheus::ITransportController* transport_;
};
```

### 5. Starting Playback

```cpp
// Start clip at position 0
if (transport->startClip(clip_handle, 0) != orpheus::SessionGraphError::OK) {
  std::cerr << "Failed to start clip\n";
  return 1;
}
```

## Building and Testing

### Build Single Example

```bash
cmake --build build --target simple_player
cmake --build build --target multi_clip_trigger
cmake --build build --target offline_renderer
```

### Test Examples

```bash
# Test simple_player with provided fixture
./build/examples/simple_player/simple_player tools/fixtures/test.wav

# Test multi_clip_trigger with multiple files
./build/examples/multi_clip_trigger/multi_clip_trigger \
  tools/fixtures/clip1.wav \
  tools/fixtures/clip2.wav

# Test offline_renderer
./build/examples/offline_renderer/offline_renderer \
  --output /tmp/test_render.wav \
  tools/fixtures/test.wav
```

## Platform Support

- **macOS:** All examples fully functional (CoreAudio driver)
- **Windows:** Real-time examples use dummy driver (no audio output - WASAPI coming soon)
- **Linux:** Real-time examples use dummy driver (no audio output - ALSA/JACK coming soon)

**Offline renderer works on all platforms** - it writes to WAV files, no audio driver needed.

## Supported Audio Formats

All examples support:

- **WAV** (PCM, IEEE float)
- **AIFF** (PCM)
- **FLAC** (lossless)

Format detection is automatic - no format-specific code needed.

## Example Modifications

### Customize Simple Player

**Add gain control:**

```cpp
// In ClipRegistration
clip_reg.gain_db = -6.0;  // Reduce by 6dB
```

**Add fade out:**

```cpp
clip_reg.fade_out_seconds = 2.0;  // 2-second fade out
```

### Customize Multi-Clip Trigger

**Increase clip capacity:**

```cpp
// Change loop condition from i <= 9 to i <= 16
for (int i = 1; i < argc && i <= 16; ++i) {
```

**Add gain per clip:**

```cpp
std::cout << "Enter gain (dB) for " << filename << ": ";
double gain_db;
std::cin >> gain_db;
clip_reg.gain_db = gain_db;
```

### Customize Offline Renderer

**Add normalization:**

```cpp
// After render completes, analyze and apply gain
auto peak = analyzePeak(output_file);
auto gain = calculateNormalizationGain(peak);
applyGain(output_file, gain);
```

**Add metadata:**

```cpp
// Use libsndfile to add BWF metadata
SF_INFO info;
sf_command(output_file, SFC_SET_BROADCAST_INFO, &bext, sizeof(bext));
```

## Troubleshooting

### Build Issues

**Problem: `ORPHEUS_BUILD_EXAMPLES` not recognized**

```bash
# Reconfigure CMake
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DORPHEUS_BUILD_EXAMPLES=ON
```

**Problem: Examples don't build on Windows/Linux**

- Real-time examples require audio drivers
- Use `offline_renderer` which works on all platforms
- WASAPI (Windows) and ALSA/JACK (Linux) drivers coming in future SDK releases

### Runtime Issues

**Problem: "Failed to open audio file"**

- Check file path is correct (use absolute paths if unsure)
- Verify file format is WAV/AIFF/FLAC
- Test file in audio editor first

**Problem: No audio output**

- macOS: Check System Settings → Sound → Output
- macOS: Grant audio permissions in Privacy & Security
- Windows/Linux: Use dummy driver (no output) or use `offline_renderer`

**Problem: Audio glitches**

- Increase buffer size from 512 to 1024 in TransportConfig
- Close other audio applications
- Check CPU usage isn't near 100%

## Next Steps

### Learn More

**API Documentation:**

- [Quick Start Guide](../docs/QUICK_START.md)
- [Transport Integration Guide](../docs/integration/TRANSPORT_INTEGRATION.md)
- [Cross-Platform Development](../docs/platform/CROSS_PLATFORM.md)
- [API Reference](../docs/api/html/index.html)

**SDK Architecture:**

- [Architecture Overview](../ARCHITECTURE.md)
- [Determinism Guarantees](../docs/DETERMINISM.md)
- [Threading Model](../docs/integration/TRANSPORT_INTEGRATION.md#threading-model)

### Build Your Own

Start with one of these examples and modify it:

1. **Custom soundboard** - Base on `multi_clip_trigger`, add GUI with JUCE
2. **Audio analysis tool** - Base on `offline_renderer`, add FFT/metering
3. **Session player** - Base on `simple_player`, load JSON sessions
4. **Batch processor** - Base on `offline_renderer`, add directory scanning

### Contributing

Found a bug or have an improvement? See [CONTRIBUTING.md](../docs/CONTRIBUTING.md) for guidelines.

---

## Example Statistics

| Example            | Files | Lines    | Binary Size | Build Time |
| ------------------ | ----- | -------- | ----------- | ---------- |
| simple_player      | 3     | ~150     | ~50KB       | <5s        |
| multi_clip_trigger | 3     | ~300     | ~60KB       | <5s        |
| offline_renderer   | 3     | ~250     | ~60KB       | <5s        |
| **Total**          | **9** | **~700** | **~170KB**  | **<15s**   |

All examples built in < 15 seconds on modern hardware (M1 Pro, SSD).

---

**Last Updated:** October 26, 2025
**SDK Version:** M2 (Milestone 2)
**Maintained By:** SDK Core Team
