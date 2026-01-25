%% Orpheus SDK Architecture Overview
%% High-level system design with ORP121 routing enhancements and shmui-juce v2.0.0
%% Last updated: 2026-01-25

graph TB
    subgraph Applications["Applications Layer"]
        APP1["Orpheus Clip Composer<br/>(JUCE App)<br/>Professional soundboard<br/>Status: v0.2.x"]
        APP2["Orpheus Wave Finder<br/>(Planned)<br/>Harmonic calculator"]
        APP3["Orpheus FX Engine<br/>(Planned)<br/>LLM-powered effects"]
        APP4["JUCE Demo Host<br/>Integration demo"]
    end

    subgraph ShmUI["shmui-juce (v2.0.0 Active)"]
        SHMUI1["Audio/<br/>AudioAnalyzer (thread-safe)<br/>FFT, RMS, bands"]
        SHMUI2["Components/<br/>WaveformVisualizer, Editor<br/>BarVisualizer, OrbVisualizer<br/>MatrixDisplay, LevelMeter"]
        SHMUI3["Controls/<br/>Button hierarchy<br/>Transport, Mute, Clip"]
        SHMUI4["Icons/<br/>Icon library<br/>Transport, Audio, UI"]
    end

    subgraph Adapters["Adapters Layer"]
        ADP1["Minhost CLI<br/>Offline rendering<br/>Session loading"]
        ADP2["REAPER Extension<br/>DAW Integration<br/>Status: Quarantined"]
        ADP3["Custom Adapters<br/>Partner integrations"]
    end

    subgraph Drivers["Driver Layer (TypeScript)"]
        DRV1["Native Driver<br/>@orpheus/engine-native<br/>N-API bindings"]
        DRV2["Service Driver<br/>@orpheus/engine-service<br/>HTTP + WebSocket"]
        DRV3["WASM Driver<br/>@orpheus/engine-wasm<br/>Browser access"]
        DRV4["Client Broker<br/>@orpheus/client<br/>Auto driver selection"]
    end

    subgraph CoreSDK["Core SDK (C++20)"]
        subgraph Session["Session Management"]
            CORE1["SessionGraph<br/>Tracks, clips, tempo<br/>Metadata persistence"]
            CORE2["Session JSON<br/>Load/save sessions"]
        end

        subgraph Transport["Real-Time Transport"]
            CORE3["TransportController<br/>Clip playback<br/>Sample-accurate timing<br/>Fade/gain/loop"]
            CORE4["ActiveClip Manager<br/>Lock-free structures"]
            CORE5["SPSC Callback Queue<br/>Lock-free ring buffer<br/>256 slots (ORP121 C-03)"]
        end

        subgraph AudioIO["Audio I/O"]
            CORE6["AudioFileReader<br/>WAV/AIFF/FLAC<br/>libsndfile"]
            CORE7["Audio Drivers<br/>CoreAudio, WASAPI<br/>ALSA, Dummy"]
        end

        subgraph Routing["Routing & DSP (ORP121)"]
            CORE8["RoutingMatrix<br/>N×M stereo routing<br/>Constant-power pan law"]
            CORE9["GainSmoother<br/>Range: -inf to +12 dB<br/>Smooth transitions"]
            CORE10["TruePeakMeter<br/>ITU-R BS.1770-4<br/>4x oversampling"]
            CORE11["HeadroomMode<br/>None / PerGroup<br/>Global / Logarithmic"]
            CORE12["Soft-Knee Limiter<br/>-2 dBFS threshold<br/>C1 continuous curve"]
        end

        subgraph ABI["ABI & Contracts"]
            CORE13["ABI Version<br/>Compatibility checks"]
            CORE14["Contract System<br/>JSON schemas"]
        end
    end

    subgraph Platform["Platform Layer"]
        PLAT1["macOS CoreAudio<br/>Low-latency I/O"]
        PLAT2["Windows WASAPI<br/>Planned v1.0"]
        PLAT3["Linux ALSA<br/>Planned v1.0"]
        PLAT4["libsndfile<br/>Audio decoding"]
    end

    subgraph External["External Services (Optional)"]
        EXT1["Network Audio<br/>AES67/ST2110<br/>PTP sync"]
        EXT2["Remote Control<br/>WebSocket/OSC"]
    end

    APP1 --> ShmUI
    APP1 --> ADP1
    APP1 --> CoreSDK
    APP2 -.-> CoreSDK
    APP3 -.-> CoreSDK
    APP4 --> CoreSDK
    APP4 -.-> ShmUI

    ShmUI --> CoreSDK

    ADP1 --> CoreSDK
    ADP2 -.-> CoreSDK
    ADP3 -.-> CoreSDK

    DRV1 --> CoreSDK
    DRV2 --> CoreSDK
    DRV3 --> CoreSDK
    DRV4 --> DRV1
    DRV4 --> DRV2
    DRV4 --> DRV3

    CORE1 --> CORE2
    CORE3 --> CORE4
    CORE3 --> CORE5
    CORE6 --> CORE7

    CORE8 --> CORE9
    CORE8 --> CORE10
    CORE8 --> CORE11
    CORE8 --> CORE12

    CORE3 --> CORE6
    CORE3 --> CORE8
    CORE7 --> PLAT1
    CORE7 --> PLAT2
    CORE7 --> PLAT3
    CORE6 --> PLAT4

    CoreSDK -.Optional.-> EXT1
    CoreSDK -.Optional.-> EXT2

    style Applications fill:#e3f2fd
    style ShmUI fill:#c8e6c9
    style Adapters fill:#f3e5f5
    style Drivers fill:#e8f5e9
    style CoreSDK fill:#fff3e0
    style Session fill:#fff9c4
    style Transport fill:#ffecb3
    style AudioIO fill:#ffe0b2
    style Routing fill:#ffccbc
    style ABI fill:#d7ccc8
    style Platform fill:#cfd8dc
    style External fill:#b2dfdb
