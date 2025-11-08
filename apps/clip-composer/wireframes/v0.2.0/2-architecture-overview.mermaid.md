%% Architecture Overview for Orpheus Clip Composer v0.2.0
%% Shows 5-layer architecture, threading model, and component responsibilities
%% Last Updated: October 31, 2025

flowchart TD
    subgraph "Thread: Message Thread (UI, I/O, Callbacks)"
        subgraph "Layer 1: JUCE UI Components"
            ClipGrid["ClipGrid<br/>(10×12 buttons × 8 tabs = 960 clips)<br/>Keyboard shortcuts, drag-drop, color coding"]
            WaveformDisplay["WaveformDisplay<br/>(Visual waveform, playhead, trim markers)<br/>Click-to-jog, zoom controls"]
            TransportControls["TransportControls<br/>(Play/Pause/Stop All/Panic)<br/>Loop mode, transport position"]
            ClipEditDialog["ClipEditDialog<br/>(Trim IN/OUT, fade controls, gain)<br/>Time counter, nudge buttons, keyboard shortcuts"]
            RoutingPanel["RoutingPanel<br/>(4 Clip Groups, group gain, mute/solo)<br/>Master output controls"]
            PerformanceMonitor["PerformanceMonitor<br/>(CPU usage, latency, buffer stats)<br/>Diagnostics display)"]
            TabSwitcher["TabSwitcher<br/>(8-tab navigation)<br/>Keyboard shortcuts 1-8"]
        end

        subgraph "Layer 2: Application Logic"
            SessionManager["SessionManager<br/>(Load/save JSON sessions)<br/>Metadata validation, file path resolution<br/>Version migration"]
            ClipManager["ClipManager<br/>(Track clip metadata: trim, fade, gain, color)<br/>Button assignments, clip lifecycle"]
            RoutingManager["RoutingManager<br/>(4 Clip Groups → Master)<br/>Group assignments, routing matrix config"]
            PreferencesManager["PreferencesManager<br/>(User settings persistence)<br/>Audio driver, buffer size, color scheme"]
        end
    end

    subgraph "Thread Boundary: Lock-Free Command Queue"
        CommandQueue["Lock-Free Queue<br/>(UI → Audio Thread)<br/>Start/stop clip, update metadata, seek<br/>NO allocations, NO locks"]
        CallbackQueue["Lock-Free Queue<br/>(Audio Thread → UI)<br/>Clip started/stopped, position updates<br/>Posted to message thread"]
    end

    subgraph "Thread: Audio Thread (Real-Time Processing)"
        subgraph "Layer 3: Orpheus SDK Integration"
            ITransportController["ITransportController<br/>(Clip playback, transport position)<br/>Sample-accurate timing, lock-free commands"]
            IAudioFileReader["IAudioFileReader<br/>(WAV/AIFF/FLAC decoding)<br/>Streaming playback, pre-buffering"]
            IRoutingMatrix["IRoutingMatrix<br/>(Multi-channel routing, mixing)<br/>4 groups → master, gain smoothing"]
            IPerformanceMonitor["IPerformanceMonitor<br/>(CPU/latency diagnostics)<br/>Audio thread metrics"]
        end

        subgraph "Layer 4: Real-Time Audio Processing (SDK Core)"
            SessionGraph["SessionGraph<br/>(Clip state machine, mix graph)<br/>Deterministic, bit-identical output"]
            AudioDriver["IAudioDriver<br/>(Platform abstraction)<br/>CoreAudio/ASIO/WASAPI"]
            MixEngine["Mix Engine<br/>(Gain smoothing, fade-in/out)<br/>Sample-accurate crossfades"]
            FileStreamer["File Streamer<br/>(Audio file streaming)<br/>Pre-allocated buffers, no I/O on audio thread"]
        end
    end

    subgraph "Thread: File I/O Thread (Background)"
        WaveformRenderer["Waveform Pre-Renderer<br/>(Generate waveform data for UI)<br/>Downsampling, peak detection"]
        FileScanner["Directory Scanner<br/>(Scan for audio files)<br/>Async file operations"]
    end

    subgraph "Layer 5: Platform Audio I/O (Hardware)"
        CoreAudio["CoreAudio<br/>(macOS)<br/>Low-latency audio I/O"]
        ASIO["ASIO<br/>(Windows Professional)<br/><5ms latency"]
        WASAPI["WASAPI<br/>(Windows Standard)<br/>~16ms latency"]
    end

    subgraph "External Dependencies"
        JUCE["JUCE 8.0.4<br/>(UI Framework, Audio I/O)<br/>Cross-platform abstraction"]
        OrpheusSDK["Orpheus SDK M2<br/>(Real-time audio engine)<br/>Host-neutral core"]
        libsndfile["libsndfile<br/>(Audio file decoding)<br/>WAV/AIFF/FLAC support"]
    end

    %% Layer 1 → Layer 2 Connections
    ClipGrid --> ClipManager
    ClipGrid --> SessionManager
    WaveformDisplay --> ClipManager
    ClipEditDialog --> ClipManager
    ClipEditDialog --> SessionManager
    TransportControls --> SessionManager
    RoutingPanel --> RoutingManager
    TabSwitcher --> ClipManager

    %% Layer 2 → Layer 3 Connections (via Command Queue)
    SessionManager --> CommandQueue
    ClipManager --> CommandQueue
    RoutingManager --> CommandQueue

    CommandQueue --> ITransportController
    CommandQueue --> IAudioFileReader
    CommandQueue --> IRoutingMatrix

    %% Layer 3 → Layer 2 Connections (via Callback Queue)
    ITransportController --> CallbackQueue
    IPerformanceMonitor --> CallbackQueue

    CallbackQueue --> SessionManager
    CallbackQueue --> ClipGrid
    CallbackQueue --> WaveformDisplay

    %% Layer 3 → Layer 4 Connections
    ITransportController --> SessionGraph
    IAudioFileReader --> FileStreamer
    IRoutingMatrix --> MixEngine
    IPerformanceMonitor --> AudioDriver

    %% Layer 4 → Layer 5 Connections
    SessionGraph --> MixEngine
    MixEngine --> AudioDriver
    AudioDriver --> CoreAudio
    AudioDriver --> ASIO
    AudioDriver --> WASAPI

    %% Background Thread Connections
    IAudioFileReader -.-> WaveformRenderer
    WaveformRenderer --> WaveformDisplay
    SessionManager -.-> FileScanner

    %% External Dependencies
    ClipGrid -.-> JUCE
    WaveformDisplay -.-> JUCE
    ITransportController -.-> OrpheusSDK
    IAudioFileReader -.-> OrpheusSDK
    IAudioFileReader -.-> libsndfile

    style CommandQueue fill:#ffcccc,stroke:#ff0000,stroke-width:2px
    style CallbackQueue fill:#ccffcc,stroke:#00ff00,stroke-width:2px
    style AudioDriver fill:#ffddaa,stroke:#ff8800,stroke-width:2px
    style OrpheusSDK fill:#ccddff,stroke:#0066ff,stroke-width:2px
