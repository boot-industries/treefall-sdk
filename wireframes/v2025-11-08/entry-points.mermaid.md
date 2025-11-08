%% Orpheus SDK Entry Points
%% All ways to interact with the SDK including applications, CLI tools, and language bindings
%% Last updated: 2025-11-08

graph TB
    subgraph Users["👥 User Personas"]
        USER1["End User<br/>Broadcast/Theater<br/>Professional"]
        USER2["Application Developer<br/>Building on SDK<br/>TypeScript/JavaScript"]
        USER3["Plugin Developer<br/>Integrating SDK<br/>C++/Native"]
        USER4["DevOps/QA<br/>Automation/Testing<br/>CLI Tools"]
    end

    subgraph GUIApps["🖥️ GUI Applications"]
        APP1["Orpheus Clip Composer<br/>JUCE Desktop App<br/>(macOS/Windows/Linux)<br/><br/>Features:<br/>- 48-button clip grid × 8 tabs<br/>- Loop playback, fades<br/>- Audio device settings<br/>- Waveform display<br/><br/>Entry: Double-click app icon<br/>Config: Built-in preferences"]

        APP2["JUCE Demo Host<br/>Development Demo<br/>(macOS/Windows/Linux)<br/><br/>Features:<br/>- Basic clip playback<br/>- SDK integration example<br/><br/>Entry: Build and run<br/>Config: In-app settings"]

        APP3["Custom Applications<br/>Your App Here<br/>(Any platform)<br/><br/>Integration Options:<br/>- Direct C++ linking<br/>- JavaScript drivers<br/>- REAPER adapter<br/><br/>Entry: Your choice<br/>Config: Your design"]
    end

    subgraph CLITools["⌨️ Command-Line Tools"]
        CLI1["Minhost<br/>./orpheus_minhost<br/><br/>Commands:<br/>--session PATH<br/>Load session JSON<br/><br/>--render OUTPUT<br/>Render to WAV file<br/><br/>--bars N --bpm BPM<br/>Click track generation<br/><br/>--transport<br/>Simulate playback<br/><br/>Example:<br/>./orpheus_minhost \<br/>  --session session.json \<br/>  --render out.wav \<br/>  --bars 4 --bpm 120"]

        CLI2["Session Inspector<br/>./inspect_session<br/><br/>Commands:<br/>--file PATH<br/>Show session summary<br/><br/>--validate<br/>Check JSON schema<br/><br/>--stats<br/>Session statistics<br/><br/>Example:<br/>./inspect_session \<br/>  --file session.json \<br/>  --stats"]

        CLI3["JSON Round-Trip Tool<br/>./json_roundtrip<br/><br/>Commands:<br/>--input PATH<br/>Load and re-save JSON<br/><br/>--compare<br/>Byte-for-byte check<br/><br/>Example:<br/>./json_roundtrip \<br/>  --input session.json \<br/>  --compare"]
    end

    subgraph JSDrivers["🌐 JavaScript/TypeScript Drivers"]
        DRV1["Native Driver<br/>@orpheus/engine-native<br/><br/>Import:<br/>import { NativeEngine } from<br/>  '@orpheus/engine-native'<br/><br/>Usage:<br/>const engine = new NativeEngine()<br/>await engine.loadSession(path)<br/>await engine.renderClick(output)<br/><br/>Features:<br/>- Lowest latency (in-process)<br/>- Direct C++ access<br/>- Node.js/Electron apps"]

        DRV2["Service Driver<br/>@orpheus/engine-service<br/><br/>Import:<br/>import { ServiceEngine } from<br/>  '@orpheus/engine-service'<br/><br/>Usage:<br/>const engine = new ServiceEngine()<br/>await engine.start()<br/>await engine.connect()<br/>await engine.loadSession(path)<br/><br/>Features:<br/>- Remote access via HTTP<br/>- WebSocket events<br/>- Multi-process architecture"]

        DRV3["WASM Driver<br/>@orpheus/engine-wasm<br/><br/>Import:<br/>import { WASMEngine } from<br/>  '@orpheus/engine-wasm'<br/><br/>Usage:<br/>const engine = new WASMEngine()<br/>await engine.initialize()<br/>await engine.loadSession(path)<br/><br/>Features:<br/>- Browser-compatible<br/>- No server required<br/>- Web Worker isolation"]

        DRV4["Client Broker<br/>@orpheus/client<br/><br/>Import:<br/>import { OrpheusClient } from<br/>  '@orpheus/client'<br/><br/>Usage:<br/>const client = new OrpheusClient()<br/>await client.connect()<br/>// Auto-selects best driver<br/><br/>Features:<br/>- Unified API<br/>- Automatic driver selection<br/>- Fallback support"]
    end

    subgraph NativeAPI["⚙️ Native C++ API"]
        CPP1["Direct C++ Linking<br/>#include &lt;orpheus/transport_controller.h&gt;<br/><br/>Usage:<br/>SessionGraph session;<br/>TransportController transport(&session);<br/>transport.startClip(clipId);<br/><br/>Build:<br/>target_link_libraries(myapp<br/>  orpheus_core<br/>  orpheus_transport<br/>  orpheus_audio_io)"]

        CPP2["REAPER Extension<br/>reaper_orpheus.dylib/dll/so<br/><br/>Installation:<br/>Copy to REAPER UserPlugins/<br/><br/>Usage:<br/>REAPER Extensions menu<br/>→ Orpheus<br/>→ Load Session<br/><br/>Status: Quarantined<br/>(Pending SDK stabilization)"]
    end

    subgraph Config["⚙️ Configuration & Environment"]
        CFG1["Session JSON Files<br/>session.json<br/><br/>Format:<br/>Human-readable JSON<br/>Version control friendly<br/><br/>Location:<br/>User-specified path<br/><br/>Schema:<br/>packages/contract/schemas/<br/>  v1.0.0-beta/session.schema.json"]

        CFG2["Environment Variables<br/><br/>ORPHEUS_LOG_LEVEL<br/>Logging verbosity<br/>(ERROR|WARN|INFO|DEBUG)<br/><br/>ORPHEUS_DRIVER_TYPE<br/>Force specific driver<br/>(native|service|wasm)<br/><br/>ORPHEUS_SERVICE_PORT<br/>Service driver port<br/>(default: 8080)"]

        CFG3["Build Configuration<br/>CMake Options<br/><br/>-DORPHEUS_ENABLE_APP_CLIP_COMPOSER<br/>Build Clip Composer app<br/><br/>-DORPHEUS_ENABLE_REALTIME<br/>Enable real-time audio drivers<br/><br/>-DCMAKE_BUILD_TYPE<br/>Debug/Release/RelWithDebInfo"]
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
    USER4 --> CLI3

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
    CLI3 --> CPP1

    CPP2 -.-> CPP1

    APP1 -.Reads.-> CFG1
    CLI1 -.Reads.-> CFG1
    DRV4 -.Reads.-> CFG2
    CPP1 -.Build.-> CFG3

    style Users fill:#e3f2fd
    style GUIApps fill:#f3e5f5
    style CLITools fill:#e8f5e9
    style JSDrivers fill:#fff3e0
    style NativeAPI fill:#fce4ec
    style Config fill:#f1f8e9
