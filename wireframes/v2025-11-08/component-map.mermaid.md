%% Orpheus SDK Component Map
%% Detailed component breakdown showing module relationships, dependencies, and public APIs
%% Last updated: 2025-11-08

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
        +validate() bool
    }

    class Track {
        +string id
        +string name
        +vector~Clip~ clips
        +TrackType type
        +addClip(Clip)
        +removeClip(id)
        +getClip(id) Clip
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
        +setOnClipFinished(callback) void
        +setOnClipLooped(callback) void
    }

    class TransportController {
        -map~string, ActiveClip~ activeClips
        -IAudioDriver* driver
        -SessionGraph* session
        -LockFreeQueue~Event~ eventQueue
        +startClip(clipId) ErrorCode
        +stopClip(clipId) ErrorCode
        +processAudio(buffer, frameCount) void
        -applyFades(buffer, clip) void
        -applyGain(buffer, clip) void
        -checkLoopPoint(clip) void
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
        +getFilePath() string
    }

    class LibsndfileReader {
        -SNDFILE* sfHandle
        -SF_INFO sfInfo
        -string filePath
        +read(buffer, frameCount) uint64_t
        +seek(frame) ErrorCode
        +getSampleRate() uint32_t
    }

    class IAudioDriver {
        <<interface>>
        +initialize(config) ErrorCode
        +start() ErrorCode
        +stop() ErrorCode
        +getDeviceList() vector~Device~
        +setAudioCallback(callback) void
        +getSampleRate() uint32_t
        +getBufferSize() uint32_t
    }

    class CoreAudioDriver {
        -AudioDeviceID deviceId
        -AudioStreamBasicDescription format
        -function~AudioCallback~ callback
        +initialize(config) ErrorCode
        +start() ErrorCode
        +stop() ErrorCode
        -audioIOProc(buffer) OSStatus
    }

    class DummyDriver {
        -uint32_t sampleRate
        -uint32_t bufferSize
        -bool running
        +initialize(config) ErrorCode
        +start() ErrorCode
        +stop() ErrorCode
        -simulationThread() void
    }

    class IRoutingMatrix {
        <<interface>>
        +setRouting(srcChannel, dstChannel, gain) ErrorCode
        +clearRouting(srcChannel, dstChannel) ErrorCode
        +setChannelGain(channel, gainDb) ErrorCode
        +process(inputs, outputs, frameCount) void
        +getChannelMeter(channel) MeterData
    }

    class RoutingMatrix {
        -vector~vector~float~~ routingGains
        -vector~GainSmoother~ channelGains
        -vector~MeterState~ meters
        +setRouting(src, dst, gain) ErrorCode
        +process(inputs, outputs, frameCount) void
        -processRouting(frame) void
        -updateMeters(buffer, frameCount) void
    }

    class GainSmoother {
        -float currentGain
        -float targetGain
        -float rampSamples
        +setTarget(gainDb, rampMs) void
        +process(buffer, frameCount) void
        +isRamping() bool
    }

    class SessionJSON {
        +loadSession(path) SessionGraph
        +saveSession(path, session) ErrorCode
        +validateSchema(json) bool
        -parseTrack(json) Track
        -parseClip(json) Clip
        -serializeTrack(track) json
        -serializeClip(clip) json
    }

    class AbiVersion {
        +uint32_t major
        +uint32_t minor
        +uint32_t patch
        +isCompatible(other) bool
        +getVersionString() string
        +fromString(version) AbiVersion
    }

    class Contract {
        +validateCommand(json) bool
        +validateEvent(json) bool
        +getSchema(type) JSONSchema
        +getAvailableCommands() vector~string~
        +getAvailableEvents() vector~string~
    }

    class ClientBroker {
        -IEngineDriver* activeDriver
        -vector~IEngineDriver*~ availableDrivers
        +connect(config) ErrorCode
        +sendCommand(command) Response
        +subscribe(event, callback) void
        +disconnect() void
        -selectBestDriver() IEngineDriver*
    }

    class NativeDriver {
        -napi_env env
        -TransportController* transport
        -SessionGraph* session
        +initialize(env) ErrorCode
        +executeCommand(json) json
        +subscribeEvent(callback) void
    }

    class ServiceDriver {
        -HTTPServer* server
        -WebSocket* wsConnection
        -Process* minhostProcess
        +initialize(config) ErrorCode
        +executeCommand(json) json
        -handleHTTPRequest(req, res) void
        -handleWebSocketMessage(msg) void
    }

    class WASMDriver {
        -Worker* webWorker
        -Module* wasmModule
        +initialize(config) ErrorCode
        +executeCommand(json) json
        -postMessageToWorker(msg) void
        -handleWorkerMessage(msg) void
    }

    SessionGraph "1" *-- "many" Track
    Track "1" *-- "many" Clip

    TransportController ..|> ITransportController
    TransportController "1" --> "1" SessionGraph
    TransportController "1" --> "1" IAudioDriver
    TransportController "1" *-- "many" ActiveClip
    ActiveClip "1" --> "1" IAudioFileReader

    LibsndfileReader ..|> IAudioFileReader
    CoreAudioDriver ..|> IAudioDriver
    DummyDriver ..|> IAudioDriver

    RoutingMatrix ..|> IRoutingMatrix
    RoutingMatrix "1" *-- "many" GainSmoother

    SessionJSON --> SessionGraph : creates

    ClientBroker --> NativeDriver : uses
    ClientBroker --> ServiceDriver : uses
    ClientBroker --> WASMDriver : uses

    NativeDriver --> TransportController : wraps
    NativeDriver --> SessionGraph : wraps
    ServiceDriver --> TransportController : wraps via minhost
    WASMDriver --> TransportController : wraps via WASM

    Contract --> SessionJSON : validates
    AbiVersion --> Contract : versioning
