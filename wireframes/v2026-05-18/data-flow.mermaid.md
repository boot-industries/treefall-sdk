%% Orpheus SDK Data Flow
%% Lock-free callback queue, true-peak metering, headroom compensation (ORP121), shmui visualization
%% Last updated: 2026-01-25

sequenceDiagram
    participant User
    participant UI as UI Thread<br/>(Message Thread)
    participant Queue as SPSC Queue<br/>(Lock-Free Ring)
    participant Transport as TransportController
    participant Routing as RoutingMatrix
    participant Meter as TruePeakMeter
    participant Analyzer as shmui::AudioAnalyzer<br/>(Thread-Safe)
    participant Vis as shmui Visualizers<br/>(Message Thread)
    participant Audio as Audio Thread
    participant Driver as Audio Driver

    Note over User,Driver: Initialization Phase

    User->>UI: Load Session
    UI->>Transport: new TransportController(session, driver)
    Transport->>Routing: new RoutingMatrix(config)
    Note over Routing: config.sampleRate = 48000<br/>config.headroomMode = Logarithmic<br/>config.enableTruePeak = true
    Routing->>Meter: Initialize per-channel meters
    Transport->>Driver: initialize(config)
    Driver-->>Transport: Success

    Note over User,Driver: Start Clip Flow (UI Thread)

    User->>UI: Click Play Button
    UI->>Transport: startClip(clipId)
    Transport->>Transport: Create ActiveClip<br/>currentFrame = trimIn<br/>gainLinear = 1.0<br/>loopEnabled = true
    Transport-->>UI: Success

    Note over User,Driver: Audio Processing Loop (Audio Thread)

    loop Every Audio Callback (~5ms @ 512 samples)
        Driver->>Audio: Audio callback
        Audio->>Transport: processAudio(buffer, frameCount)

        loop For each active clip
            Transport->>Transport: Read samples from AudioFileReader
            Transport->>Transport: Apply fade IN/OUT curves
            Transport->>Transport: Apply clip gain (atomic read)
            Transport->>Routing: Add to input buffer
        end

        Note over Routing: ORP121 Routing Pipeline

        Routing->>Routing: Step 1: Channel Processing
        Note right of Routing: Apply GainSmoother<br/>Apply constant-power pan<br/>(cos/sin coefficients)

        Routing->>Meter: Step 2: Channel Metering
        Note right of Meter: 4x oversample<br/>48-tap polyphase FIR<br/>Detect inter-sample peaks

        Routing->>Routing: Step 3: Group Mixing
        Note right of Routing: Sum channels to groups<br/>Apply HeadroomMode<br/>(Logarithmic: -10*log10(n))

        Routing->>Meter: Step 4: Group Metering
        Meter-->>Routing: Group true-peak values

        Routing->>Routing: Step 5: Master Output
        Note right of Routing: Sum groups to master<br/>Apply soft-knee limiter<br/>(threshold: -2 dBFS)

        Routing->>Meter: Step 6: Master Metering
        Routing-->>Transport: Processed output buffer

        alt Clip reached end
            alt Loop enabled
                Transport->>Transport: Seek to trimIn
                Transport->>Queue: postCallback(onClipLooped)
                Note over Queue: Lock-free push<br/>No mutex!
            else No loop
                Transport->>Transport: Remove from activeClips
                Transport->>Queue: postCallback(onClipFinished)
            end
        end

        Transport-->>Audio: Buffer filled
        Audio-->>Driver: Return from callback
    end

    Note over User,Driver: Callback Processing (UI Thread, 10Hz Timer)

    UI->>UI: Timer callback
    UI->>Queue: processCallbacks()

    loop While queue not empty
        Queue->>Queue: Read from ring buffer<br/>(atomic readIndex)
        Queue-->>UI: callback function
        UI->>UI: Execute callback
        UI->>User: Update UI state
    end

    Note over User,Driver: Gain Update Flow (Glitch-Free)

    User->>UI: Adjust gain slider
    UI->>Transport: updateClipGain(clipId, -6.0dB)
    Transport->>Transport: gainLinear = pow(10, -6.0/20)
    Transport->>Transport: Atomic write to ActiveClip
    Transport-->>UI: Success
    Note over Audio: Audio thread reads new gain<br/>on next callback (no lock)

    Note over User,Driver: Metering Query Flow

    UI->>Routing: getTruePeak(channel)
    Routing->>Meter: getPeak()
    Meter-->>Routing: -3.2 dBTP
    Routing-->>UI: MeterData with true-peak

    Note over User,Driver: Session Save Flow

    User->>UI: Save Session
    UI->>Transport: Get session state
    Transport-->>UI: SessionGraph
    UI->>UI: Serialize to JSON
    UI-->>User: Session saved

    Note over User,Driver: shmui Visualization Flow (Thread-Safe)

    Note over Analyzer: AudioAnalyzer runs on Audio Thread
    Audio->>Analyzer: pushSamples(buffer, frameCount)
    Note right of Analyzer: Lock-free ring buffer<br/>Computes FFT, RMS, bands

    Note over Vis: Visualizers poll on Message Thread (60 FPS)
    Vis->>Analyzer: getFFTData() / getRMS() / getBandLevels()
    Note right of Analyzer: Atomic read operations<br/>No locks required
    Analyzer-->>Vis: FFTData / RMS / BandLevels
    Vis->>Vis: Update visualization state
    Vis->>User: Paint waveform/bars/orb/meters

    Note over Analyzer,Vis: Thread boundary:<br/>Audio Thread writes → Message Thread reads<br/>No mutexes, no priority inversion
