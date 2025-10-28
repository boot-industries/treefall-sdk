# Offline Renderer Example

**Purpose:** Demonstrates non-real-time rendering to WAV files

This example shows how to use the Orpheus SDK for offline (non-real-time) audio rendering. This is useful for batch processing, mixdowns, bounce-to-disk operations, and deterministic output generation.

## Features Demonstrated

- Offline (non-real-time) audio rendering
- Mixing multiple audio files to single output
- Custom sample rate and bit depth configuration
- Progress reporting during render
- Deterministic output (bit-identical across runs)
- WAV file writing with libsndfile

## Building

```bash
cd /path/to/orpheus-sdk
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DORPHEUS_BUILD_EXAMPLES=ON
cmake --build build --target offline_renderer
```

## Usage

```bash
./build/examples/offline_renderer/offline_renderer --output <output.wav> [options] <input1.wav> <input2.wav> ...
```

### Required Arguments

- `--output FILE` - Output WAV file path

### Optional Arguments

- `--duration SEC` - Duration in seconds (default: longest input)
- `--sample-rate HZ` - Sample rate in Hz (default: 48000)
- `--bit-depth N` - Bit depth: 16, 24, or 32 (default: 24)
- `--help` - Show help message

## Examples

### Render Single File

```bash
./offline_renderer --output processed.wav input.wav
```

### Mix Multiple Files

```bash
./offline_renderer --output mix.wav drums.wav bass.wav vocals.wav synth.wav
```

### Custom Sample Rate and Bit Depth

```bash
./offline_renderer \
  --output high_quality.wav \
  --sample-rate 96000 \
  --bit-depth 32 \
  input.wav
```

### Render Specific Duration

```bash
# Render only first 30 seconds
./offline_renderer --output excerpt.wav --duration 30 long_file.wav
```

### Batch Processing

```bash
# Process all WAV files in directory
for f in *.wav; do
  ./offline_renderer --output "processed_$f" "$f"
done
```

## Expected Output

```
Loading input files...
  Loaded: drums.wav (45.2s, 48000Hz, 2ch)
  Loaded: bass.wav (45.2s, 48000Hz, 2ch)
  Loaded: vocals.wav (40.1s, 48000Hz, 2ch)

Render configuration:
  Output:      mix.wav
  Duration:    45.2 seconds
  Sample rate: 48000 Hz
  Bit depth:   24 bit
  Input clips: 3

Rendering...
Progress: 5%
Progress: 10%
Progress: 15%
...
Progress: 95%
Progress: 100%

Render complete!
  Output file: mix.wav
  Duration:    45.2 seconds
  File size:   19.6 MB
```

## Key Concepts

### Offline vs Real-Time Rendering

**Real-Time:**

- Processes audio in small blocks (512 samples = 10ms at 48kHz)
- Must keep up with playback speed (can't run slower)
- Used for live playback, monitoring, performance

**Offline:**

- Processes audio as fast as CPU allows (no real-time constraint)
- Can run faster than real-time (2x, 5x, 10x speed)
- Used for mixdowns, bounce-to-disk, batch processing
- Deterministic output (always same result)

### Deterministic Output

One of the Orpheus SDK's core guarantees is **bit-identical output**:

```bash
# Render same input twice
./offline_renderer --output run1.wav input.wav
./offline_renderer --output run2.wav input.wav

# Files are byte-for-byte identical
diff run1.wav run2.wav  # No difference!
```

This is critical for:

- Automated testing
- Version control of audio
- Reproducible research
- Debugging audio algorithms

### Progress Callbacks

The `IOfflineRenderCallback` interface allows monitoring render progress:

```cpp
class RenderProgressCallback : public orpheus::IOfflineRenderCallback {
public:
  void onProgress(double progress_0_to_1, uint64_t frames_rendered,
                  uint64_t total_frames) override {
    int percent = static_cast<int>(progress_0_to_1 * 100.0);
    std::cout << "Progress: " << percent << "%\n";
  }
};
```

Useful for:

- CLI progress bars
- GUI progress dialogs
- Logging and monitoring
- Cancellation support (future feature)

### Sample Rate and Bit Depth

**Sample Rate Options:**

- **44100 Hz** - CD quality, standard audio
- **48000 Hz** - Professional audio, video post-production (recommended)
- **96000 Hz** - High-resolution audio, mastering
- **192000 Hz** - Ultra-high resolution (rarely needed, large files)

**Bit Depth Options:**

- **16-bit** - CD quality, smallest files, audible noise floor
- **24-bit** - Professional standard, excellent dynamic range (recommended)
- **32-bit** - Float or int, maximum headroom, large files

**File Size Formula:**

```
file_size_bytes = sample_rate × duration_seconds × num_channels × (bit_depth / 8)
file_size_MB = file_size_bytes / (1024 × 1024)
```

**Example:** 1 minute stereo at 48kHz/24-bit:

```
48000 × 60 × 2 × 3 = 17,280,000 bytes = 16.5 MB
```

## Use Cases

### Mixdown / Bounce to Disk

Combine multiple tracks into single stereo file:

```bash
./offline_renderer \
  --output final_mix.wav \
  --bit-depth 32 \
  track1.wav track2.wav track3.wav track4.wav
```

### Sample Rate Conversion

Convert audio to different sample rate:

```bash
./offline_renderer \
  --output converted.wav \
  --sample-rate 44100 \
  input_48k.wav
```

**Note:** Current SDK version (M2) doesn't resample - all inputs must match output sample rate. Future versions will add automatic resampling.

### Batch Processing

Process entire directories:

```bash
#!/bin/bash
mkdir -p processed
for file in raw/*.wav; do
  basename=$(basename "$file")
  ./offline_renderer --output "processed/$basename" "$file"
done
```

### Automated Testing

Generate reference files for testing:

```bash
./offline_renderer --output test_reference.wav test_input.wav
# Commit test_reference.wav to version control
# In tests: verify new render matches reference byte-for-byte
```

## Performance Characteristics

**Render Speed:** Depends on CPU, storage, and input complexity

Typical performance on modern hardware (M1 Pro, SSD):

- **Simple playback:** 10-20× real-time (10s file renders in 0.5-1s)
- **Complex mixing:** 5-10× real-time (multiple files, effects)
- **High sample rate:** 2-5× real-time (96kHz/192kHz)

**Memory Usage:**

- SDK transport: ~50MB baseline
- Per-clip overhead: ~10MB (file buffers)
- Output buffer: ~10-50MB (depends on render chunk size)

**Storage I/O:**

- Input: Sequential read (fast, well-cached)
- Output: Sequential write (fast)
- Use SSD for best performance

## Limitations

- No real-time playback monitoring (offline only)
- No gain/fade/effects controls (coming in future SDK versions)
- All inputs must have matching sample rate (no automatic resampling yet)
- Output is always stereo (mono/surround coming in future versions)
- No compression codecs (MP3/AAC/OGG) - WAV only

## Troubleshooting

**Problem: "Failed to load <file>"**

- Check file path is correct
- Verify file format is WAV/AIFF/FLAC
- Check file isn't corrupted

**Problem: Render is slow (<1× real-time)**

- Check CPU usage (close other apps)
- Use SSD storage instead of HDD
- Reduce sample rate (96kHz → 48kHz)
- Reduce bit depth (32-bit → 24-bit)

**Problem: Output file is huge**

- High sample rate (192kHz) creates large files
- Use 24-bit instead of 32-bit (25% smaller)
- Reduce duration if not needed
- Use 48kHz instead of 96kHz (50% smaller)

**Problem: "Sample rate mismatch"**

- All input files must have same sample rate
- Convert files first: `ffmpeg -i input.wav -ar 48000 output.wav`
- Or specify matching `--sample-rate`

**Problem: Output is silent**

- Check input files aren't silent
- Verify clips were registered successfully
- Check transport started clips correctly
- Increase input file gain in audio editor

**Problem: Output has clipping/distortion**

- Multiple clips summing together can exceed 0dBFS
- Reduce input gains before rendering
- Use 32-bit float output for maximum headroom
- Apply normalization/limiting in post-processing

## Code Structure

The example follows this pattern:

1. **Parse arguments** - Command-line options and input files
2. **Load input files** - Validate and extract metadata
3. **Create transport** - Initialize with render config
4. **Register clips** - Add all input files to transport
5. **Start clips** - Begin playback at time 0
6. **Render offline** - Process audio to output file
7. **Report results** - Show completion status and file size

## Next Steps

**For more examples:**

- `simple_player` - Basic real-time playback
- `multi_clip_trigger` - Interactive multi-clip triggering

**For production rendering:**

- Add gain/fade controls (SDK API available)
- Implement custom effects processing
- Add metadata tagging (BWF, iXML)
- Support batch processing with job queues

**For SDK documentation:**

- [Quick Start Guide](../../docs/QUICK_START.md)
- [Offline Rendering Guide](../../docs/integration/OFFLINE_RENDERING.md)
- [API Reference](../../docs/api/html/index.html)

## Related Files

- `offline_renderer.cpp` - Main source code
- `CMakeLists.txt` - Build configuration

---

**Last Updated:** October 26, 2025
**SDK Version:** M2 (Milestone 2)
