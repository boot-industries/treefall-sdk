%% Orpheus SDK Deployment Infrastructure
%% CI/CD pipeline, build system, testing infrastructure, and deployment architecture
%% Last updated: 2025-11-08

graph TB
    subgraph Development["💻 Development Environment"]
        DEV1["Developer Workstation<br/>(macOS/Windows/Linux)<br/><br/>Tools:<br/>- C++20 compiler (clang/gcc/msvc)<br/>- CMake 3.20+<br/>- Node.js 18+ / pnpm<br/>- Git"]
        DEV2["Local Build<br/><br/>Commands:<br/>cmake -S . -B build<br/>cmake --build build<br/><br/>Output:<br/>- Static libraries<br/>- Executables<br/>- Test binaries"]
        DEV3["Local Testing<br/><br/>Commands:<br/>ctest --test-dir build<br/><br/>Tools:<br/>- GoogleTest (C++)<br/>- Jest (TypeScript)<br/>- AddressSanitizer<br/>- UBSanitizer"]
    end

    subgraph VersionControl["📦 Version Control (GitHub)"]
        VC1["Git Repository<br/>github.com/yourusername/orpheus-sdk<br/><br/>Branches:<br/>- main (stable)<br/>- feature/* (development)<br/>- claude/* (AI-assisted work)"]
        VC2["Pull Request<br/><br/>Checks:<br/>- CI pipeline passes<br/>- Code review approved<br/>- No conflicts<br/>- Tests pass"]
        VC3["Release Tags<br/><br/>Format:<br/>v1.0.0-alpha<br/>v1.0.0-beta<br/>v1.0.0<br/><br/>Triggers:<br/>- Package builds<br/>- Documentation publish"]
    end

    subgraph CIPipeline["🔄 CI/CD Pipeline (GitHub Actions)"]
        CI1["Unified Pipeline<br/>.github/workflows/ci-pipeline.yml<br/><br/>Trigger: Push, PR<br/>Target: <25 minutes<br/><br/>Jobs:<br/>- cpp-build-test (6 combinations)<br/>- lint<br/>- native-driver<br/>- typescript<br/>- integration<br/>- deps<br/>- perf"]

        CI2["Matrix Build<br/>C++ Build & Test<br/><br/>OS: ubuntu, windows, macos<br/>Type: Debug, Release<br/><br/>Total: 6 parallel jobs<br/><br/>Tools:<br/>- CMake<br/>- GCC/Clang/MSVC<br/>- AddressSanitizer (Debug)<br/>- UBSanitizer (Debug)"]

        CI3["Lint Job<br/>Code Quality Checks<br/><br/>Tools:<br/>- clang-format (C++)<br/>- eslint (TypeScript)<br/>- prettier (JSON/YAML)<br/><br/>Gate: Must pass to merge"]

        CI4["Native Driver Job<br/>N-API Bindings Build<br/><br/>Platform: ubuntu<br/><br/>Steps:<br/>1. Build C++ SDK<br/>2. Build native addon<br/>3. Run Node.js tests"]

        CI5["TypeScript Job<br/>JS Package Testing<br/><br/>Packages:<br/>- @orpheus/client<br/>- @orpheus/contract<br/>- @orpheus/react<br/><br/>Tools:<br/>- pnpm<br/>- Jest<br/>- TypeScript compiler"]

        CI6["Integration Job<br/>End-to-End Tests<br/><br/>Scenarios:<br/>- Service driver handshake<br/>- Native driver commands<br/>- Event frequency validation<br/>- Multi-clip stress test"]

        CI7["Dependencies Job<br/>Dependency Audit<br/><br/>Checks:<br/>- npm audit (vulnerabilities)<br/>- Outdated packages<br/>- License compliance<br/>- CMake dependency graph"]

        CI8["Performance Job<br/>Performance Budgets<br/><br/>Metrics:<br/>- CPU usage <20% (8 clips)<br/>- Memory <100MB baseline<br/>- Latency <10ms (256 buffer)<br/><br/>Gate: Warn on regression"]
    end

    subgraph SpecializedCI["🔧 Specialized Workflows"]
        SPEC1["Chaos Tests<br/>.github/workflows/chaos-tests.yml<br/><br/>Trigger: Nightly (3 AM UTC)<br/>Duration: 2 hours<br/><br/>Tests:<br/>- Rapid start/stop (100/sec)<br/>- 24-hour stability<br/>- Resource exhaustion<br/>- Recovery from failures"]

        SPEC2["Security Audit<br/>.github/workflows/security-audit.yml<br/><br/>Trigger: Weekly (Monday)<br/><br/>Tools:<br/>- CodeQL (static analysis)<br/>- npm audit<br/>- Dependency scanning<br/>- Secret detection"]

        SPEC3["Docs Publish<br/>.github/workflows/docs-publish.yml<br/><br/>Trigger: Release tag<br/><br/>Steps:<br/>1. Generate Doxygen<br/>2. Build API docs<br/>3. Publish to GitHub Pages<br/>4. Update versioned docs"]
    end

    subgraph BuildArtifacts["📦 Build Artifacts"]
        ART1["C++ Libraries<br/><br/>Static:<br/>- liborpheus_core.a<br/>- liborpheus_transport.a<br/>- liborpheus_audio_io.a<br/><br/>Shared:<br/>- liborpheus.dylib/dll/so"]

        ART2["Executables<br/><br/>CLI Tools:<br/>- orpheus_minhost<br/>- inspect_session<br/>- json_roundtrip<br/><br/>Applications:<br/>- OrpheusClipComposer.app"]

        ART3["npm Packages<br/><br/>Published to npm:<br/>- @orpheus/client<br/>- @orpheus/contract<br/>- @orpheus/engine-native<br/>- @orpheus/engine-service<br/>- @orpheus/engine-wasm<br/>- @orpheus/react"]

        ART4["Test Binaries<br/><br/>GoogleTest:<br/>- abi_smoke_test<br/>- session_roundtrip_test<br/>- transport_tests<br/>- routing_tests<br/>- audio_io_tests"]
    end

    subgraph Distribution["🌍 Distribution Channels"]
        DIST1["GitHub Releases<br/><br/>Assets:<br/>- Source code (zip/tar.gz)<br/>- Compiled binaries (multi-platform)<br/>- Documentation PDF<br/>- CHANGELOG.md<br/><br/>URL:<br/>github.com/.../releases"]

        DIST2["npm Registry<br/><br/>Packages:<br/>@orpheus/* (public)<br/><br/>Install:<br/>npm install @orpheus/client<br/><br/>Versioning:<br/>Semantic versioning (semver)"]

        DIST3["Application Stores<br/>(Future)<br/><br/>macOS:<br/>- Mac App Store<br/>- Notarized DMG<br/><br/>Windows:<br/>- Microsoft Store<br/>- Standalone installer"]

        DIST4["GitHub Pages<br/>docs.orpheus-sdk.dev<br/><br/>Content:<br/>- API documentation<br/>- User guides<br/>- Tutorial videos<br/>- Release notes"]
    end

    subgraph DeploymentTargets["🎯 Deployment Targets"]
        TARGET1["Desktop Applications<br/><br/>Platforms:<br/>- macOS 10.15+<br/>- Windows 10+<br/>- Linux (Ubuntu 20.04+)<br/><br/>Distribution:<br/>- DMG (macOS)<br/>- EXE installer (Windows)<br/>- AppImage (Linux)"]

        TARGET2["Web Applications<br/><br/>Deployment:<br/>- Static hosting (Vercel, Netlify)<br/>- Node.js server (Heroku, Fly.io)<br/><br/>Requirements:<br/>- HTTPS (for WebSocket)<br/>- CORS configuration"]

        TARGET3["Embedded Systems<br/>(Future)<br/><br/>Targets:<br/>- Raspberry Pi (ARM)<br/>- Dedicated audio hardware<br/><br/>Requirements:<br/>- Cross-compilation<br/>- Minimal dependencies"]

        TARGET4["Cloud Services<br/>(Future)<br/><br/>Use cases:<br/>- Server-side rendering<br/>- Batch processing<br/>- API services<br/><br/>Platforms:<br/>- AWS Lambda<br/>- Google Cloud Run"]
    end

    DEV1 --> DEV2
    DEV2 --> DEV3
    DEV3 --> VC1

    VC1 --> VC2
    VC2 --> CI1

    CI1 --> CI2
    CI1 --> CI3
    CI1 --> CI4
    CI1 --> CI5
    CI1 --> CI6
    CI1 --> CI7
    CI1 --> CI8

    CI2 --> ART1
    CI2 --> ART2
    CI2 --> ART4
    CI4 --> ART3
    CI5 --> ART3

    SPEC1 -.Nightly.-> CI1
    SPEC2 -.Weekly.-> CI1
    VC3 --> SPEC3

    ART1 --> DIST1
    ART2 --> DIST1
    ART3 --> DIST2
    SPEC3 --> DIST4

    DIST1 --> TARGET1
    DIST2 --> TARGET2
    DIST1 -.Future.-> TARGET3
    DIST1 -.Future.-> TARGET4

    VC2 --> VC3
    VC3 --> DIST1
    VC3 --> DIST2

    style Development fill:#e3f2fd
    style VersionControl fill:#f3e5f5
    style CIPipeline fill:#e8f5e9
    style SpecializedCI fill:#fff3e0
    style BuildArtifacts fill:#fce4ec
    style Distribution fill:#f1f8e9
    style DeploymentTargets fill:#e0f2f1
