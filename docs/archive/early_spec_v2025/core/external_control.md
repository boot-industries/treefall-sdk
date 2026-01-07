# External Control Specification

## Cross-References
This specification integrates with:
- Error handling patterns defined in `error_handling.md`
- Performance requirements defined in `performance.md`
- State management rules defined in `state_management.md`

## Network Control
- **Protocol Design**
  * WebSocket primary (remote control)
  * UDP for time-critical
  * TCP for reliability
  * OSC for control surfaces
  * MIDI for hardware

## Core Control Systems

### MIDI Control
- **MIDI Architecture**
  * Multi-port support
  * Virtual ports
  * Network MIDI
  * Bluetooth MIDI

- **Message Handling**
  * Note messages
  * Control changes
  * Program changes
  * System messages
  * For complete MIDI CC assignments, see `appendices/midi_cc_map.md`

- **Parameter Control**
  * Level control (all clips)
  * Pan control (mono clips)
  * Transport control
  * Group assignment
  * Emergency control
  * Fade Control
    - Selected Clip Fades
      * CC 20: IN fade time (0-127 = 0-3s)
      * CC 21: IN fade curve
        - 0-31: Logarithmic
        - 32-63: Linear
        - 64-95: Exponential
        - 96-127: S-Curve
      * CC 22: OUT fade time (0-127 = 0-3s)
      * CC 23: OUT fade curve (same ranges)
      * CC 24: Crossfade time (0-127 = 0-3s)
      * CC 25: Crossfade curve (same ranges)
    - Power Compensation
      * CC 26: IN fade power (0=off, 127=on)
      * CC 27: OUT fade power (0=off, 127=on)
      * CC 28: Crossfade power (0=off, 127=on)
    - Quick Access
      * CC 29: Reset all fades to default
      * CC 30: Copy fade settings
      * CC 31: Paste fade settings

- **Clock System**
  * External BPM input
    - MIDI clock following
    - Beat sync handling
  * Internal BPM output
    - MIDI clock generation
    - Beat sync output

- **System Integration**
  * Interface MIDI routing
  * Port configuration
  * Clock source selection
  * MIDI Learn system
  * Control mapping
  * Feedback system

- **Platform Support**
  * Windows MIDI
  * CoreMIDI (macOS/iOS)
  * Network MIDI
  * Bluetooth MIDI

- **Automation Events**
  * Scheduled triggers
  * Beat-based events
  * State changes
  * Emergency triggers

For complete MIDI implementation details, see `appendices/midi_implementation.md`

### OSC Integration
- **Protocol Design**
  * Address patterns
    - /log/export/{format} - Exports logs in CSV/JSON/XML format
    - /log/sync/{service} - Syncs with Drive/Dropbox/S3
    - /media/folder/* - Project media operations
    - /media/search/* - File recovery operations
  * Message mapping
  * Network discovery

- **Security**
  * Authentication
  * Encryption
  * Access control
  * Session management

- **Clip Control**
  * /clip/{id}/level        # 0.0 to 1.0
  * /clip/{id}/pan         # -1.0 to 1.0 (mono only)
  * /clip/{id}/choke       # boolean
  * /clip/{id}/interrupt   # trigger - force interrupt
  * /clip/{id}/cue/{0-3}
  * /clip/{id}/warp/{0-3}
  * /clip/{id}/time        # -0.5 to 1.0
  * /clip/{id}/pitch       # -12 to 12
  * /clip/{id}/sync        # boolean
  * /clip/{id}/pitchlock   # boolean
  * /clip/{id}/in_point     # Sample position
  * /clip/{id}/out_point    # Sample position
  * /clip/{id}/in_adjust    # +/- increment
  * /clip/{id}/out_adjust   # +/- increment
  * /clip/{id}/fade/in/time      # 0.0 to 3.0 seconds
  * /clip/{id}/fade/in/curve     # 0=Log, 1=Lin, 2=Exp, 3=S
  * /clip/{id}/fade/in/power     # boolean
  * /clip/{id}/fade/out/time     # 0.0 to 3.0 seconds
  * /clip/{id}/fade/out/curve    # 0=Log, 1=Lin, 2=Exp, 3=S
  * /clip/{id}/fade/out/power    # boolean
  * /clip/{id}/crossfade/time    # 0.0 to 3.0 seconds
  * /clip/{id}/crossfade/curve   # 0=Log, 1=Lin, 2=Exp, 3=S
  * /clip/{id}/crossfade/power   # boolean
  * /clip/{id}/markers/cue/{0-3}
  * /clip/{id}/markers/warp/{0-3}
  * /clip/{id}/marker_sequence  # Int: order

- **Logging Control**
  * /log/playback/start      # Timestamp, clip info
  * /log/playback/stop       # Timestamp, duration
  * /log/export/start        # Format selection
  * /log/export/status       # Progress
  * /log/export/complete     # Success/fail
  * /log/sync/{service}/status  # Sync status

- **Media Management**
  * /media/folder/select     # Set project folder
  * /media/folder/status     # Get current path
  * /media/search/start      # Begin file search
  * /media/search/results    # List candidates
  * /media/search/select     # Choose replacement

### GPI/GPO Control
- **Hardware Integration**
  * Contact closure input
  * Relay output control
  * Opto-isolator support
  * Voltage level sensing

- **Timing System**
  * BPM pulse output
  * Global tempo sync
  * Frame sync
  * Beat clock division

- **Emergency Control**
  * Critical stop handling
  * Failsafe states
  * Override priority
  * State recovery
  * Emergency Commands
    - /emergency/stop      # Immediate system halt
    - /emergency/status    # Current emergency state
    - /emergency/recover   # Initiate recovery
    - /emergency/reset     # Clear emergency state
  * State Communication
    - /emergency/resource/*  # Resource state namespace
      * /allocation  # Resource allocation state

## Remote Applications
- **State Synchronization**
  * Real-time sync
  * Device discovery
  * Connection management
  * Error recovery
  * Resource Control
    - /remote/resource/*
      * /allocate    # Request resources

- **Platform-Specific Control**
  * Desktop Protocols
    - Full OSC implementation
    - Complete MIDI mapping
    - All emergency commands
  * Mobile Protocols
    - Limited OSC subset
    - Basic MIDI control
    - Essential emergency commands
  * Cross-Platform Communication
    - Protocol adaptation
    - Command translation
    - State synchronization

- **iOS App Modes**
  * Standalone Soundboard
    - Independent operation
    - Local audio processing
    - Format-native playback
  * Remote Controller
    - Desktop app control
    - Live monitoring
    - Waveform previews
    - Countdown timers
  * Platform Support
    - iPhone optimization
    - iPad optimization
    - iPadOS multitasking

## Performance Requirements

### Timing
- **Response Times**
  * Network response: <50ms
  * Hardware sync: <1ms
  * State updates: <16ms
  * Emergency commands: <1ms

### Resource Usage
- **Optimization**
  * Network bandwidth
  * CPU efficiency
  * Memory management
  * Power awareness

## Security Requirements
- **Authentication**
  * Device pairing
  * User validation
  * Session tokens
  * Access control

- **Encryption**
  * TLS 1.3
  * Certificate validation
  * Key management
  * Secure storage

## Tab Control
- **Tab Management**
  * /page/{1-2}/tab/{0-7}  # Int: select tab
  * /page/{1-2}/grid/*     # Grid commands for specific page

## Clip Control
- **Clip Management**
  * /clip/{id}/fifo        # Int: position
  * /clip/{id}/marker_sequence  # Int: order

## Bus Control
- **Bus Management**
  * /bus/clipgroup/{a-d}/*
  * /bus/audition/* - Import/Edit bus operations
  * /bus/record/{a-b}/*
  * /bus/ltc/*
  * /bus/monitor/*

## Control Systems

### MIDI Implementation
- **Control Types**
  * Direct Control
    - Note triggers
    - CC parameters
    - Program changes
    - System messages
  * Feedback
    - Button states
    - Level meters
    - Position data
    - Error states

### OSC Integration
- **Message Structure**
  * Core Commands
    - Transport control
    - Grid operations
    - Parameter adjustment
    - State queries
  * Feedback Messages
    - Real-time updates
    - State changes
    - Error notifications
    - Performance data 

## Error Handling
Error handling for external control follows patterns defined in `error_handling.md`, including:
- Connection failure recovery
- Protocol error handling
- Timeout management
- State synchronization recovery

Error handling for external control follows patterns defined in `error_handling.md`, including:
- Connection failure recovery
- Protocol error handling
- Timeout management
- State synchronization recovery

### Documentation Control
- **Cue Sheet Commands**
  * /cuereport/generate - Generate new cue sheet
  * /cuereport/template - Select template
  * /cuereport/validate - Validate current data
  * /cuereport/export - Export in selected format
  * /cuereport/annotate - Add manual notes 

## Performance Requirements
- **Response Times**
  * Control Latency
    - Hardware Control: <1ms
      * Sub-millisecond response essential
      * Enables precise synchronization
      * Critical for broadcast environments
    - UI Feedback: <16ms
      * Based on screen refresh rates
      * Maintains perceived immediacy
      * Crucial for professional operation
    - Audio Response: <5ms
      * Professional-grade performance
      * Live monitoring requirement
      * Multi-output sync maintenance 

### Emergency Control Integration
- **UI State Management**
  * Emergency States
    - Visual feedback requirements
      * Resource pressure display

### Format State Monitoring
- **Read-Only Access**
  * Per-Clip Fade Status
    - Current fade times
    - Active curve types
    - Power comp state
  * State Change Notification
    - Format updates (read-only)
    - Resource impact changes
    - Processing chain status 

### Time Source Monitoring
- **Status Queries Only**
  * Read-only source status
  * Current sync quality metrics
  * Drift/jitter measurements 