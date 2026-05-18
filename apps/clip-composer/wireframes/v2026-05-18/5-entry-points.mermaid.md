%% Entry Points for Orpheus Clip Composer v0.2.0
%% Shows application initialization, user interactions, and system events
%% Last Updated: October 31, 2025

flowchart TD
    subgraph "Application Lifecycle"
        Start["Application Start<br/>(Main.cpp entry point)"]
        JUCEInit["JUCE Initialization<br/>(JUCEApplication::initialise)"]
        CreateWindow["Create MainWindow<br/>(juce::DocumentWindow)"]
        InitSDK["Initialize Orpheus SDK<br/>(AudioEngine::initialize)"]
        InitAudio["Initialize Audio Driver<br/>(CoreAudio/WASAPI)"]
        CreateUI["Create UI Components<br/>(MainComponent constructor)"]
        LoadLastSession["Load Last Session<br/>(Auto-restore previous state)"]
        ShowWindow["Show Main Window<br/>(setVisible true)"]
        Running["Application Running<br/>(Event loop active)"]
        Shutdown["Application Shutdown<br/>(JUCEApplication::shutdown)"]
        CleanupAudio["Stop Audio Driver<br/>(Cleanup buffers)"]
        CleanupSDK["Cleanup SDK Resources<br/>(AudioEngine destructor)"]
        Exit["Exit Application"]

        Start --> JUCEInit
        JUCEInit --> CreateWindow
        CreateWindow --> InitSDK
        InitSDK --> InitAudio
        InitAudio --> CreateUI
        CreateUI --> LoadLastSession
        LoadLastSession --> ShowWindow
        ShowWindow --> Running
        Running --> Shutdown
        Shutdown --> CleanupAudio
        CleanupAudio --> CleanupSDK
        CleanupSDK --> Exit
    end

    subgraph "User Interactions: Mouse"
        MouseClipButton["Click Clip Button<br/>(Start/stop clip playback)"]
        MouseWaveform["Click Waveform<br/>(Click-to-jog, seek position)"]
        MouseDragTrim["Drag Trim Slider<br/>(Adjust trim IN/OUT points)"]
        MouseRightClick["Right-Click Button<br/>(Context menu: Edit, Delete, Copy)"]
        MouseDragDrop["Drag Audio File<br/>(Load file into button)"]
        MouseDragReorder["Drag Button<br/>(Reorder clips, 240ms hold time)"]
        MouseTabClick["Click Tab<br/>(Switch between 8 tabs)"]

        MouseClipButton --> Running
        MouseWaveform --> Running
        MouseDragTrim --> Running
        MouseRightClick --> Running
        MouseDragDrop --> Running
        MouseDragReorder --> Running
        MouseTabClick --> Running
    end

    subgraph "User Interactions: Keyboard"
        KeySpace["Space Bar<br/>(Stop currently playing clip)"]
        KeyNumbers["1-8 Keys<br/>(Switch tabs 1-8)"]
        KeyQWERTY["QWERTY Layout<br/>(48 button shortcuts)"]
        KeyArrows["Arrow Keys<br/>(Navigate grid up/down/left/right)"]
        KeyEnter["Enter/Return<br/>(Open ClipEditDialog for selected button)"]
        KeyCmdClick["Cmd+Click Waveform<br/>(Set trim IN point)"]
        KeyCmdShiftClick["Cmd+Shift+Click Waveform<br/>(Set trim OUT point)"]
        KeyBrackets["[ ] Keys<br/>(Set trim IN/OUT, restart playback)"]
        KeyAngleBrackets["< > Keys<br/>(Set trim IN/OUT via mouse buttons)"]
        KeyEscape["Escape<br/>(Close dialog, cancel operation)"]
        KeyCmdS["Cmd+S<br/>(Save current session)"]
        KeyCmdO["Cmd+O<br/>(Open session file picker)"]

        KeySpace --> Running
        KeyNumbers --> Running
        KeyQWERTY --> Running
        KeyArrows --> Running
        KeyEnter --> Running
        KeyCmdClick --> Running
        KeyCmdShiftClick --> Running
        KeyBrackets --> Running
        KeyAngleBrackets --> Running
        KeyEscape --> Running
        KeyCmdS --> Running
        KeyCmdO --> Running
    end

    subgraph "System Events: Audio Callbacks"
        AudioCallback["Audio Driver Callback<br/>(Every ~10ms @ 512 samples)"]
        ProcessBuffer["Process Audio Buffer<br/>(SDK renders audio)"]
        UpdatePosition["Update Transport Position<br/>(Atomic write, sample count)"]
        PostCallbacks["Post UI Callbacks<br/>(onClipStarted, onClipStopped)"]

        AudioCallback --> ProcessBuffer
        ProcessBuffer --> UpdatePosition
        UpdatePosition --> PostCallbacks
        PostCallbacks --> Running
    end

    subgraph "System Events: File Operations"
        FileOpen["File → Open Session<br/>(JSON file picker)"]
        FileSave["File → Save Session<br/>(Write JSON to disk)"]
        FileSaveAs["File → Save Session As<br/>(Choose new location)"]
        FileDropped["File Dropped on Grid<br/>(Drag-drop audio file)"]
        FileAutoSave["Auto-Save Timer<br/>(Every 5 minutes, configurable)"]

        FileOpen --> Running
        FileSave --> Running
        FileSaveAs --> Running
        FileDropped --> Running
        FileAutoSave --> Running
    end

    subgraph "System Events: UI Timers"
        Timer75fps["75fps UI Update Timer<br/>(13.3ms interval)"]
        UpdatePlayhead["Update Playhead Position<br/>(Read atomic transport position)"]
        UpdateButtonStates["Update Button States<br/>(Highlight playing clips)"]
        ProcessCallbackQueue["Process Callback Queue<br/>(UI thread safe callbacks)"]

        Timer75fps --> UpdatePlayhead
        Timer75fps --> UpdateButtonStates
        Timer75fps --> ProcessCallbackQueue
        UpdatePlayhead --> Running
        UpdateButtonStates --> Running
        ProcessCallbackQueue --> Running
    end

    subgraph "Session Loading Flow"
        SessionPicker["User Selects .occSession File"]
        ParseJSON["SessionManager Parses JSON"]
        ValidateSchema["Validate Session Schema v1.0"]
        LoadClips["Load Clip Metadata (960 clips)"]
        OpenAudioFiles["Open Audio Files (Background Thread)"]
        RenderWaveforms["Render Waveforms (Background Thread)"]
        RefreshUI["Refresh ClipGrid (Current Tab)"]

        SessionPicker --> ParseJSON
        ParseJSON --> ValidateSchema
        ValidateSchema --> LoadClips
        LoadClips --> OpenAudioFiles
        LoadClips --> RenderWaveforms
        OpenAudioFiles --> RefreshUI
        RenderWaveforms --> RefreshUI
        RefreshUI --> Running
    end

    subgraph "Environment Differences"
        macOS["macOS Build<br/>(CoreAudio driver)"]
        Windows["Windows Build<br/>(WASAPI driver)"]
        Debug["Debug Build<br/>(Sanitizers enabled, logging verbose)"]
        Release["Release Build<br/>(Optimizations enabled, logging minimal)"]

        macOS -.-> InitAudio
        Windows -.-> InitAudio
        Debug -.-> JUCEInit
        Release -.-> JUCEInit
    end

    style Running fill:#ccffcc,stroke:#00ff00,stroke-width:3px
    style Shutdown fill:#ffcccc,stroke:#ff0000,stroke-width:2px
    style AudioCallback fill:#ffddaa,stroke:#ff8800,stroke-width:2px
    style Timer75fps fill:#cceeff,stroke:#0088ff,stroke-width:2px
