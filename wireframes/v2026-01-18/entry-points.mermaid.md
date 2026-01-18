%% Orpheus SDK Entry Points
%% All ways to interact with the SDK including applications, CLI tools, and language bindings
%% Last updated: 2026-01-18

graph TB
    subgraph Users["User Personas"]
        USER1["End User<br/>Broadcast/Theater"]
        USER2["App Developer<br/>TypeScript/JavaScript"]
        USER3["Plugin Developer<br/>C++/Native"]
        USER4["DevOps/QA<br/>CLI Tools"]
    end

    subgraph GUIApps["GUI Applications"]
        APP1["Orpheus Clip Composer<br/>JUCE Desktop App<br/><br/>- 48-button grid x 8 tabs<br/>- Loop playback, fades<br/>- Waveform display<br/>- ORP121: Improved routing"]

        APP2["JUCE Demo Host<br/>Integration Demo"]

        APP3["Custom Applications<br/>Your App Here"]
    end

    subgraph CLITools["Command-Line Tools"]
        CLI1["Minhost<br/>./orpheus_minhost<br/><br/>--session PATH<br/>--render OUTPUT<br/>--bars N --bpm BPM"]

        CLI2["Session Inspector<br/>./inspect_session<br/><br/>--file PATH<br/>--validate<br/>--stats"]
    end

    subgraph JSDrivers["JavaScript Drivers"]
        DRV1["Native Driver<br/>@orpheus/engine-native<br/>Lowest latency"]

        DRV2["Service Driver<br/>@orpheus/engine-service<br/>HTTP + WebSocket"]

        DRV3["WASM Driver<br/>@orpheus/engine-wasm<br/>Browser access"]

        DRV4["Client Broker<br/>@orpheus/client<br/>Auto driver selection"]
    end

    subgraph NativeAPI["Native C++ API"]
        CPP1["Direct C++ Linking<br/><br/>#include routing_matrix.h<br/>#include transport_controller.h<br/><br/>ORP121 APIs:<br/>- HeadroomMode config<br/>- TruePeak metering<br/>- Lock-free callbacks"]

        CPP2["REAPER Extension<br/>(Quarantined)"]
    end

    subgraph Config["Configuration"]
        CFG1["Session JSON<br/>session.json<br/>Human-readable<br/>Version control friendly"]

        CFG2["Environment Variables<br/>ORPHEUS_LOG_LEVEL<br/>ORPHEUS_DRIVER_TYPE"]

        CFG3["RoutingConfig<br/>ORP121 additions:<br/>- sample_rate<br/>- headroom_mode<br/>- enable_true_peak"]
    end

    USER1 --> APP1
    USER1 -.-> APP3

    USER2 --> DRV4
    USER2 --> DRV1
    USER2 --> DRV2
    USER2 --> DRV3

    USER3 --> CPP1
    USER3 -.-> CPP2

    USER4 --> CLI1
    USER4 --> CLI2

    APP1 --> CPP1
    APP2 --> CPP1
    APP3 --> CPP1
    APP3 -.-> DRV4

    DRV4 --> DRV1
    DRV4 --> DRV2
    DRV4 --> DRV3

    DRV1 --> CPP1
    DRV2 --> CLI1
    DRV3 --> CPP1

    CLI1 --> CPP1
    CLI2 --> CPP1

    APP1 -.Reads.-> CFG1
    APP1 -.Uses.-> CFG3
    DRV4 -.Reads.-> CFG2

    style Users fill:#e3f2fd
    style GUIApps fill:#f3e5f5
    style CLITools fill:#e8f5e9
    style JSDrivers fill:#fff3e0
    style NativeAPI fill:#fce4ec
    style Config fill:#f1f8e9
