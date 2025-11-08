%% Orpheus SDK Architecture Overview
%% High-level system design showing layered architecture from platform to applications
%% Last updated: 2025-11-08

graph TB
    subgraph Applications["🎯 Applications Layer"]
        APP1["Orpheus Clip Composer<br/>(JUCE App)<br/>Professional soundboard<br/>Status: v0.2.0-alpha"]
        APP2["Orpheus Wave Finder<br/>(Planned)<br/>Harmonic calculator<br/>Status: v1.0 roadmap"]
        APP3["Orpheus FX Engine<br/>(Planned)<br/>LLM-powered effects<br/>Status: v1.0 roadmap"]
        APP4["JUCE Demo Host<br/>Integration demo<br/>Status: Active"]
    end

    subgraph Adapters["🔌 Adapters Layer (Thin Integration)"]
        ADP1["Minhost<br/>CLI Interface<br/>Offline rendering<br/>Session loading"]
        ADP2["REAPER Extension<br/>DAW Integration<br/>Status: Quarantined<br/>Pending SDK stabilization"]
        ADP3["Custom Adapters<br/>Partner integrations<br/>Status: Extensible"]
    end

    subgraph Drivers["🌐 Driver Layer (JavaScript/TypeScript)"]
        DRV1["Native Driver<br/>@orpheus/engine-native<br/>N-API bindings<br/>In-process access"]
        DRV2["Service Driver<br/>@orpheus/engine-service<br/>HTTP + WebSocket<br/>Remote access"]
        DRV3["WASM Driver<br/>@orpheus/engine-wasm<br/>Emscripten<br/>Browser access"]
        DRV4["Client Broker<br/>@orpheus/client<br/>Unified interface<br/>Auto driver selection"]
    end

    subgraph CoreSDK["⚙️ Core SDK (C++20)"]
        subgraph Session["Session Management"]
            CORE1["SessionGraph<br/>Tracks, clips, tempo<br/>Metadata persistence"]
            CORE2["Session JSON<br/>Load/save sessions<br/>Version control friendly"]
        end

        subgraph Transport["Real-Time Transport"]
            CORE3["TransportController<br/>Clip playback<br/>Sample-accurate timing<br/>Fade/gain/loop"]
            CORE4["ActiveClip Manager<br/>Lock-free structures<br/>Audio thread state"]
        end

        subgraph AudioIO["Audio I/O"]
            CORE5["AudioFileReader<br/>WAV/AIFF/FLAC<br/>libsndfile integration"]
            CORE6["Audio Drivers<br/>CoreAudio, WASAPI<br/>ALSA, Dummy"]
        end

        subgraph Routing["Routing & DSP"]
            CORE7["RoutingMatrix<br/>N×M routing<br/>Gain smoothing<br/>Metering"]
            CORE8["DSP Processing<br/>Oscillators<br/>Future: plugins"]
        end

        subgraph ABI["ABI & Contracts"]
            CORE9["ABI Version<br/>Version negotiation<br/>Compatibility checks"]
            CORE10["Contract System<br/>JSON schemas<br/>Command/event validation"]
        end
    end

    subgraph Platform["🖥️ Platform Layer"]
        PLAT1["macOS<br/>CoreAudio<br/>Low-latency I/O"]
        PLAT2["Windows<br/>WASAPI/ASIO<br/>Planned v1.0"]
        PLAT3["Linux<br/>ALSA<br/>Planned v1.0"]
        PLAT4["libsndfile<br/>Audio decoding<br/>Cross-platform"]
    end

    subgraph External["🌍 External Services (Optional)"]
        EXT1["Network Audio<br/>AES67/RTP<br/>PTP sync<br/>Planned v1.0"]
        EXT2["Remote Control<br/>WebSocket/OSC<br/>MIDI control<br/>Planned v1.0"]
        EXT3["Cloud Services<br/>License validation<br/>Optional features<br/>Never required for core"]
    end

    APP1 --> ADP1
    APP1 --> CoreSDK
    APP2 -.-> CoreSDK
    APP3 -.-> CoreSDK
    APP4 --> CoreSDK

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
    CORE5 --> CORE6
    CORE7 --> CORE8
    CORE9 --> CORE10

    CORE3 --> CORE5
    CORE3 --> CORE7
    CORE6 --> PLAT1
    CORE6 --> PLAT2
    CORE6 --> PLAT3
    CORE5 --> PLAT4

    CoreSDK -.Optional.-> EXT1
    CoreSDK -.Optional.-> EXT2
    CoreSDK -.Optional.-> EXT3

    style Applications fill:#e3f2fd
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
