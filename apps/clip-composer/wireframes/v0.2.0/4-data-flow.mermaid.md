%% Data Flow for Orpheus Clip Composer v0.2.0
%% Shows sequence diagrams for key workflows: clip trigger, session save, waveform update, transport position
%% Last Updated: October 31, 2025

sequenceDiagram
    participant User
    participant ClipButton
    participant ClipGrid
    participant SessionManager
    participant AudioEngine
    participant SDK_Transport as SDK (ITransportController)
    participant SDK_AudioThread as SDK Audio Thread
    participant WaveformDisplay

    Note over User,WaveformDisplay: Flow 1: Clip Trigger (User Click → Audio Playback)

    User->>ClipButton: Click button
    ClipButton->>ClipGrid: mouseDown(event)
    ClipGrid->>SessionManager: getClipAtButton(tab, buttonIndex)
    SessionManager-->>ClipGrid: ClipMetadata (handle, trim, fade, gain)
    ClipGrid->>AudioEngine: startClip(handle)
    AudioEngine->>SDK_Transport: enqueue(StartClipCommand)
    Note over SDK_Transport: Lock-free queue enqueue
    SDK_Transport-->>AudioEngine: SessionGraphError::OK
    AudioEngine-->>ClipGrid: Result::OK
    ClipGrid->>ClipButton: Set visual state (pending)

    Note over SDK_AudioThread: Audio Thread Processing (10ms later)

    SDK_AudioThread->>SDK_Transport: processAudio(outputs, numSamples)
    SDK_Transport->>SDK_Transport: Dequeue StartClipCommand
    SDK_Transport->>SDK_Transport: Start clip playback
    SDK_Transport->>SDK_Transport: Enqueue onClipStarted callback

    Note over AudioEngine: Message Thread Callback (13ms later @ 75fps)

    SDK_Transport->>AudioEngine: onClipStarted(handle, position)
    AudioEngine->>ClipGrid: highlightPlayingClip(handle)
    ClipGrid->>ClipButton: setPlaying(true)
    ClipButton->>ClipButton: Repaint (green border)

    Note over User,WaveformDisplay: Flow 2: Session Save (Metadata → JSON → File)

    User->>SessionManager: Save button clicked
    SessionManager->>SessionManager: getAllClips()
    SessionManager->>SessionManager: getRoutingConfig()
    SessionManager->>SessionManager: getPreferences()
    SessionManager->>SessionManager: Serialize to JSON
    SessionManager->>SessionManager: Write to file (blocking I/O)
    SessionManager-->>User: Show confirmation dialog

    Note over User,WaveformDisplay: Flow 3: Waveform Update (File Load → Background Render → UI Display)

    User->>SessionManager: Load session
    SessionManager->>SessionManager: Parse JSON
    SessionManager->>AudioEngine: loadAudioFile(filePath)
    AudioEngine->>SDK_Transport: openFile(filePath)
    SDK_Transport-->>AudioEngine: FileInfo (sampleRate, numChannels, duration)
    AudioEngine->>AudioEngine: Enqueue waveform render task
    Note over AudioEngine: Background Thread (File I/O)
    AudioEngine->>AudioEngine: Read audio samples
    AudioEngine->>AudioEngine: Downsample for display (peak detection)
    AudioEngine->>AudioEngine: Post UI update via MessageManager
    Note over WaveformDisplay: Message Thread
    AudioEngine->>WaveformDisplay: setWaveform(waveformData)
    WaveformDisplay->>WaveformDisplay: Repaint with new waveform

    Note over User,WaveformDisplay: Flow 4: Transport Position Update (Audio Thread → UI)

    SDK_AudioThread->>SDK_Transport: processAudio(outputs, numSamples)
    SDK_Transport->>SDK_Transport: Update sample position (atomic)
    Note over AudioEngine: Message Thread (75fps timer)
    AudioEngine->>SDK_Transport: getTransportPosition()
    SDK_Transport-->>AudioEngine: TransportPosition (samplePosition, isPlaying)
    AudioEngine->>WaveformDisplay: setPlayheadPosition(samplePosition)
    WaveformDisplay->>WaveformDisplay: Repaint playhead (red line)
