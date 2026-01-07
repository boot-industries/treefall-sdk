# Interaction Model Specification

## Core Interactions

### Grid Interface
- **Grid Timing Integration**
  * Display Thread Management
    - 60fps refresh target (16.7ms)
    - Degradable under load
    - Format-aware time divisions
    - Emergency protocol aware
- **Clip Group Controls**
  * Group Settings Panel
    - Time Division Selection
      * Frame Rate Dropdown
        - Blackburst (75fps)
        - Film (24fps)
        - NTSC-DF (29.97fps drop)
        - NTSC (29.97fps non-drop)
        - PAL (30fps)
        - HD (60fps)
        - Musical Mode (120 BPM default)
        - Current selection indicator
        - Conflict warning display
        - Override status feedback
      * Visual Feedback
        - Current mode indicator
        - Override status
        - Clip tempo conflicts
    - Group Assignment (A-D)
    - Choke Group Settings
    - Level Control
    - Format Settings
- **Clip Operations**
  * Button Types
    - Stereo Clip (default)
      * Time Division Display
        - Inherits group settings
        - Shows manual override
        - Visual tempo conflict indicator
      * Standard stereo operation
      * Level control
      * Choke enabled/disabled
      * Standard metering
    - Mono Clip (auto-detected)
      * Automatic stereo expansion
      * Level control
      * Pan position control
      * Mono source metering
    * Button Sizing
      - Standard: Single grid cell
      - Stretched: Up to 4 cells
      - Stretch Directions:
        * Horizontal expansion
        * Vertical expansion
        * Combined expansion
      - Behavior Preservation:
        * Maintains FIFO operation
        * Preserves group assignment
        * Retains all functionality
      - Visual Feedback:
        * Clear size indication
        * Stretch handles
        * Selection highlighting
        * Group color coding
   * Trigger Modes
     - Single click: Play/Stop
     - Double click: Edit
     - Right click: Context menu
     - Drag: Move/Copy
   * Selection Modes
     - Single select
     - Multi-select (Shift/Ctrl)
     - Group select
     - Tab select

- **Button Operations**
  * Single trigger
  * Multi-select
  * Group operations
  * Marker management
  * Drag-and-drop

- **Tab Management**
  * Tab switching
  * Layout persistence
  * State tracking
  * Error indication

### Transport Controls
- **Playback Control**
  * Play/Stop
  * Position scrubbing
  * Beat/Bar navigation
  * Emergency stop

- **Monitoring**
  * Level control
  * Audition (Import/Edit)
  * Meter display
  * Status feedback

- **Tempo Control**
  * BPM display/entry
  * External sync indicator
  * Tap tempo
  * Beat flash indicator
  * Clip sync controls
    - Sync enable/disable
    - Beat grid editor
    - Warp marker adjustment
    - Master tempo selection

### Edit Interface
- **Mouse Controls**
  * Primary Controls:
    - Left-click: Sets IN point (sample-accurate)
    - Right-click: Sets OUT point (sample-accurate)
    - Middle-click: Jumps playhead (minimal latency)
    - Control+click: Sets cue points (max 4, FIFO basis)
  * Modifier Keys:
    - Shift+click: Fine adjustment mode
    - Alt+click: Preview from point
    - Control+Shift: Clear markers
  * Drag Operations:
    - Standard drag: Scrub playhead
    - Shift+drag: Fine scrub
    - Control+drag: Adjust marker positions

- **Touch Controls**
  * Primary Gestures:
    - Single tap: Set IN/OUT (context menu)
    - Double tap: Jump playhead
    - Two-finger tap: Set cue point
    - Pinch: Zoom waveform
  * Advanced Gestures:
    - Three-finger swipe: Navigate markers
    - Two-finger scrub: Fine position control
    - Four-finger tap: Reset view
  * Visual Feedback:
    - Haptic feedback on marker placement
    - Visual confirmation of actions
    - Real-time waveform update
    - Position indicator tracking

- **Time/Pitch Control**
  * Independent Controls
    - Time stretch: -50% to +100%
    - Pitch shift: ±12 semitones
    - BPM sync toggle
    - Pitch lock toggle
  * Format-Specific Edit Controls
    - Stereo Mode
      * Independent channel preview
      * Width adjustment
      * Balance control
    - Mono Mode
      * Pan position editor
      * Expansion preview
      * Format conversion tools
  * Marker Management
    - Interaction Model
      * Click to add/remove
      * Drag to adjust
      * Time Division snap behavior
      * Auto-replace when max reached
    - Marker Types
      * Cue Markers (max 4)
        * Purpose flags
        * Time Division correlation
        * Transition points
      * Warp Markers (max 4)
        * Analysis status
        * Confidence metrics
        * Time Division alignment
  * Format Conversion Interface
    - Conversion Preview
      * Before/After comparison
      * Resource impact display
      * Quality assessment
    - User Controls
      * Conversion approval
      * Priority adjustment
      * Cancel option
    - Status Display
      * Conversion progress
      * Resource availability
      * State preservation

- **Format Controls**
  * Format Display
    - Current format status (read-only)
    - Channel configuration
    - Resource impact indication
  * Format Modification
    - Protected operation
    - Requires explicit user action
    - Confirmation dialog required
    - Clear warning of resource impact
    - Change logged to history

## Hardware Integration

### Audio Interface
- **Device Management**
  * Interface selection
  * Channel routing
  * Clock configuration
  * Buffer settings

- **Status Display**
  * Clock/sync status
  * Buffer health
  * Channel activity
  * Error states

### External Control
- **MIDI Control**
  * MIDI Integration
    - Maps to core control system (see external_control.md)
    - Hardware learn support
    - Visual feedback system

- **GPI/GPO**
  * Hardware Integration
    - Direct control interface (see external_control.md)
    - Emergency stop support
    - Status indication

## User Interface

### Layout System
- **Page Layout**
  * View Configuration
    - Single page (default)
    - Dual page (side-by-side)
  * Per-Page Elements
    - Tab bar (8 tabs max)
    - Active grid view
    - Status indicators

- **Tab System**
  * 8 tabs per page
  * Individual grid per tab
  * Tab state persistence

- **Grid Layout**
  * Configurable matrix
    - Default: 10x12
    - Min size: 4x4
    - Max size: 16x16
  * Dynamic sizing
  * Group indicators

### Control Panels
- **Unified Bottom Panel**
  * Panel Modes (mutually exclusive):
    - Labelling Mode:
      * Clip metadata editing
      * Schema-specific copy/paste
      * Batch property updates
      * Custom field management
    - Editor Mode:
      * Waveform display
      * Transport controls
      * Loop toggle
      * DSP parameter control
      * IN/OUT time entry fields
    - Routing Mode:
      * Matrix grid interface
      * "Outputs" and "Inputs" tabs
      * Device endpoint mapping
      * Group routing configuration
    - Preferences Mode:
      * Application settings
      * Global parameters
      * Persistent controls
      * System configuration
  * Panel Behavior:
    - Single mode visible at a time
    - Persistent size and position
    - Context-aware mode switching
    - State preservation between modes

- **Transport section**
  * Play/Stop
  * Position scrubbing
  * Beat/Bar navigation
  * Emergency stop

- **Level controls**
  * Level control
  * Audition (Import/Edit)
  * Meter display

- **Status displays**
  * Status feedback
  * Error states

- **Configuration access**
  * Interface selection
  * Channel routing
  * Clock configuration
  * Buffer settings

### Visual Feedback
- **State Indication**
  * Time Display
    - Per-clip countdown
      * Source indicator
      * Drift warning
      * Sync status
      * Thread Status
        - Core affinity indicator
        - Priority state display
        - Resource protection status
        - Emergency state indication
    - Global time reference
      * Primary source
      * Source health
      * Sync indicators
  * Playback status
  * Queue status
  * Error states
  * System health

- **Performance Meters**
  * Audio levels
  * CPU usage
  * Memory status
  * Network activity

- **Performance Monitoring**
  * System Status
    - Resource utilization
    - Memory status
    * Thread Status Display
      - Core Assignment
        * Current core ID
        * Lock status indicator
        * Migration warnings
      - Priority Status
        * Current priority level
        * QoS/MMCSS state
        * Power mode indicator
      - Protection Status
        * Memory isolation
        * Cache optimization
        * DPC/ISR routing
  * Emergency Indicators
    - Critical state warnings
    - Recovery status
    - Error notifications

## Platform-Specific Input

### Windows
- **Input Methods**
  * Mouse/keyboard
  * Touch input
  * Pen support
  * Hardware control

- **System Integration**
  * Window management
  * Multi-monitor
  * DPI awareness
  * Power states

### macOS
- **Input Methods**
  * Mouse/keyboard
  * Trackpad gestures
  * Touch Bar
  * Hardware control

- **System Integration**
  * Window management
  * Multi-monitor
  * Retina support
  * Power states

### iOS
- **Input Methods**
  * Touch gestures
  * Apple Pencil
  * Hardware control
  * External keyboard

- **System Integration**
  * Split screen
  * Slide over
  * Background audio
  * Power management

## Performance Requirements

### Response Times
- **Input Handling**
  * Touch/click: <16ms
  * Hardware control: <1ms
  * Animation: 60fps
  * State updates: immediate

### Resource Usage
- **UI Impact**
  * CPU: <10%
  * GPU: optimized
  * Memory: efficient
  * Power: managed

### Interface Configuration
- **Device Management**
  * Interface selection dialog
  * Channel routing matrix
  * Clock source selection
  * Buffer size control

- **Status Monitoring**
  * Clock/sync indicators
  * Buffer health display
  * Channel activity
  * Error reporting

### Edit Panel
- **Time/Pitch Control**
  * Independent sliders
    - Time stretch control - Range: -50% to +100%
    - Pitch shift control - Range: -12 to +12 semitones
    - BPM sync toggle
    - Pitch lock toggle

- **Waveform View**
  * Audition System
    - Uses Audition bus (Import/Edit)
    - Configurable routing
  * Dual-pane display
    - Top: Cue markers
    - Bottom: Warp markers
  * Time Division overlay
    - Frame markers
    - Tempo indicators
    - Sync points
  * Marker Management
    - Click to add
    - Auto-replace when maximum reached
    - Drag to adjust
    - Click to delete

- **Clip Controls**
  * Start/End trim
  * IN/OUT Point Control
    - Fine increment buttons (+/-)
    - Continuous playback
    - Auto-reset to IN point
    - Automatic fades
      * Default: 5ms edit fades
      * Default: 25ms loop fades
      * Zero-crossing preference
    * Fade Controls
      - Independent IN/OUT fades
        * Range: 0s to 3s
        * Step size: 0.1s
        * Replaces automatic fades when set
      - Curve shape selection
      * Curve types:
        - Logarithmic (default)
        - Linear
        - Exponential
        - S-Curve
      * Power compensation
      * Preview waveform display
      - Edit point crossfades
        * Range: 0s to 3s
        * Step size: 0.1s
        * For cue marker transitions
      - Separate from anti-click protection
      - Stored per clip
    - Visual feedback
  * Loop points
  * BPM detection
  * Grid adjustment
  * Fade Interface
    - Fade Time Control
      * 0.1s increment buttons
      * Direct time entry (0-3.0s)
      * Clear button (revert to global)
    - Curve Selection
      * Logarithmic/Linear/Exponential/S-Curve
      * Power compensation toggle

## Media Management

### File Operations
- **Project Media Selection**
  * Initial folder prompt
  * Folder reassignment
  * Status indication
- **Recovery Interface**
  * Missing file notifications
  * Search result display
  * Confirmation dialogs

### Media Management Interface
- **Project Media Dialog**
  * **Initial Setup Dialog**
    - Required on new session creation
    - Components:
      - Current path display
      - Browse button/control
      - Quick access shortcuts
      - Network location support
      - Recent locations list
    - Validation:
      - Path accessibility check
      - Write permission verification
      - Space availability check
      - Network stability test

  * **Change Location Dialog**
    - Accessible via:
      - Main menu
      - Session settings
      - Quick access toolbar
    - Components:
      - Current location display
      - New location selector
      - Migration options:
        * Move files to new location
        * Keep files in place
        * Copy files to new location
      - Validation preview:
        * Space requirements
        * Permission checks
        * Network assessment
    - Status Display:
      - File count/total size
      - Migration estimate
      - Potential issues
      - Required actions

- **File Recovery Interface**
  * Missing File Handling:
    - Clear status indication
    - Search progress display
    - Candidate presentation
    - Batch operation tools
  * Reconciliation Tools:
    - Location comparison
    - Metadata verification
    - Settings preview
    - Confirmation workflow

## Visual Guidelines

### Visual Standards
For detailed visual specifications and component standards, see `visual_reference/visual_guidelines.md`

### Timing Requirements
- **User Interface**
  * Response Time: <16ms (60fps)
    - Button state changes
    - Grid updates
    - Visual feedback
    - Progress indicators

- **Hardware Control**
  * Response Time: <1ms
    - External control surfaces
    - MIDI/OSC input
    - GPI triggers
    - Emergency controls

- **Audio Preview**
  * Response Time: <5ms
    - Audition system
    - Waveform preview
    - Marker auditioning
    - Edit monitoring

### Accessibility Requirements
- **Visual Accessibility**
  * High Contrast Modes
    - Emergency state visibility
    - Resource threshold clarity
    - Format status legibility
    - Thread state distinction
- **WCAG 2.1 AA Compliance**
  * Keyboard Navigation
    - All controls must be keyboard accessible
    - Focus indicators must be visible
    - Tab order must be logical
    - Keyboard shortcuts must be documented
  * ARIA Implementation
    - All interactive elements must have proper roles
    - Live regions for status updates
    - Error announcements
    - State changes must be announced
  * Screen Reader Support
    - All controls must have descriptive labels
    - Complex widgets must have proper ARIA patterns
    - Dynamic content updates must be announced
    - Format status changes must be conveyed
  * Color and Contrast
    - All text must meet AA contrast requirements
    - Color must not be sole means of conveying information
    - Focus states must be clearly visible
    - Emergency states must have multiple indicators

- **Bottom Panel System**
  * Clip Labelling Panel
    - Label & Appearance Tab
      * Button text editing
      * Visual customization
      * Size configuration
    - Metadata Tab
      * Schema-specific editing
      * Copy/paste support
      * Batch operations
  * Application Preferences Panel
    - Persistent playback controls
    - Global settings
    - System configuration
  * Visibility Rules
    - Only one bottom panel visible
    - Main grid view always visible
    - State preservation between switches

### Format-Aware UI States
- **Resource Visualization**
  * Format Indicators
    - Stereo Status
      * Native stereo processing
      * Downmix consideration (80% threshold)
      * Emergency conversion (90% threshold)
    - Mono Status
      * Native mono processing
      * Voice optimization status
      * Pan law indication
  * Resource Pressure Display
    - Warning State (80%)
      * Format-specific indicators
      * Voice reduction preview
      * Conversion candidates
    - Critical State (90%)
      * Emergency protocol phase
      * Format conversion status
      * Voice allocation map

- **Emergency State UI**
  * Phase-Based Feedback
    - Phase 1 (<1ms)
      * Immediate mute indication
      * Format state capture display
      * Voice allocation snapshot
    - Phase 2 (<2ms)
      * Resource protection status
      * Processing chain state
      * DSP status indicators
    - Phase 3 (<5ms)
      * State preservation progress
      * Format metadata status
      * Recovery preparation
    - Phase 4 (<50ms)
      * Recovery sequence progress
      * Format state restoration
      * Voice reallocation status

### Time Division Controls
- **Grid Settings**
  * Time Division Settings
    * Frame Rate Selection
      - Dropdown menu
      * Time division rates
      * Group inheritance status
    - Visual feedback
      * Current rate display
      * Time division alignment status
      * Conflict indicators

  * Musical Tempo
    - BPM controls
    * Time division locked tempo
    * Override controls
    - Visual indicators
      * Tempo confidence
    * Time division correlation
    * Sync status

- **Grid Visualization**
  * Time Division Visualization
    * Frame Markers
      - Major divisions
      - Minor divisions
    * Time division snap points
  * Tempo Overlay
    - Beat markers
    - Bar lines
    - Tempo changes