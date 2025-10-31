# OCC022 Clip Metadata Schema v1.0

**Document Version:** 1.0
**Date:** October 12, 2025
**Status:** Draft
**Supersedes:** None (extends OCC009 Session Metadata Manifest)

---

## Executive Summary

This document defines the complete metadata schema for individual audio clips in Orpheus Clip Composer. Each clip is more than just an audio file reference—it's a performance-ready cue with trimming, DSP, routing, visual appearance, and playback behavior fully specified.

**Design Philosophy:**

- **Sample-accurate determinism** - Same clip data produces identical playback every time
- **Portability** - Relative paths and human-readable JSON enable session migration
- **Extensibility** - Schema accommodates future features without breaking compatibility
- **SpotOn-inspired** - Incorporates proven workflows from professional broadcast tools

---

## 1. Clip Identity & Core References

### 1.1 Unique Identification

```json
{
  "identity": {
    "uuid": "550e8400-e29b-41d4-a716-446655440000",
    "name": "Intro Music",
    "created_at": "2025-10-12T14:32:00Z",
    "modified_at": "2025-10-12T15:45:00Z"
  }
}
```

**Fields:**

- `uuid` (string, required) - RFC 4122 UUID v4 for globally unique identification
- `name` (string, required) - User-facing clip name (displayed on button)
- `created_at` (ISO 8601 timestamp) - Creation timestamp
- `modified_at` (ISO 8601 timestamp) - Last modification timestamp

**Rationale:**

- UUIDs enable reliable clip tracking across sessions and migrations
- Timestamps support change tracking and session history

### 1.2 Audio File Reference

```json
{
  "audio": {
    "file_path": "media/intro_music.wav",
    "file_path_absolute": "/Users/studio/projects/show_2025/media/intro_music.wav",
    "format": "WAV",
    "sample_rate": 48000,
    "bit_depth": 24,
    "channels": 2,
    "duration_samples": 2304000,
    "duration_seconds": 48.0,
    "file_hash": "sha256:a3b5c7d9e1f2a4b6c8d0e2f4a6b8c0d2e4f6a8b0c2d4e6f8a0b2c4d6e8f0a2b4"
  }
}
```

**Fields:**

- `file_path` (string, required) - Relative path from session file location
- `file_path_absolute` (string, optional) - Last known absolute path (for recovery)
- `format` (string) - Audio format (WAV, AIFF, FLAC, MP3, AAC, OGG)
- `sample_rate` (integer) - Native sample rate in Hz
- `bit_depth` (integer) - Bit depth (16, 24, 32)
- `channels` (integer) - Channel count (1=mono, 2=stereo, etc.)
- `duration_samples` (integer) - Total duration in samples at native sample rate
- `duration_seconds` (float) - Total duration in seconds (for UI display)
- `file_hash` (string) - SHA-256 hash for file verification

**Rationale:**

- **Relative paths** enable session portability across systems
- **File hash** enables intelligent file recovery when paths break
- **Sample-accurate duration** supports deterministic timing calculations
- **Format metadata** enables proper decoder selection

---

## 2. Edit Points & Trimming

### 2.1 Trim In/Out Points (SpotOn-Inspired)

```json
{
  "edit_points": {
    "trim_in_samples": 4800,
    "trim_out_samples": 2299200,
    "trim_in_seconds": 0.1,
    "trim_out_seconds": 47.9,
    "cue_points": [
      {
        "id": "cue_001",
        "name": "Verse Start",
        "position_samples": 576000,
        "position_seconds": 12.0,
        "color": "#FF5733"
      }
    ],
    "autotrim_enabled": false,
    "autotrim_threshold_db": -20.0
  }
}
```

**Fields:**

- `trim_in_samples` (integer, default: 0) - Playback start point in samples
- `trim_out_samples` (integer, default: duration_samples) - Playback end point in samples
- `trim_in_seconds` (float) - Trim in time (derived, for UI display)
- `trim_out_seconds` (float) - Trim out time (derived, for UI display)
- `cue_points` (array) - Up to 4 internal cue markers (Control+click in editor)
- `autotrim_enabled` (boolean) - AutoTrim feature active (SpotOn-inspired)
- `autotrim_threshold_db` (float) - dB threshold for AutoTrim detection

**Cue Point Schema:**

- `id` (string) - Unique identifier within clip
- `name` (string) - User-facing label
- `position_samples` (integer) - Sample-accurate position
- `position_seconds` (float) - Time position (derived)
- `color` (hex color) - Visual marker color

**Rationale:**

- **Sample values are authoritative** - seconds are derived for display
- **AutoTrim** automates trim point detection (SpotOn SPT023 feature)
- **Cue points** enable rehearsal mode and quick navigation

### 2.2 Fade Configuration

```json
{
  "fades": {
    "fade_in": {
      "enabled": true,
      "duration_samples": 2400,
      "duration_seconds": 0.05,
      "curve": "linear"
    },
    "fade_out": {
      "enabled": true,
      "duration_samples": 4800,
      "duration_seconds": 0.1,
      "curve": "exponential"
    }
  }
}
```

**Fields:**

- `enabled` (boolean) - Fade active
- `duration_samples` (integer) - Fade length in samples
- `duration_seconds` (float) - Fade length in seconds (derived)
- `curve` (enum) - Fade shape: `linear`, `exponential`, `logarithmic`, `s_curve`

**Rationale:**

- **SpotOn-inspired granular fade control** (SPT023)
- Separate in/out fade configurations for asymmetric fades
- Sample-accurate fade lengths for deterministic rendering

---

## 3. DSP & Processing

### 3.1 Time-Stretch & Pitch-Shift

```json
{
  "dsp": {
    "time_stretch": {
      "enabled": false,
      "ratio": 1.0,
      "algorithm": "rubberband",
      "preserve_formants": true
    },
    "pitch_shift": {
      "enabled": false,
      "semitones": 0.0,
      "cents": 0.0,
      "algorithm": "rubberband"
    },
    "normalization": {
      "enabled": false,
      "target_lufs": -23.0,
      "peak_limit_db": -1.0
    }
  }
}
```

**Fields:**

**Time-Stretch:**

- `enabled` (boolean)
- `ratio` (float) - Playback speed multiplier (0.5 = half speed, 2.0 = double speed)
- `algorithm` (enum) - `rubberband`, `soundtouch`, `sonic`
- `preserve_formants` (boolean) - Maintain vocal quality

**Pitch-Shift:**

- `enabled` (boolean)
- `semitones` (float) - Pitch shift in semitones (-12 to +12)
- `cents` (float) - Fine pitch adjustment (-100 to +100)
- `algorithm` (enum) - DSP library selection

**Normalization:**

- `enabled` (boolean)
- `target_lufs` (float) - Target loudness (EBU R128 standard: -23 LUFS)
- `peak_limit_db` (float) - Peak ceiling to prevent clipping

**Rationale:**

- Professional broadcast environments require loudness normalization
- Real-time DSP enables live tempo adjustments (performer use case)
- Algorithm selection enables quality/performance trade-offs

---

## 4. Playback Behavior

### 4.1 Clip Group & Routing

```json
{
  "playback": {
    "clip_group": "A",
    "output_routing": "default",
    "loop_enabled": false,
    "stop_others_on_trigger": true,
    "fifo_choke": true,
    "volume_db": 0.0,
    "pan": 0.0
  }
}
```

**Fields:**

- `clip_group` (enum) - `A`, `B`, `C`, `D` - determines output routing and FIFO behavior
- `output_routing` (string) - "default" uses group routing, or custom route name
- `loop_enabled` (boolean) - Loop playback continuously
- `stop_others_on_trigger` (boolean) - Stop other clips in same group when triggered
- `fifo_choke` (boolean) - First-in-first-out choke mode (only one clip plays at a time)
- `volume_db` (float) - Clip-level volume adjustment (-60.0 to +12.0 dB)
- `pan` (float) - Stereo pan position (-1.0 = full left, 0.0 = center, +1.0 = full right)

**Rationale:**

- **Clip groups** provide flexible routing and choke behavior
- **FIFO choke** prevents overlapping playback (broadcast use case)
- **Per-clip volume/pan** enables quick mix adjustments without editing audio

### 4.2 Master/Slave Linking (SpotOn-Inspired)

```json
{
  "linking": {
    "is_master": false,
    "master_clip_uuid": null,
    "slave_clip_uuids": [],
    "sync_mode": "sequential"
  }
}
```

**Fields:**

- `is_master` (boolean) - This clip controls linked slaves
- `master_clip_uuid` (string, nullable) - UUID of master clip (if this is a slave)
- `slave_clip_uuids` (array of strings) - UUIDs of linked slave clips
- `sync_mode` (enum) - `sequential`, `simultaneous`

**Rationale:**

- **SpotOn master/slave linking** (SPT023) enables coordinated cue triggering
- Sequential mode triggers slaves in order after master completes
- Simultaneous mode triggers all linked clips together

### 4.3 AutoPlay & Sequential Playback (SpotOn-Inspired)

```json
{
  "autoplay": {
    "enabled": false,
    "next_clip_uuid": null,
    "next_clip_delay_samples": 0
  }
}
```

**Fields:**

- `enabled` (boolean) - AutoPlay/Play Next active
- `next_clip_uuid` (string, nullable) - Next clip to trigger on completion
- `next_clip_delay_samples` (integer) - Gap between clips (0 = seamless)

**Rationale:**

- **SpotOn AutoPlay/jukebox mode** (SPT023) for continuous playout
- Sample-accurate gap control for seamless transitions

---

## 5. UI & Visual Appearance

### 5.1 Grid Position & Layout

```json
{
  "ui": {
    "grid_position": {
      "tab": 1,
      "row": 0,
      "col": 0,
      "span_rows": 1,
      "span_cols": 2
    },
    "appearance": {
      "color": "#3498db",
      "label": "Intro Music",
      "font_size": 14,
      "icon": null
    }
  }
}
```

**Fields:**

**Grid Position:**

- `tab` (integer, 1-8) - Tab page number
- `row` (integer, 0-9) - Grid row (10 rows per tab)
- `col` (integer, 0-11) - Grid column (12 columns per tab)
- `span_rows` (integer, 1-4) - Vertical stretch (buttons can occupy up to 4 cells)
- `span_cols` (integer, 1-4) - Horizontal stretch

**Appearance:**

- `color` (hex color) - Button background color
- `label` (string) - Display text (may differ from clip name)
- `font_size` (integer) - Text size in points
- `icon` (string, nullable) - Path to custom icon image

**Rationale:**

- **Button stretching** enables visual hierarchy and ergonomics
- **Customizable appearance** supports color-coded workflows (SpotOn SPT023)
- **Separate label from name** allows technical names vs. display labels

### 5.2 Waveform Display Cache

```json
{
  "waveform_cache": {
    "enabled": true,
    "cache_path": ".cache/waveforms/550e8400-e29b-41d4-a716-446655440000.png",
    "resolution": "1024x128",
    "last_generated": "2025-10-12T15:00:00Z"
  }
}
```

**Fields:**

- `enabled` (boolean) - Use pre-rendered waveform
- `cache_path` (string) - Relative path to cached waveform image
- `resolution` (string) - Image dimensions
- `last_generated` (timestamp) - Cache validity tracking

**Rationale:**

- Waveform rendering is expensive; caching improves UI performance
- Cache invalidated when audio file or trim points change

---

## 6. Metadata & Tags

### 6.1 User Metadata

```json
{
  "metadata": {
    "tags": ["intro", "upbeat", "morning_show"],
    "notes": "Use this for Monday morning show opener",
    "rating": 5,
    "color_category": "blue"
  }
}
```

**Fields:**

- `tags` (array of strings) - User-defined tags for searching/filtering
- `notes` (string) - Freeform text notes
- `rating` (integer, 1-5) - User rating
- `color_category` (string) - Color-coded category

**Rationale:**

- Supports advanced search and organization
- Future AI auto-tagging can populate tags field

### 6.2 Recording Metadata (for clips recorded directly into buttons)

```json
{
  "recording": {
    "was_recorded_live": true,
    "recorded_at": "2025-10-12T10:30:00Z",
    "recorded_by": "operator_01",
    "input_source": "Record_A",
    "ltc_timecode": "10:30:00:00"
  }
}
```

**Fields:**

- `was_recorded_live` (boolean) - Clip was recorded via OCC's record feature
- `recorded_at` (timestamp) - Recording timestamp
- `recorded_by` (string) - User identifier
- `input_source` (string) - Input channel used (Record_A, Record_B)
- `ltc_timecode` (string) - LTC timecode if available

**Rationale:**

- **Direct recording into buttons** is key workflow (OCC021)
- Metadata enables logging and archival requirements

---

## 7. Performance & State Tracking

### 7.1 Playback Statistics

```json
{
  "statistics": {
    "play_count": 47,
    "last_played_at": "2025-10-12T14:00:00Z",
    "total_play_duration_seconds": 2256.0,
    "error_count": 0,
    "last_error": null
  }
}
```

**Fields:**

- `play_count` (integer) - Times clip has been triggered
- `last_played_at` (timestamp) - Most recent playback
- `total_play_duration_seconds` (float) - Cumulative play time
- `error_count` (integer) - Playback failures
- `last_error` (string, nullable) - Most recent error message

**Rationale:**

- Usage statistics inform session logs
- Error tracking aids diagnostics

---

## 8. Complete Example Schema

```json
{
  "identity": {
    "uuid": "550e8400-e29b-41d4-a716-446655440000",
    "name": "Morning Show Intro",
    "created_at": "2025-10-12T14:32:00Z",
    "modified_at": "2025-10-12T15:45:00Z"
  },
  "audio": {
    "file_path": "media/morning_intro.wav",
    "file_path_absolute": "/Users/studio/show/media/morning_intro.wav",
    "format": "WAV",
    "sample_rate": 48000,
    "bit_depth": 24,
    "channels": 2,
    "duration_samples": 2304000,
    "duration_seconds": 48.0,
    "file_hash": "sha256:a3b5c7d9e1f2a4b6c8d0e2f4a6b8c0d2e4f6a8b0c2d4e6f8a0b2c4d6e8f0a2b4"
  },
  "edit_points": {
    "trim_in_samples": 4800,
    "trim_out_samples": 2299200,
    "trim_in_seconds": 0.1,
    "trim_out_seconds": 47.9,
    "cue_points": [
      {
        "id": "cue_001",
        "name": "Verse Start",
        "position_samples": 576000,
        "position_seconds": 12.0,
        "color": "#FF5733"
      }
    ],
    "autotrim_enabled": false,
    "autotrim_threshold_db": -20.0
  },
  "fades": {
    "fade_in": {
      "enabled": true,
      "duration_samples": 2400,
      "duration_seconds": 0.05,
      "curve": "linear"
    },
    "fade_out": {
      "enabled": true,
      "duration_samples": 4800,
      "duration_seconds": 0.1,
      "curve": "exponential"
    }
  },
  "dsp": {
    "time_stretch": {
      "enabled": false,
      "ratio": 1.0,
      "algorithm": "rubberband",
      "preserve_formants": true
    },
    "pitch_shift": {
      "enabled": false,
      "semitones": 0.0,
      "cents": 0.0,
      "algorithm": "rubberband"
    },
    "normalization": {
      "enabled": false,
      "target_lufs": -23.0,
      "peak_limit_db": -1.0
    }
  },
  "playback": {
    "clip_group": "A",
    "output_routing": "default",
    "loop_enabled": false,
    "stop_others_on_trigger": true,
    "fifo_choke": true,
    "volume_db": 0.0,
    "pan": 0.0
  },
  "linking": {
    "is_master": false,
    "master_clip_uuid": null,
    "slave_clip_uuids": [],
    "sync_mode": "sequential"
  },
  "autoplay": {
    "enabled": false,
    "next_clip_uuid": null,
    "next_clip_delay_samples": 0
  },
  "ui": {
    "grid_position": {
      "tab": 1,
      "row": 0,
      "col": 0,
      "span_rows": 1,
      "span_cols": 2
    },
    "appearance": {
      "color": "#3498db",
      "label": "Intro Music",
      "font_size": 14,
      "icon": null
    }
  },
  "waveform_cache": {
    "enabled": true,
    "cache_path": ".cache/waveforms/550e8400-e29b-41d4-a716-446655440000.png",
    "resolution": "1024x128",
    "last_generated": "2025-10-12T15:00:00Z"
  },
  "metadata": {
    "tags": ["intro", "upbeat", "morning_show"],
    "notes": "Use this for Monday morning show opener",
    "rating": 5,
    "color_category": "blue"
  },
  "recording": {
    "was_recorded_live": true,
    "recorded_at": "2025-10-12T10:30:00Z",
    "recorded_by": "operator_01",
    "input_source": "Record_A",
    "ltc_timecode": "10:30:00:00"
  },
  "statistics": {
    "play_count": 47,
    "last_played_at": "2025-10-12T14:00:00Z",
    "total_play_duration_seconds": 2256.0,
    "error_count": 0,
    "last_error": null
  }
}
```

---

## 9. Orpheus SDK Integration Points

**What OCC provides to Orpheus SDK:**

- Clip metadata structure for SessionGraph
- Sample-accurate timing requirements
- DSP processing requirements (time-stretch, pitch-shift, normalization)
- Routing matrix requirements

**What OCC needs from Orpheus SDK:**

- `SessionGraph` to hold clip references and transport state
- `AudioFileReader` abstraction for multi-format decoding
- `TransportController` for sample-accurate playback
- `RoutingMatrix` for flexible output routing
- `AudioProcessor` interface for DSP plugin architecture

**Schema Evolution Strategy:**

- SDK remains format-agnostic (accepts JSON metadata via adapter)
- OCC owns JSON schema definition
- Future applications can extend schema without SDK changes

---

## 10. Validation & Error Handling

### 10.1 Required Field Validation

```javascript
REQUIRED: -identity.uuid -
  identity.name -
  audio.file_path -
  audio.format -
  audio.sample_rate -
  audio.duration_samples -
  playback.clip_group -
  ui.grid_position(tab, row, col);
```

### 10.2 Constraint Validation

```javascript
CONSTRAINTS:
- trim_in_samples >= 0
- trim_out_samples <= duration_samples
- trim_in_samples < trim_out_samples
- fade durations <= (trim_out - trim_in)
- clip_group in ['A', 'B', 'C', 'D']
- tab in [1..8]
- row in [0..9]
- col in [0..11]
- span_rows in [1..4]
- span_cols in [1..4]
```

### 10.3 Error Recovery

- **Missing file_path**: Use file_hash to search for relocated files
- **Invalid trim points**: Reset to defaults (0, duration_samples)
- **Missing DSP algorithm**: Fallback to default (rubberband)
- **Invalid grid position**: Place in first available slot

---

## 11. Versioning & Migration

### 11.1 Schema Version

```json
{
  "schema_version": "1.0",
  "compatibility": {
    "min_occ_version": "0.1.0",
    "max_occ_version": null
  }
}
```

### 11.2 Migration Strategy

- **Backward compatibility**: New fields optional, defaults provided
- **Forward compatibility**: Unknown fields preserved (pass-through)
- **Migration scripts**: Automate schema upgrades for major versions

---

## 12. Related Documents

- **OCC021** - Orpheus Clip Composer Product Vision (authoritative)
- **OCC009** - Session Metadata Manifest (parent document)
- **OCC011** - Wireframes v2 (UI context)
- **SPT023** - Comprehensive SpotOn Report (inspiration for features)
- **MOV001** - Merging Ovation Analysis (professional reference)

---

## 13. Open Questions for Resolution

1. **DSP Plugin Architecture** - Should we support VST3 plugins per-clip? (v2.0 decision)
2. **Sample Rate Conversion** - Automatic or manual when clip sample rate != session sample rate?
3. **Stereo vs. Multichannel** - How to represent 5.1, 7.1, Atmos clips? (future expansion)
4. **Metadata Extensibility** - Custom user-defined metadata fields? JSON blob?

---

**Document Status:** Ready for review and implementation planning.
