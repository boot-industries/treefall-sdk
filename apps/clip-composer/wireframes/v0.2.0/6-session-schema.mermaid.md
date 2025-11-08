%% Session Schema for Orpheus Clip Composer v0.2.0
%% Shows JSON session file structure with clip metadata, routing, and preferences
%% Last Updated: October 31, 2025

classDiagram
    class SessionFile {
        +File Format: .occSession (JSON)
        +Encoding: UTF-8
        +Version: 1.0.0
        +Location: ~/Documents/Orpheus Clip Composer/Sessions/
    }

    class SessionRoot {
        +sessionMetadata: SessionMetadata
        +clips: ClipMetadata[]
        +routing: RoutingConfiguration
        +preferences: SessionPreferences
    }

    class SessionMetadata {
        +string name
        +string version "1.0.0"
        +string createdDate (ISO 8601)
        +string modifiedDate (ISO 8601)
        +string author
        +string description
        +int sampleRate (44100, 48000, 96000)
        +int bufferSize (default: 512)
    }

    class ClipMetadata {
        +int handle (1-960)
        +string name
        +string filePath (absolute)
        +int buttonIndex (0-119)
        +int tabIndex (0-7)
        +int clipGroup (0-3)
        +int64 trimIn (samples)
        +int64 trimOut (samples)
        +float gainDb (-48.0 to +12.0)
        +string color (hex "#RRGGBB")
        +int fadeInSamples (default: 0)
        +int fadeOutSamples (default: 0)
        +string fadeInCurve (Linear, Exponential, Logarithmic)
        +string fadeOutCurve (Linear, Exponential, Logarithmic)
        +bool loopEnabled (default: false)
        +string playbackMode (OneShot, Loop)
        +int64 loopStart (samples)
        +int64 loopEnd (samples)
        +bool stopOthersOnPlay (default: false)
        +CuePoint[] cuePoints
    }

    class CuePoint {
        +string name
        +int64 position (samples)
        +string color (hex "#RRGGBB")
    }

    class RoutingConfiguration {
        +ClipGroup[4] clipGroups
        +float masterGain (dB)
        +bool masterMute
    }

    class ClipGroup {
        +string name
        +float gainDb (dB)
        +bool mute
        +bool solo
    }

    class SessionPreferences {
        +bool autoSave (default: true)
        +int autoSaveInterval (seconds, default: 300)
        +bool showWaveforms (default: true)
        +string colorScheme (Dark, Light)
        +string audioDriver (CoreAudio, WASAPI, ASIO)
        +string audioDevice (device name)
        +int audioBufferSize (128, 256, 512, 1024)
        +int audioSampleRate (44100, 48000, 96000)
        +bool showPerformanceMonitor (default: false)
        +int uiRefreshRate (30, 60, 75, 120 fps)
    }

    class FilePathResolution {
        +Absolute Paths: /Users/username/Audio/clip.wav
        +Relative Paths: ./audio/clip.wav (relative to session file)
        +Missing Files: Mark as "Missing File", allow relocation
        +Portability: Use forward slashes, convert on load
    }

    class VersionMigration {
        +v1.0.0: Initial schema
        +v1.1.0: Add stopOthersOnPlay field (future)
        +v2.0.0: Add DSP effects metadata (future)
        +Migration Strategy: Backward compatible (old → new)
        +Validation: JSON schema validation on load
    }

    SessionFile --> SessionRoot
    SessionRoot --> SessionMetadata
    SessionRoot --> ClipMetadata : contains 0-960
    SessionRoot --> RoutingConfiguration
    SessionRoot --> SessionPreferences

    ClipMetadata --> CuePoint : contains 0-N

    RoutingConfiguration --> ClipGroup : contains 4

    SessionRoot ..> FilePathResolution : uses
    SessionRoot ..> VersionMigration : supports

    style SessionFile fill:#ccddff,stroke:#0066ff,stroke-width:2px
    style SessionRoot fill:#ccffcc,stroke:#00ff00,stroke-width:2px
    style ClipMetadata fill:#ffeecc,stroke:#ff9900,stroke-width:2px
    style VersionMigration fill:#ffcccc,stroke:#ff0000,stroke-width:2px
