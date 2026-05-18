%% Orpheus SDK Component Map
%% Detailed component breakdown with ORP121 additions and shmui-juce v2.0.0 classes
%% Last updated: 2026-01-25

classDiagram
    class SessionGraph {
        +vector~Track~ tracks
        +TempoInfo tempo
        +string sessionName
        +addTrack(Track)
        +removeTrack(id)
        +getTrack(id) Track
        +getTempo() TempoInfo
        +setTempo(bpm, timeSig)
    }

    class Track {
        +string id
        +string name
        +vector~Clip~ clips
        +TrackType type
        +addClip(Clip)
        +removeClip(id)
    }

    class Clip {
        +string id
        +string name
        +string audioFilePath
        +uint64_t trimInSamples
        +uint64_t trimOutSamples
        +float fadeInSeconds
        +float fadeOutSeconds
        +FadeCurve fadeInCurve
        +FadeCurve fadeOutCurve
        +float gainDb
        +bool loopEnabled
    }

    class ITransportController {
        <<interface>>
        +startClip(clipId) ErrorCode
        +stopClip(clipId) ErrorCode
        +stopAllClips() ErrorCode
        +updateClipGain(clipId, gainDb) ErrorCode
        +updateClipTrim(clipId, trimIn, trimOut) ErrorCode
        +setClipLoopMode(clipId, enabled) ErrorCode
        +processAudio(buffer, frameCount) void
        +processCallbacks() void
    }

    class TransportController {
        -map~string,ActiveClip~ activeClips
        -IAudioDriver* driver
        -SessionGraph* session
        -SPSCCallbackQueue callbackQueue
        +startClip(clipId) ErrorCode
        +stopClip(clipId) ErrorCode
        +processAudio(buffer, frameCount) void
        +processCallbacks() void
        +postCallback(callback) void
    }

    class SPSCCallbackQueue {
        <<ORP121 C-03>>
        -array~function,256~ ring
        -atomic~size_t~ writeIndex
        -atomic~size_t~ readIndex
        +push(callback) bool
        +pop() function
        +isEmpty() bool
    }

    class ActiveClip {
        +atomic~uint64_t~ currentFrame
        +atomic~float~ gainLinear
        +atomic~bool~ loopEnabled
        +shared_ptr~IAudioFileReader~ reader
        +uint64_t trimInSamples
        +uint64_t trimOutSamples
        +FadeState fadeState
    }

    class IAudioFileReader {
        <<interface>>
        +read(buffer, frameCount) uint64_t
        +seek(frame) ErrorCode
        +getSampleRate() uint32_t
        +getChannelCount() uint32_t
        +getTotalFrames() uint64_t
    }

    class IAudioDriver {
        <<interface>>
        +initialize(config) ErrorCode
        +start() ErrorCode
        +stop() ErrorCode
        +getDeviceList() vector~Device~
        +setAudioCallback(callback) void
    }

    class RoutingConfig {
        <<ORP121 Q-03>>
        +uint8_t numInputChannels
        +uint8_t numOutputChannels
        +uint8_t numGroups
        +uint32_t sampleRate
        +HeadroomMode headroomMode
        +bool enableTruePeak
    }

    class HeadroomMode {
        <<enumeration>>
        <<ORP121 Q-05>>
        None
        PerGroup
        Global
        Logarithmic
    }

    class IRoutingMatrix {
        <<interface>>
        +setRouting(src, dst, gain) ErrorCode
        +setChannelGain(channel, gainDb) ErrorCode
        +setChannelPan(channel, pan) ErrorCode
        +process(inputs, outputs, frameCount) void
        +getChannelMeter(channel) MeterData
        +getTruePeak(channel) float
    }

    class RoutingMatrix {
        -vector~ChannelState~ channels
        -vector~GroupState~ groups
        -MasterState master
        -RoutingConfig config
        +setRouting(src, dst, gain) ErrorCode
        +process(inputs, outputs, frameCount) void
        -processRouting(frame) void
        -processMetering(buffer, frameCount) void
        -getHeadroomCompensation(group) float
    }

    class ChannelState {
        +GainSmoother gain
        +GainSmoother panLeft
        +GainSmoother panRight
        +TruePeakMeter truePeak
        +float peakLevel
        +uint8_t groupAssignment
    }

    class GroupState {
        +GainSmoother gain
        +TruePeakMeter truePeak
        +float peakLevel
        +vector~float~ bufferL
        +vector~float~ bufferR
    }

    class GainSmoother {
        <<ORP121 C-01>>
        -float currentGain
        -float targetGain
        -float rampSamples
        +setTarget(gainDb, rampMs) void
        +process() float
        +isRamping() bool
        +MAX_GAIN_DB = 12.0f
    }

    class TruePeakMeter {
        <<ORP121 Q-04>>
        -array~float,12~ history
        -size_t historyIndex
        -float peakHold
        +process(sample) float
        +processBuffer(buffer, frames) float
        +getPeak() float
        +reset() void
        +OVERSAMPLE_FACTOR = 4
        +TAPS_PER_PHASE = 12
    }

    class SoftKneeLimiter {
        <<ORP121 C-02>>
        +process(sample) float
        +THRESHOLD = 0.794f
        +KNEE_WIDTH = 0.3f
        +CEILING = 0.9999f
    }

    class shmui_AudioAnalyzer {
        <<shmui v2.0.0>>
        <<thread-safe>>
        -fftData
        -rmsBuffer
        -frequencyBands
        +pushSamples(buffer, frames) void
        +getFFTData() FFTData
        +getRMS() float
        +getBandLevels() BandLevels
    }

    class shmui_WaveformVisualizer {
        <<shmui v2.0.0>>
        -AudioAnalyzer* analyzer
        -WaveformVariant variant
        +setSource(AudioSource) void
        +setVariant(Variant) void
        +paint(Graphics) void
    }

    class shmui_WaveformEditor {
        <<shmui v2.0.0>>
        -selection trimIn/trimOut
        -fadeIn/fadeOut
        +setAudioFile(File) void
        +getTrimRange() Range
        +onSelectionChanged callback
    }

    class shmui_BarVisualizer {
        <<shmui v2.0.0>>
        -AudioAnalyzer* analyzer
        -barCount
        -stateAnimations
        +setBarCount(count) void
        +paint(Graphics) void
    }

    class shmui_LevelMeter {
        <<shmui v2.0.0>>
        -MeterType type
        -peakHold
        +setLevel(dB) void
        +setType(VU/PPM/Peak) void
        +paint(Graphics) void
    }

    class shmui_Button {
        <<shmui v2.0.0>>
        -ButtonStyle style
        -ButtonSize size
        +setStyle(Primary/Secondary/Ghost) void
        +setSize(Small/Medium/Large) void
        +onClick callback
    }

    class shmui_TransportButton {
        <<shmui v2.0.0>>
        -TransportState state
        +setState(Play/Pause/Stop/Record) void
        +getState() TransportState
    }

    class shmui_ClipButton {
        <<shmui v2.0.0>>
        -ClipState state
        -Clip* clip
        +trigger() void
        +stop() void
        +getState() ClipState
        +onStateChanged callback
    }

    SessionGraph "1" *-- "many" Track
    Track "1" *-- "many" Clip

    TransportController ..|> ITransportController
    TransportController "1" --> "1" SessionGraph
    TransportController "1" --> "1" IAudioDriver
    TransportController "1" *-- "many" ActiveClip
    TransportController "1" *-- "1" SPSCCallbackQueue
    ActiveClip "1" --> "1" IAudioFileReader

    RoutingMatrix ..|> IRoutingMatrix
    RoutingMatrix "1" --> "1" RoutingConfig
    RoutingMatrix "1" *-- "many" ChannelState
    RoutingMatrix "1" *-- "many" GroupState
    RoutingMatrix --> SoftKneeLimiter : uses

    ChannelState "1" *-- "1" GainSmoother
    ChannelState "1" *-- "1" TruePeakMeter
    GroupState "1" *-- "1" GainSmoother
    GroupState "1" *-- "1" TruePeakMeter

    RoutingConfig --> HeadroomMode : uses

    TransportController --> RoutingMatrix : processes through

    shmui_WaveformVisualizer --> shmui_AudioAnalyzer : uses
    shmui_WaveformEditor --> shmui_AudioAnalyzer : uses
    shmui_BarVisualizer --> shmui_AudioAnalyzer : uses
    shmui_LevelMeter --> shmui_AudioAnalyzer : uses

    shmui_TransportButton --|> shmui_Button
    shmui_ClipButton --|> shmui_Button

    shmui_ClipButton --> Clip : displays state
