%% Orpheus SDK Repository Structure
%% Complete directory tree visualization showing core components, packages, adapters, and applications
%% Last updated: 2025-11-08

graph TB
    subgraph Root["Repository Root"]
        A["📁 orpheus-sdk<br/>Professional Audio SDK"]
    end

    subgraph Core["Core SDK (C++20)"]
        B["📁 src/<br/>Implementation files"]
        C["📁 include/orpheus/<br/>Public API headers"]
        D["📁 tests/<br/>GoogleTest suite"]

        B1["📁 src/core/<br/>Session, Transport, Audio I/O"]
        B2["📁 src/platform/<br/>Platform-specific drivers"]
        B3["📁 src/dsp/<br/>DSP processing"]
        B4["📁 src/orpheus/<br/>Utilities"]

        C1["📄 abi_version.h<br/>Version negotiation"]
        C2["📄 transport_controller.h<br/>Real-time transport"]
        C3["📄 audio_file_reader.h<br/>File I/O"]
        C4["📄 audio_driver.h<br/>Platform drivers"]
        C5["📄 routing_matrix.h<br/>Audio routing"]
    end

    subgraph Packages["Driver Layer (TypeScript/JavaScript)"]
        E["📁 packages/"]

        E1["📁 client/<br/>Unified client API"]
        E2["📁 contract/<br/>JSON schemas"]
        E3["📁 engine-native/<br/>N-API bindings"]
        E4["📁 engine-service/<br/>HTTP + WebSocket"]
        E5["📁 engine-wasm/<br/>Emscripten WASM"]
        E6["📁 react/<br/>React components"]
    end

    subgraph Adapters["Adapters (Host Integration)"]
        F["📁 adapters/"]

        F1["📁 minhost/<br/>CLI interface"]
        F2["📁 reaper/<br/>REAPER extension (quarantined)"]
    end

    subgraph Applications["Applications"]
        G["📁 apps/"]

        G1["📁 clip-composer/<br/>JUCE soundboard app"]
        G2["📁 juce-demo-host/<br/>JUCE demo"]
    end

    subgraph Tools["Developer Tools"]
        H["📁 tools/"]

        H1["📁 cli/<br/>Command-line utilities"]
        H2["📁 conformance/<br/>Validation tools"]
        H3["📁 fixtures/<br/>Test data"]
        H4["📁 perf/<br/>Performance benchmarks"]
    end

    subgraph Docs["Documentation"]
        I["📁 docs/"]

        I1["📁 orp/<br/>ORP implementation plans"]
        I2["📄 ARCHITECTURE.md<br/>System design"]
        I3["📄 ROADMAP.md<br/>Development timeline"]
        I4["📄 CONTRIBUTING.md<br/>Contribution guide"]
    end

    subgraph Config["Configuration Files"]
        J["📁 .github/<br/>CI/CD workflows"]
        K["📄 CMakeLists.txt<br/>Build system"]
        L["📄 package.json<br/>Node.js config"]
        M["📄 pnpm-workspace.yaml<br/>Monorepo config"]
        N["📄 CLAUDE.md<br/>Dev guide"]
        O["📄 .clang-format<br/>Code style"]
    end

    A --> B
    A --> C
    A --> D
    A --> E
    A --> F
    A --> G
    A --> H
    A --> I
    A --> J
    A --> K
    A --> L
    A --> M
    A --> N
    A --> O

    B --> B1
    B --> B2
    B --> B3
    B --> B4

    C --> C1
    C --> C2
    C --> C3
    C --> C4
    C --> C5

    E --> E1
    E --> E2
    E --> E3
    E --> E4
    E --> E5
    E --> E6

    F --> F1
    F --> F2

    G --> G1
    G --> G2

    H --> H1
    H --> H2
    H --> H3
    H --> H4

    I --> I1
    I --> I2
    I --> I3
    I --> I4

    style Root fill:#e1f5ff
    style Core fill:#fff4e6
    style Packages fill:#e8f5e9
    style Adapters fill:#f3e5f5
    style Applications fill:#fce4ec
    style Tools fill:#fff8e1
    style Docs fill:#e0f2f1
    style Config fill:#f1f8e9
