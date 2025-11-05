# Bus Architecture Specification

## Core Bus Systems

### Fixed Bus Architecture
- **Output Configuration**
  * Clip Group Busses (A-D)
    - Independent stereo processing
    - Format-native voice allocation
      * Stereo clips = stereo voice
      * Mono clips = mono voice with pan
    - Mono-to-stereo expansion with pan
    - Optional mono output with downmix
      * Only at group output stage
      * Preserves internal stereo processing
  * Processing Chain
    - Format-Specific Routing
      * Stereo path: Direct stereo processing
      * Mono path: Expansion with pan control
    - Resource Monitoring
      * Per-format voice tracking
      * Load balancing
      * Auto-scaling triggers
    - Format Conversion Chain
      - Pre-Conversion
        * Resource validation
        * State preservation
        * Buffer preparation
      - Conversion Process
        * Sample-accurate switching
        * Pan position preservation
        * Width state handling
      - Post-Conversion
        * Resource reallocation
        * State restoration
        * Buffer cleanup
  * Audition Bus
    - Dedicated stereo processing
    - Zero-impact preview system
    - Independent routing
    - Format-native processing
  * Input Busses
    - Record A/B (stereo)
    - LTC (mono)
    - Format-appropriate processing

### Main Output Busses
- **Group Busses (A-D)**
  * Independent processing
  * Dynamic channel routing
    - Stereo internal processing
    - Mono downmix at output stage only
    - Per-group output format selection
  * Time Division Settings
    - Per-group sync mode selection:
      * Blackburst sync (75fps)
      * 24fps (41.67ms)
      * 29.97fps drop-frame (33.37ms)
      * 29.97fps non-drop (33.37ms)
      * 30fps (33.33ms)
      * 60fps (16.67ms)
      * Default: 120 BPM (12.5ms/80fps)
    - Independent timing per group
    - Affects all clips in group
  * State preservation
  * Level control

- **Fixed Bus Allocation**
  * Output Busses (5 stereo)
    - Clip Groups A-D
    - Audition (Import/Edit)
      * Freely routable to outputs
  * Input Busses
    - Record A (stereo)
    - Record B (stereo)
    - LTC (mono)

### Monitor System
- **Audition System**
  * Zero-impact design
  * Independent routing
  * Auto-muting logic
  * State isolation

- **Monitor Output**
  * Dedicated processing
  * Level control
  * Format conversion
  * Metering system

## Routing Architecture

### Matrix System
- **Input Routing**
  * Source selection
  * Format handling
    - Mono sources: automatic stereo expansion with pan
    - Stereo sources: native stereo processing
    - LTC input: dedicated mono processing
  * Channel mapping
  * Level control

- **Output Assignment**
  * Group routing
  * Monitor routing
  * Format adaptation
  * Channel mapping

### Control Integration
- **External Control**
  * MIDI routing control
  * OSC bus control
  * GPI/GPO triggers
  * Remote app control

- **Automation**
  * Bus state automation
  * Level automation
  * Route changes
  * Mute control

## Timing System

### Sample Rate
- **Format Handling**
  * Sample rate detection
  * Rate conversion when needed
  * Format adaptation
  * Error handling

### Tempo System
- **BPM Management**
  * Internal tempo engine
  * External MIDI clock handling
  * Beat-accurate timing
  * Trigger synchronization

## Performance Requirements

### Latency
- **Core Timing**
  * Critical Operations
    - Bus muting: <1ms
    - Voice deallocation: <1ms
    - Format switching: <2ms
    - Route changes: <2ms
  * Background Operations
    - State preservation: <10ms
    - System recovery: <100ms
    - Resource rebalancing: background
  * Format-Native Processing
    - Zero conversion in critical path
    - Background format preparation
    - Resource-aware scaling

### Resource Usage
- **System Impact**
  * Per bus: <2% CPU
  * Memory: optimized
  * Cache: efficient
  * Power: managed

## Error Handling

### Recovery Systems
- **Bus Recovery**
  * Route reconfiguration
  * State preservation
  * Buffer management
  * Error reporting

- **Failover**
  * Emergency routes
  * Backup paths
  * State recovery
  * Error notification

### Emergency Handling
- **Bus Emergency Protocol**
  * Immediate Actions
    - All bus muting
    - Voice deallocation
    - State snapshot
  * Protocol Timing
    - Bus muting: <1ms
    - Voice deallocation: <1ms
    - State capture: <2ms
    - Recovery init: <5ms
  * Recovery Sequence
    - Bus reinitialization
    - Voice reallocation
    - State restoration
  * Format-Specific Recovery
    - Stereo bus recovery
    - Mono bus recovery
    - Resource redistribution

## Platform Integration

### Windows
- **Driver Integration**
  * ASIO routing
  * WASAPI paths
  * Driver recovery
  * State handling

### macOS
- **Core Integration**
  * CoreAudio routing
  * AU Graph paths
  * Device handling
  * State management

### iOS
- **Engine Integration**
  * AVAudioEngine routing
  * Session handling
  * Interruption management
  * State preservation

### Matrix System
- **Routing Configuration**
  * Clip Group Routing
    - Group assignments (A-D)
    - Direct output routing
    - Level control
  * Monitor routing

### System Buses
- **Timing Bus**
  * Independent timing thread
  * Zero impact on audio chain
  * Source priority handling
  * Cross-bus synchronization
  * Emergency protocol integration
  - Main Mix Bus
  - Group Buses (A-D)
  - Audition Bus

### Bus Architecture Overview
  * Output Configuration
    - Groups A-D (Main Output)
      * Voice Allocation:
        * 48 mono voices as 24 stereo voices by default, assignable as mono
        * +1 dedicated stereo voice for Audition Bus
        * 3 voices per group in choke mode
        * Format-native processing maintained
      * Bus Features:
        - Independent level control
        - Format-native processing
        - Emergency protocol protection
    
    - Audition Bus (Preview)
      * Single dedicated stereo voice
      * Independent from main 24-voice pool
      * Zero-impact preview system
      * Format-native processing
      * Auto-yields under load

    - Input Configuration
      * Record A/B (stereo)
        - Format-native processing
        - Degradable under load
      * LTC (mono)
        - Dedicated sync input
        - Format-specific processing

### Bus Timing Requirements
- **Thread Management**
  * Critical Bus Processing
    - Core Assignment
      * Dedicated core allocation
      * Lock verification
      * Master Clock Integration
        - Global timing reference
        - Sample-accurate playback
        - Drift compensation
      * Per-Group Time Division
        - Independent frame rate/tempo
        - Display grid configuration
        - Visual feedback handling
    - Priority Settings
      * Platform-specific config
        - Windows: MMCSS audio
        - macOS: Time constraint
        - iOS: Audio session
    - Resource Protection
      * Memory isolation
      * Cache optimization
      * DPC/ISR management

- **Latency Requirements**
  * Output Chain
    - Group A-D: <1ms
    - Audition: <2ms
    - Monitor: <5ms
  * Input Chain
    - Record A/B: <5ms
    - LTC input: <1ms