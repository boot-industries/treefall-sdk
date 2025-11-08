%% Repository Structure for Orpheus Clip Composer v0.2.0
%% Shows directory organization, key files, and documentation layout
%% Last Updated: October 31, 2025

graph TB
    Root["/apps/clip-composer<br/>(Application Root)"]

    subgraph "Source Code (C++20)"
        Source["Source/<br/>(JUCE Application Code)"]
        Main["Main.cpp<br/>(Entry Point)"]
        MainComp["MainComponent.h/cpp<br/>(Top-level UI Container)"]

        subgraph "UI Components"
            UI["UI/<br/>(12 files)"]
            ClipEdit["ClipEditDialog.h/cpp<br/>(Waveform Editor)"]
            Waveform["WaveformDisplay.h/cpp<br/>(Visual Waveform)"]
            TabSwitch["TabSwitcher.h/cpp<br/>(8-Tab Navigation)"]
            AudioSettings["AudioSettingsDialog.h/cpp<br/>(Device Selection)"]
            ColorPicker["ColorSwatchPicker.h/cpp<br/>(Color Palette)"]
            PreviewPlayer["PreviewPlayer.h/cpp<br/>(File Preview)"]
            InterLAF["InterLookAndFeel.h<br/>(UI Styling)"]
        end

        subgraph "ClipGrid System"
            ClipGrid["ClipGrid/<br/>(4 files)"]
            ClipGridImpl["ClipGrid.h/cpp<br/>(10×12 Button Layout)"]
            ClipButton["ClipButton.h/cpp<br/>(Individual Button)"]
        end

        subgraph "Audio Engine Integration"
            AudioEngineDir["AudioEngine/<br/>(2 files)"]
            AudioEngine["AudioEngine.h/cpp<br/>(SDK Integration Layer)"]
        end

        subgraph "Legacy Audio (Duplicate)"
            AudioDir["Audio/<br/>(2 files - legacy)"]
            AudioEngineLegacy["AudioEngine.h/cpp<br/>(To be removed)"]
        end

        subgraph "Transport System"
            TransportDir["Transport/<br/>(2 files)"]
            TransportControls["TransportControls.h/cpp<br/>(Play/Stop/Panic UI)"]
        end

        subgraph "Session Management"
            SessionDir["Session/<br/>(2 files)"]
            SessionManager["SessionManager.h/cpp<br/>(JSON Save/Load)"]
        end
    end

    subgraph "Resources & Assets"
        Resources["Resources/<br/>(Fonts, Icons)"]
        HKGrotesk["HKGrotesk_3003/<br/>(TTF, OTF, WEB)"]
    end

    subgraph "Documentation (35 Active Docs)"
        DocsRoot["docs/"]
        OCC["occ/<br/>(OCC### prefix)"]

        subgraph "Product Documentation"
            OCC021["OCC021 - Product Vision"]
            OCC026["OCC026 - MVP Definition"]
        end

        subgraph "Technical Specifications"
            OCC040["OCC040-OCC045<br/>(Architecture Specs)"]
            OCC096["OCC096 - SDK Integration"]
            OCC097["OCC097 - Session Format"]
            OCC098["OCC098 - UI Components"]
        end

        subgraph "Sprint Reports"
            OCC090["OCC090 - v0.2.0 Sprint"]
            OCC093["OCC093 - v0.2.0 Complete"]
        end

        Archive["archive/<br/>(90+ day old docs)"]
    end

    subgraph "Configuration & Build"
        CMake["CMakeLists.txt<br/>(JUCE + SDK Build)"]
        ClaudeDir[".claude/<br/>(Skills, Progress)"]
        ClaudeIgnore[".claudeignore<br/>(Exclude Patterns)"]
        SkillsJSON["skills.json<br/>(Lazy-loaded Skills)"]
    end

    subgraph "Root Documentation"
        CLAUDE["CLAUDE.md<br/>(440 lines - Dev Guide)"]
        PROGRESS["PROGRESS.md<br/>(480 lines - Status)"]
        README["README.md<br/>(Getting Started)"]
        CHANGELOG["CHANGELOG.md<br/>(v0.1.0, v0.2.0)"]
    end

    subgraph "Build Artifacts (DO NOT COMMIT)"
        Build["build/<br/>(JUCE Build Output)"]
        Artefacts["orpheus_clip_composer_app_artefacts/<br/>(Tauri Builds)"]
    end

    Root --> Source
    Source --> Main
    Source --> MainComp
    Source --> UI
    Source --> ClipGrid
    Source --> AudioEngineDir
    Source --> AudioDir
    Source --> TransportDir
    Source --> SessionDir

    UI --> ClipEdit
    UI --> Waveform
    UI --> TabSwitch
    UI --> AudioSettings
    UI --> ColorPicker
    UI --> PreviewPlayer
    UI --> InterLAF

    ClipGrid --> ClipGridImpl
    ClipGrid --> ClipButton

    AudioEngineDir --> AudioEngine
    AudioDir --> AudioEngineLegacy

    TransportDir --> TransportControls
    SessionDir --> SessionManager

    Root --> Resources
    Resources --> HKGrotesk

    Root --> DocsRoot
    DocsRoot --> OCC
    OCC --> OCC021
    OCC --> OCC026
    OCC --> OCC040
    OCC --> OCC096
    OCC --> OCC097
    OCC --> OCC098
    OCC --> OCC090
    OCC --> OCC093
    OCC --> Archive

    Root --> CMake
    Root --> ClaudeDir
    ClaudeDir --> ClaudeIgnore
    ClaudeDir --> SkillsJSON

    Root --> CLAUDE
    Root --> PROGRESS
    Root --> README
    Root --> CHANGELOG

    Root -.-> Build
    Root -.-> Artefacts

    style Build fill:#f9f,stroke:#333,stroke-width:2px,stroke-dasharray: 5 5
    style Artefacts fill:#f9f,stroke:#333,stroke-width:2px,stroke-dasharray: 5 5
    style AudioDir fill:#ff9,stroke:#333,stroke-width:2px
    style AudioEngineLegacy fill:#ff9,stroke:#333,stroke-width:2px
