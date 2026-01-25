%% Orpheus SDK Repository Structure
%% Complete directory tree with ORP121 additions and shmui-juce v2.0.0
%% Last updated: 2026-01-25

graph TB
    subgraph Root["Repository Root"]
        A["orpheus-sdk<br/>Professional Audio SDK"]
    end

    subgraph Core["Core SDK (C++20)"]
        B["src/<br/>Implementation"]
        C["include/orpheus/<br/>Public API"]
        D["tests/<br/>GoogleTest suite"]

        B1["src/core/session/<br/>SessionGraph, JSON"]
        B2["src/core/transport/<br/>TransportController"]
        B3["src/core/routing/<br/>RoutingMatrix, DSP"]
        B4["src/core/audio_io/<br/>File readers"]
        B5["src/platform/<br/>Audio drivers"]

        C1["abi_version.h"]
        C2["transport_controller.h"]
        C3["routing_matrix.h<br/>(HeadroomMode, TruePeak)"]
        C4["audio_driver.h"]
        C5["performance_monitor.h"]
    end

    subgraph RoutingFiles["ORP121 Routing (New)"]
        R1["true_peak_meter.h<br/>ITU-R BS.1770-4"]
        R2["gain_smoother.cpp<br/>+12 dB range"]
        R3["GAIN_STAGING.md<br/>Documentation"]
    end

    subgraph Packages["Packages (shmui-juce v2.0.0)"]
        E["packages/"]
        E1["shmui-juce/<br/>Audio visualization + Controls"]
        E2["CMakeLists.txt<br/>Build integration"]
        E3["ShmUI.h<br/>Main include header"]
        E4["Audio/<br/>AudioAnalyzer (thread-safe)"]
        E5["Components/<br/>Visualizers + Controls"]
        E6["Controls/<br/>Button system"]
        E7["Icons/<br/>Icon library"]
        E8["Shaders/<br/>OrbFragment, OrbVertex"]
    end

    subgraph ShmComponents["shmui Components"]
        SC1["WaveformVisualizer<br/>WaveformEditor"]
        SC2["BarVisualizer<br/>OrbVisualizer"]
        SC3["MatrixDisplay<br/>LevelMeter"]
        SC4["TransportBar<br/>ScrubBar"]
        SC5["AudioPlayerControls"]
    end

    subgraph ShmControls["shmui Controls (Button Hierarchy)"]
        SB1["Button (base)<br/>Style variants"]
        SB2["TextButton<br/>IconButton"]
        SB3["ToggleButton<br/>TransportButton"]
        SB4["MuteButton<br/>ClipButton"]
    end

    subgraph Adapters["Adapters"]
        F["adapters/"]
        F1["minhost/<br/>CLI interface"]
        F2["reaper/<br/>(quarantined)"]
    end

    subgraph Applications["Applications"]
        G["apps/"]
        G1["clip-composer/<br/>JUCE soundboard"]
        G2["juce-demo-host/"]
    end

    subgraph Docs["Documentation"]
        I["docs/"]
        I1["orp/<br/>ORP documents"]
        I2["ORP121 Master Plan"]
        I3["ORP122 Phase 4 Report"]
        I4["GAIN_STAGING.md"]
    end

    subgraph Wireframes["Wireframes"]
        W["wireframes/"]
        W1["v2025-11-08/<br/>Previous version"]
        W2["v2026-01-18/<br/>Current (ORP121)"]
    end

    subgraph Config["Configuration"]
        J[".github/<br/>CI workflows"]
        K["CMakeLists.txt"]
        L["CLAUDE.md"]
    end

    A --> B
    A --> C
    A --> D
    A --> E
    A --> F
    A --> G
    A --> I
    A --> W
    A --> J
    A --> K
    A --> L

    B --> B1
    B --> B2
    B --> B3
    B --> B4
    B --> B5

    B3 --> R1
    B3 --> R2

    C --> C1
    C --> C2
    C --> C3
    C --> C4
    C --> C5

    E --> E1
    E1 --> E2
    E1 --> E3
    E1 --> E4
    E1 --> E5
    E1 --> E6
    E1 --> E7
    E1 --> E8

    E5 --> SC1
    E5 --> SC2
    E5 --> SC3
    E5 --> SC4
    E5 --> SC5

    E6 --> SB1
    SB1 --> SB2
    SB1 --> SB3
    SB1 --> SB4

    F --> F1
    F --> F2

    G --> G1
    G --> G2

    I --> I1
    I1 --> I2
    I1 --> I3
    I1 --> I4

    W --> W1
    W --> W2

    style Root fill:#e1f5ff
    style Core fill:#fff4e6
    style RoutingFiles fill:#ffccbc
    style Packages fill:#e8f5e9
    style ShmComponents fill:#c8e6c9
    style ShmControls fill:#a5d6a7
    style Adapters fill:#f3e5f5
    style Applications fill:#fce4ec
    style Docs fill:#e0f2f1
    style Wireframes fill:#fff9c4
    style Config fill:#f1f8e9
