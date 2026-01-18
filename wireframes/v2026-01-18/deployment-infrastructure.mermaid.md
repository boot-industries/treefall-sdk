%% Orpheus SDK Deployment Infrastructure
%% CI/CD pipeline, build system, and testing infrastructure
%% Last updated: 2026-01-18

graph TB
    subgraph Development["Development"]
        DEV1["Developer Workstation<br/>C++20, CMake, Node.js"]
        DEV2["Local Build<br/>cmake --build build"]
        DEV3["Local Testing<br/>ctest + sanitizers"]
    end

    subgraph VersionControl["Version Control"]
        VC1["Git Repository<br/>main, feature/*, claude/*"]
        VC2["Pull Request<br/>CI must pass"]
        VC3["Release Tags<br/>v1.0.0-rc.1"]
    end

    subgraph CIPipeline["CI Pipeline"]
        CI1["Unified Pipeline<br/>ci-pipeline.yml<br/>Target: 25 min"]

        CI2["Matrix Build<br/>ubuntu/windows/macos<br/>Debug/Release<br/>ASan + UBSan"]

        CI3["Lint Job<br/>clang-format<br/>eslint"]

        CI4["Native Driver<br/>N-API build"]

        CI5["TypeScript<br/>pnpm build"]

        CI6["Integration Tests<br/>Multi-clip stress"]

        CI7["Dependencies<br/>npm audit"]

        CI8["Performance<br/>Budget validation"]
    end

    subgraph Testing["ORP121 Test Suite"]
        TEST1["routing_matrix_test<br/>27 tests"]
        TEST2["callback_queue_stress<br/>6 tests"]
        TEST3["Performance Tests<br/>Waveform rendering"]
    end

    subgraph SpecializedCI["Specialized Workflows"]
        SPEC1["Chaos Tests<br/>Nightly 3 AM UTC"]
        SPEC2["Security Audit<br/>Weekly Monday"]
        SPEC3["Docs Publish<br/>On release tag"]
    end

    subgraph BuildArtifacts["Build Artifacts"]
        ART1["C++ Libraries<br/>liborpheus_*.a"]
        ART2["Executables<br/>orpheus_minhost<br/>ClipComposer.app"]
        ART3["npm Packages<br/>@orpheus/*"]
    end

    subgraph Distribution["Distribution"]
        DIST1["GitHub Releases<br/>Binaries + source"]
        DIST2["npm Registry<br/>@orpheus packages"]
        DIST3["GitHub Pages<br/>API docs"]
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

    CI2 --> TEST1
    CI2 --> TEST2
    CI2 --> TEST3

    CI2 --> ART1
    CI2 --> ART2
    CI5 --> ART3

    SPEC1 -.Nightly.-> CI1
    SPEC2 -.Weekly.-> CI1
    VC3 --> SPEC3

    ART1 --> DIST1
    ART2 --> DIST1
    ART3 --> DIST2
    SPEC3 --> DIST3

    VC2 --> VC3
    VC3 --> DIST1
    VC3 --> DIST2

    style Development fill:#e3f2fd
    style VersionControl fill:#f3e5f5
    style CIPipeline fill:#e8f5e9
    style Testing fill:#ffccbc
    style SpecializedCI fill:#fff3e0
    style BuildArtifacts fill:#fce4ec
    style Distribution fill:#f1f8e9
