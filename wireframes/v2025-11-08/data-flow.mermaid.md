%% Orpheus SDK Data Flow
%% Detailed visualization of how data moves through the system including thread interactions and event flows
%% Last updated: 2025-11-08

sequenceDiagram
    participant User
    participant UI as UI Thread<br/>(JUCE/React/CLI)
    participant Transport as TransportController
    participant Active as ActiveClip<br/>(Lock-Free State)
    participant Audio as Audio Thread<br/>(Real-Time Callback)
    participant Driver as Audio Driver<br/>(CoreAudio/WASAPI)
    participant Reader as AudioFileReader<br/>(Pre-loaded File)

    Note over User,Reader: Initialization Phase

    User->>UI: Load Session
    UI->>UI: Parse JSON
    UI->>Transport: new TransportController(session, driver)
    Transport->>Driver: initialize(config)
    Driver-->>Transport: Success

    Note over User,Reader: Start Clip Flow (UI → Audio Thread)

    User->>UI: Click Play Button
    UI->>UI: Load audio file if needed
    UI->>Reader: new AudioFileReader(filePath)
    Reader-->>UI: File loaded into memory

    UI->>Transport: startClip(clipId)
    Transport->>Transport: Create ActiveClip structure
    Transport->>Active: Set initial state (atomics)<br/>currentFrame=trimIn<br/>gainLinear=1.0<br/>loopEnabled=true
    Active-->>Transport: State ready
    Transport->>Transport: Add to activeClips map
    Transport-->>UI: Success (ErrorCode::OK)

    Note over User,Reader: Real-Time Audio Processing (Audio Thread Loop)

    loop Every Audio Callback (e.g., 256 frames @ 48kHz = ~5.3ms)
        Driver->>Audio: Audio callback requested
        Audio->>Transport: processAudio(buffer, frameCount)

        loop For each active clip
            Transport->>Active: Read atomic state<br/>currentFrame, gainLinear, loopEnabled
            Active-->>Transport: Current state

            Transport->>Reader: read(buffer, frameCount)
            Reader-->>Transport: Audio samples

            Transport->>Transport: Apply fade IN/OUT<br/>if within fade region

            Transport->>Transport: Apply clip gain<br/>multiply by gainLinear

            Transport->>Transport: Check loop point<br/>if currentFrame >= trimOut

            alt Loop enabled and reached end
                Transport->>Active: currentFrame = trimIn (atomic)
                Transport->>Transport: Queue "ClipLooped" event
            else Reached end without loop
                Transport->>Transport: Stop clip, remove from active
                Transport->>Transport: Queue "ClipFinished" event
            end

            Transport->>Active: Increment currentFrame (atomic)
        end

        Transport->>Transport: Mix all clips into output buffer
        Transport-->>Audio: Buffer filled
        Audio-->>Driver: Return from callback
        Driver->>Driver: Send buffer to hardware
    end

    Note over User,Reader: Event Notification (Audio → UI Thread)

    Transport->>UI: Timer callback (10Hz on UI thread)
    UI->>Transport: Poll event queue
    Transport-->>UI: ClipFinished event
    UI->>User: Update UI (button state)
    UI->>UI: Invoke registered callbacks

    Note over User,Reader: Stop Clip Flow (UI Thread)

    User->>UI: Click Stop Button
    UI->>Transport: stopClip(clipId)
    Transport->>Transport: Remove from activeClips map
    Transport-->>UI: Success (ErrorCode::OK)
    UI->>User: Update UI

    Note over User,Reader: Update Gain Flow (UI → Audio Thread, No Glitches)

    User->>UI: Adjust gain slider
    UI->>Transport: updateClipGain(clipId, -6.0dB)
    Transport->>Transport: Convert dB to linear<br/>gainLinear = pow(10, -6.0/20)
    Transport->>Active: Write gainLinear (atomic)
    Active-->>Transport: State updated
    Transport-->>UI: Success

    Note over Audio,Driver: Audio thread reads new gain<br/>on next callback (no interruption)

    Note over User,Reader: Session Save Flow

    User->>UI: Save Session
    UI->>Transport: Get current session state
    Transport-->>UI: SessionGraph
    UI->>UI: Serialize to JSON<br/>including all clip metadata
    UI->>UI: Write to file
    UI-->>User: Session saved
