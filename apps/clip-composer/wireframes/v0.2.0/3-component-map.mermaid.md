%% Component Map for Orpheus Clip Composer v0.2.0
%% Shows detailed component relationships, dependencies, and public APIs
%% Last Updated: October 31, 2025

classDiagram
    class MainComponent {
        +juce::Component
        -ClipGrid* clipGrid
        -TransportControls* transportControls
        -RoutingPanel* routingPanel
        -TabSwitcher* tabSwitcher
        -PerformanceMonitor* perfMonitor
        +resized()
        +paint(Graphics)
        +keyPressed(KeyEvent)
    }

    class ClipGrid {
        +juce::Component
        -ClipButton[120] buttons
        -SessionManager* sessionManager
        -AudioEngine* audioEngine
        -int currentTab
        +setCurrentTab(int)
        +refreshButtons()
        +highlightPlayingClip(ClipHandle)
        +clearAllHighlights()
        +onClipTriggered(row, col)
    }

    class ClipButton {
        +juce::Button
        -ClipMetadata metadata
        -bool isPlaying
        -juce::Colour buttonColor
        +setClip(ClipMetadata)
        +setPlaying(bool)
        +getClipHandle() ClipHandle
        +mouseDown(MouseEvent)
        +paint(Graphics)
    }

    class TabSwitcher {
        +juce::Component
        -int currentTab
        -std::function~void(int)~ onTabChanged
        +setCurrentTab(int)
        +getCurrentTab() int
        +keyPressed(KeyEvent)
    }

    class TransportControls {
        +juce::Component
        -AudioEngine* audioEngine
        -juce::Label positionLabel
        -juce::TextButton stopAllButton
        -juce::TextButton panicButton
        +updatePosition(TransportPosition)
        +onStopAllClicked()
        +onPanicClicked()
    }

    class WaveformDisplay {
        +juce::Component
        -std::vector~float~ waveformData
        -int64_t playheadPosition
        -int64_t trimIn
        -int64_t trimOut
        -bool isPlaying
        +setWaveform(vector~float~)
        +setPlayheadPosition(int64_t)
        +setTrimPoints(int64_t in, int64_t out)
        +mouseDown(MouseEvent)
        +paint(Graphics)
        +timerCallback()
    }

    class ClipEditDialog {
        +juce::DialogWindow
        -WaveformDisplay* waveformDisplay
        -juce::Slider trimInSlider
        -juce::Slider trimOutSlider
        -juce::Slider fadeInSlider
        -juce::Slider fadeOutSlider
        -juce::Slider gainSlider
        -juce::TextButton playButton
        -juce::TextButton stopButton
        -ClipMetadata currentClip
        +loadClip(ClipMetadata)
        +saveClip() ClipMetadata
        +onTrimInChanged()
        +onTrimOutChanged()
        +onPlayClicked()
        +keyPressed(KeyEvent)
    }

    class ColorSwatchPicker {
        +juce::Component
        -std::vector~juce::Colour~ palette
        -std::function~void(Colour)~ onColorSelected
        +showPicker(Colour current)
        +mouseDown(MouseEvent)
    }

    class PreviewPlayer {
        +juce::Component
        -AudioEngine* audioEngine
        -ClipHandle previewHandle
        +loadFile(juce::File)
        +play()
        +stop()
    }

    class AudioSettingsDialog {
        +juce::DialogWindow
        -juce::ComboBox driverComboBox
        -juce::ComboBox deviceComboBox
        -juce::ComboBox bufferSizeComboBox
        -juce::ComboBox sampleRateComboBox
        +showDialog()
        +saveSettings()
    }

    class SessionManager {
        -std::vector~ClipMetadata~ clips
        -RoutingConfiguration routing
        -SessionPreferences preferences
        -juce::File currentSessionFile
        +loadSession(File) Result
        +saveSession(File) Result
        +getClipAtButton(int tab, int button) ClipMetadata
        +setClipAtButton(int tab, int button, ClipMetadata)
        +getAllClips() vector~ClipMetadata~
        +getRoutingConfig() RoutingConfiguration
        +setRoutingConfig(RoutingConfiguration)
        +validateSession(JSON) Result
    }

    class ClipMetadata {
        +ClipHandle handle
        +std::string name
        +std::string filePath
        +int buttonIndex
        +int tabIndex
        +int clipGroup
        +int64_t trimIn
        +int64_t trimOut
        +float gainDb
        +juce::Colour color
        +int fadeInSamples
        +int fadeOutSamples
        +bool loopEnabled
        +PlaybackMode playbackMode
        +std::vector~CuePoint~ cuePoints
        +toJSON() JSON
        +fromJSON(JSON) ClipMetadata
    }

    class AudioEngine {
        -orpheus::ITransportController* transport
        -orpheus::IAudioFileReader* fileReader
        -orpheus::IRoutingMatrix* routingMatrix
        -orpheus::IPerformanceMonitor* perfMonitor
        -juce::AudioDeviceManager deviceManager
        +initialize() Result
        +startClip(ClipHandle) Result
        +stopClip(ClipHandle) Result
        +stopAllClips() Result
        +seekClip(ClipHandle, int64_t position) Result
        +updateClipMetadata(ClipHandle, ClipMetadata) Result
        +processBlock(AudioBuffer, MidiBuffer)
        +getTransportPosition() TransportPosition
        +getCPUUsage() float
        +getLatency() double
    }

    class RoutingConfiguration {
        +ClipGroup[4] groups
        +float masterGain
        +bool masterMute
        +toJSON() JSON
        +fromJSON(JSON) RoutingConfiguration
    }

    class ClipGroup {
        +std::string name
        +float gainDb
        +bool mute
        +bool solo
    }

    class TransportPosition {
        +int64_t samplePosition
        +double timeSeconds
        +int64_t trimIn
        +int64_t trimOut
        +bool isPlaying
        +ClipHandle activeClip
    }

    class InterLookAndFeel {
        +juce::LookAndFeel_V4
        +drawButtonBackground(Graphics, Button)
        +drawLabel(Graphics, Label)
        +drawLinearSlider(Graphics, Slider)
        +getFont() Font
    }

    %% UI Component Relationships
    MainComponent --> ClipGrid
    MainComponent --> TransportControls
    MainComponent --> TabSwitcher
    MainComponent --> InterLookAndFeel

    ClipGrid --> ClipButton : contains 120
    ClipGrid --> SessionManager : queries
    ClipGrid --> AudioEngine : triggers

    ClipButton --> ClipMetadata : stores

    ClipEditDialog --> WaveformDisplay : contains
    ClipEditDialog --> ColorSwatchPicker : uses
    ClipEditDialog --> PreviewPlayer : uses
    ClipEditDialog --> ClipMetadata : edits

    TabSwitcher --> ClipGrid : notifies

    %% Application Logic Relationships
    SessionManager --> ClipMetadata : manages
    SessionManager --> RoutingConfiguration : persists
    AudioEngine --> SessionManager : queries metadata

    RoutingConfiguration --> ClipGroup : contains 4

    %% Audio Engine Relationships
    AudioEngine --> TransportPosition : publishes
    TransportControls --> AudioEngine : controls
    ClipGrid --> AudioEngine : starts/stops clips
    ClipEditDialog --> AudioEngine : preview playback

    %% Shared Utilities
    ClipGrid --> InterLookAndFeel : applies styling
    ClipButton --> InterLookAndFeel : applies styling
    TransportControls --> InterLookAndFeel : applies styling
    WaveformDisplay --> InterLookAndFeel : applies styling

    %% Data Flow
    SessionManager ..> ClipGrid : refreshes on load
    AudioEngine ..> TransportControls : position updates
    AudioEngine ..> ClipGrid : playback state callbacks
    AudioEngine ..> WaveformDisplay : playhead updates
