# System Architecture Specification

## Implementation Guidelines

### Code Organization
- **Core Systems**
  * Audio Thread Requirements:
    - Isolated with <5ms end-to-end response
    - Emergency protocol support (<1ms to <100ms phases)
    - Format-native processing chain
  * Lock-free data structures
  * Zero-allocation audio path
  * Thread-safe state management
  * Voice Management
    - Fixed voice pools:
      * Main pool: 24 stereo voices
      * Preview: 1 stereo voice
      * Efficient mono pairing
    - Format-native processing

### Error Handling Patterns
- **Audio Chain**
  * Emergency Protocol Implementation:
    - Phase 1 (<1ms): Immediate response
    - Phase 2 (<2ms): Protection measures
    - Phase 3 (<10ms): State preservation
    - Phase 4 (<100ms): Recovery procedures
  * Immediate muting on critical errors
  * Automatic fade on interruption
  * Buffer underrun recovery
  * State preservation

- **Resource Errors**
  * Graceful degradation
  * Dynamic voice reduction
  * Memory pressure handling
  * Cache eviction strategy

### Performance Optimization
- **Audio Processing**
  * SIMD optimization where available
  * Cache-line alignment
  * Memory pooling
  * Zero-copy operations

- **Resource Management**
  * Thread affinity
  * Priority inheritance
  * Power state awareness
  * Background task scheduling

### Testing Requirements
- **Unit Testing**
  * Core algorithm validation
  * State management verification
  * Error handling coverage
  * Performance benchmarks

- **Integration Testing**
  * End-to-end audio chain
  * External control validation
  * Resource limit testing
  * Platform-specific tests

## Version Control

### Documentation and APIs
- **Version Format: v{major}.{minor}.{patch}**
  * Major: Breaking changes to architecture/APIs
  * Minor: Feature additions/modifications
  * Patch: Corrections/clarifications
  * Required tracking:
    - Version number
    - Last updated date
    - Change summary
    - Related tickets

### File Formats
- **Audio Projects**
  * Format: ORP{year}{version}
  * Example: ORP2024A
  * Header-stored version
  * Backward compatibility required

- **Sessions**
  * Database schema version
  * Feature support version
  * Automatic migration
  * Compatibility layers

### External Protocols
- **MIDI/OSC Versioning**
  * SysEx version reporting
  * Capability negotiation
  * Version-specific endpoints
  * Migration support

### Schema Management
- **Database Evolution**
  * Schema versioning
  * Migration scripts
  * Data validation
  * Rollback support
  * Upgrade process:
    - Step-by-step migration
    - Data preservation
    - Validation checks
    - Recovery points

## Core Components

### Resource Management
All resource management specifications are defined in `performance.md`, including:
- Memory and thread management
- CPU utilization targets
- Power state handling
- Cache optimization strategies

### Media System
- **Project Management**
  * Media folder handling
    - Central repository
    - Path management
    - File tracking
  * Recovery system
    - Intelligent search
    - File reconciliation
    - State preservation

### Logging System
- **Real-time Logging**
  * Playback tracking
    - Clip timestamps
    - Duration logging
    - Metadata capture
  * Export system
    - Multiple formats
    - Cloud integration
    - Automated export

### Audio System
- **Voice Management**
  * Format-Native Allocation
    - Voice Types
      * Stereo voice
      * Mono voice
    - Bus Integration
      * Group routing (A-D)
      * Audition bus allocation
      * Emergency bus management
    - Pan Law Implementation
      * -4.5dB center compensation
      * Format-aware pan curves

  * Resource Integration
    - Fixed Bus Architecture
      - 5 stereo output busses
        * Groups A-D: Format-native processing
        * Audition: Zero-impact preview
      - Format-State Preservation
        * Processing chain integrity
        * Voice allocation stability
        * Resource pressure handling

- **Interface Management**
  * Device configuration
  * Channel routing
  * Clock/sync handling
  * Buffer management

### CLIP GRID System
- **Page Management**
  * SINGLE PAGE/DUAL PAGE modes
  * Per-TAB state tracking
  * Layout persistence

- **TAB Management**
  * 8 TABS per session
  * Independent CLIP GRID states
  * TAB switching logic

- **Layout Engine**
  * CLIP GRID Configuration
    - User-defined dimensions
    - Runtime size validation
  * Dynamic resizing

- **Playback Management**
  * Marker sequence management
  * Clip Group assignment (A-D)
  * Clip interruption handling
    - Choke-enabled clip detection
    - Group-based interrupt routing
    - Fade coordination
  * Audition system
  * State tracking

### Bus Architecture
- **Fixed Bus Allocation**
  * Output Busses (5 stereo)
    - Clip Groups A-D
    - Audition (Import/Edit)
    - Format-native processing per bus
  * Input Busses
    - Record A/B (stereo)
    - LTC (mono)

### Control Systems
- **External Control**
  * MIDI integration
  * OSC protocol support
  * GPI/GPO interface
  * Remote app control

- **Automation System**
  * Parameter automation
  * Scheduled events
  * State recall
  * Trigger mapping

## Platform Integration

### Core Features
- **Windows**
  * ASIO/WASAPI
  * Format Management
    * Native stereo pairs
    * Mono channel routing
    * Format-specific buffering
  * DirectX
  * Threading model
  * Resource management

- **macOS**
  * CoreAudio
  * Format Management
    * Native AU routing
    * Format detection
    * Buffer optimization
  * Metal
  * GCD integration
  * Resource handling

- **iOS**
  * AVAudioEngine
  * Background audio
  * Power management
  * State preservation

## Performance Architecture

### Real-time Systems
- **Audio Chain**
  * <5ms response time
  * Sample-accurate sync
  * Format-native processing
  * Error resilience

- **User Interface**
  * 60fps rendering
  * <16ms response
  * State feedback
  * Error indication

### Resource Optimization
- **CPU Management**
  * Thread priority
  * Core assignment
  * Power states
  * Thermal management
  * Resource Thresholds
    - Warning: 80% app ceiling (72% system)
    - Critical: 90% app ceiling (81% system)
    - Recovery: 70% app ceiling (63% system)

- **Memory Management**
  * Buffer optimization
  * Cache efficiency
  * Resource pooling
  * Leak prevention

## Error Handling
See error_handling.md for comprehensive error management:
- Recovery systems
- Error reporting
- Prevention strategies
- Logging system

## Security Architecture
- Process isolation model
- Resource protection
- State preservation
- Access control framework

## Platform Integration
- Native API abstraction
- Driver management
- Resource handling
- State synchronization

## Timing Systems

### Core Timing
- **Sample Rate**
  * Minimum 44.1kHz support
  * Automatic rate detection
  * Format conversion when needed
  * Interface rate adaptation

- **Tempo System**
  * Internal BPM engine
    - Stable tempo generation
    - Schedule management
    - Trigger timing
  * External BPM support
    - MIDI clock following
    - Beat sync
    - Tempo change handling

### State Management
- **Automation State**
  * Parameter tracking
  * Event scheduling
  * State preservation
  * Recovery points

- **Resource Management**
  * Voice Allocation
    - Format-native processing
      * Stereo clips = stereo voice
      * Mono clips = mono voice
    - Dynamic scaling based on:
      * Available DSP resources
      * System capability
      * Memory constraints
    - Group-based limits:
      * Choke mode: 2-4 voices max
      * Free mode: DSP-limited
    - Priority system:
      * Primary playback (highest)
      * Preview/audition (secondary)
  * Performance Monitoring
    - Real-time CPU tracking
    - Memory usage per voice
    - Auto-scaling thresholds
    - Resource distribution

### Resource Management
- **CPU/Thread Management**
  * See resource_management.md for details

- **Memory Optimization**
  * See resource_management.md for details

- **Power State Handling**
  * See resource_management.md for details

- **Performance Monitoring**
  * See resource_management.md for details

For complete platform-specific implementation details, see `platform/implementation.md`

### Core Components
- **State Management**
  * Preservation Chain
    - Real-time State
      * Voice allocation map
      * Resource utilization
      * Bus configuration
    - Emergency State
      * Complete snapshot
      * Resource state
      * Recovery points
    - Recovery State
      * Pre-emergency data
      * Resource distribution
      * Format configuration

### System Integration Map
- **Emergency Protocols**
  * Initiation: error_handling.md
  * Resource Management: resource_management.md
  * State Preservation: state_management.md
  * User Interface: interaction_model.md

- **Format Management**
  * Definition: clip_metadata.md
  * Processing: clip_system.md
  * Resources: resource_management.md
  * Interface: interaction_model.md

### Platform Integration
  * iOS
    - Background audio
  * **Background Behavior Matrix**
    * Windows/macOS
      - Full functionality maintained
      - Resource thresholds unchanged
      - Format processing preserved
    * iOS
      - Reduced functionality mode
      - Aggressive resource conservation
      - Format optimization enforced
      - Battery-aware processing 

### Logging Architecture
- **Three-Tier Logging System**
  * Performance Rights Tier:
    - Real-time playback capture
    - Rights management data
    - PRO reporting system
    - Export management
  * Technical Tier:
    - System diagnostics
    - Resource utilization
    - Error conditions
    - Performance metrics
  * Media Management Tier:
    - File operations
    - Recovery actions
    - Reconciliation events
    - Storage status

- **Storage Strategy**
  * Primary Storage:
    - SQLite for performance logs
    - JSON for technical logs
    - Binary for system logs
  * Backup System:
    - Local redundancy
    - Cloud sync integration
    - Export scheduling
    - Archive management 

### Audio System
- **Format Support**
  * Primary Formats
    - WAV, AIFF, FLAC (native JUCE)
    - MP3 (JUCE's built-in decoder)
    - OGG (JUCE's built-in decoder)
  * Platform-Native Codecs
    - AAC/M4A via CoreAudio (macOS/iOS)
    - WMA via MediaFoundation (Windows) 

### Control Integration Architecture
- **Format-Aware Control Layer**
  * Hardware Integration
    - Format-aware routing
    - Resource-aware assignment
    - Emergency protocol triggers
  * Protocol Management
    - MIDI format state control
    - OSC resource endpoints
    - Hardware state mapping 

For complete implementation requirements, see:
`rules/core_requirements.rules`

## Multi-Core Audio Processing Architecture

### Thread Architecture
- **Real-time Audio Thread** (Highest Priority)
  * Core affinity: Dedicated core
  * Priority: Real-time
  * Zero interruption guarantee
  * Responsibilities:
    - Active playback streams
    - Basic fade processing (critical priority)
    - Smart fade suggestions (background only)
    - Upcoming trigger preparation
    - Voice pre-allocation
    - Emergency protocol handling
  * Pre-emptive resource reservation
    - Fade out completion paths
    - Upcoming trigger buffers
    - Format-native voice pools
    - Emergency headroom

- **Processing Thread Pool**
  * Core affinity: Remaining cores minus 1
  * Priority: High
  * Thread count: (CPU cores - 2)
  * High-Density Session Management:
    - Intelligent Memory Stratification
      * Hot cache: Currently playing + next likely triggers
      - Response: Guaranteed <1ms
      - Contents: 
        * Currently playing
        * Next predicted triggers
        * Active group clips
        * Emergency protocol buffer

      * Warm cache: Recently played + adjacent clips
        - Response: <5ms (may require brief resource reallocation)
        - Contents:
          * Recently played clips
          * Spatially adjacent clips
          * Group-related content
          * Format-matched voices

      * Cold storage: Remaining session clips
        - System Responsibilities:
          * Continuous background optimization
          * Intelligent cache promotion
          * Transparent resource management
          * Zero operator intervention required
        - Performance Requirements:
          * All clips must play on demand
          * No audible artifacts
          * No UI indication of cache state
          * Seamless format handling
        - Implementation Strategy:
          * Aggressive preloading
          * Predictive cache management
          * Dynamic resource allocation
          * Automatic format optimization

    - Dynamic Voice Management
      * Pre-rendered fade curves
      * Shared voice pools
      * Format-aware voice recycling
      * Zero-latency voice acquisition
    - Predictive Resource Allocation
      * Pattern-based prediction
      * Usage frequency analysis
      * Spatial proximity loading
      * Group-based preloading

  * Responsibilities:
    - Predictive loading
      * Next likely triggers
      * Fade curve preparation
      * Format conversion prep
      * Voice pre-warming
    - Background processing
      * Time/pitch processing
      * Format conversion
      * Waveform analysis
      * Cache preparation
  * Resource reservation
    - Guaranteed fade completion
    - Protected voice allocation
    - Format-native pathways
    - Click-free transitions

### Processing Guarantees
- **Click-Free Operation**
  * Zero-interruption policy
    - Active playback protection
    - Fade completion guarantee
    - Trigger preparation paths
    - Format transition smoothing
  * Resource pre-allocation
    - Fade processing headroom
    - Voice reservation system
    - Format-native buffers
    - Emergency capacity

- **Background Management**
  * Transparent resource handling
    - Dynamic voice allocation
    - Automatic format optimization
    - Cache pre-warming
    - Memory defragmentation
  * Operator focus protection
    - Zero UI interaction required
    - Automatic resource balancing
    - Predictive load management
    - Silent optimization

### Processing Optimization
- **Lock-Free Architecture**
  * Wait-free ring buffers
  * Atomic operations
  * Memory barriers
  * Cache line alignment

- **SIMD Acceleration**
  * AVX/SSE optimization for:
    - Fade curve processing
    - Format conversion
    - Voice processing
    - Mix operations
  * Format-aware vectorization
  * Parallel voice processing
  * Cache-coherent operations 

### Platform-Specific Optimizations

- **Windows**
  * Thread Management
    - WASAPI/ASIO thread isolation
    - Core parking prevention
    - Timer resolution optimization
    - DirectX hardware acceleration
  * Multi-Core Optimization
    - Processor groups handling (>64 cores)
    - NUMA awareness
    - Thread-to-core affinity maps
    - Power scheme optimization

- **macOS**
  * Thread Management
    - Grand Central Dispatch integration
    - XPC Services for background tasks
    - Timer coalescing awareness
    - Metal acceleration
  * Multi-Core Optimization
    - QoS class assignment
    - Workgroup management
    - Power efficiency cores handling
    - Neural Engine offloading where applicable

- **iOS**
  * Thread Management
    - Background task handling
    - Audio session management
    - Power efficiency awareness
    - Metal acceleration
  * Multi-Core Optimization
    - Performance/Efficiency core balance
    - Battery impact management
    - Thermal throttling adaptation
    - Background processing limits
  * Resource Constraints
    - Dynamic thread pool sizing
    - Aggressive voice management
    - Power-aware processing
    - Background mode optimization 

### Large Session Optimization
- **Memory Management**
  * Tiered Storage Strategy
    - L1: Active playback (RAM)
      * Currently playing clips
      * Imminent triggers
      * Critical fade data
      * Emergency protocol buffer
    - L2: Quick access (RAM)
      * Recent/likely triggers
      * Pre-computed fade curves
      * Format-native voices
      * Group-related clips
    - L3: Ready state (Fast Storage)
      * Memory-mapped files
      * Streaming preparation
      * Format-preserved state
      * Quick-load headers

  * Resource Reservation
    - Guaranteed voice availability
    - Pre-allocated fade processors
    - Format-native pathways
    - Emergency headroom maintained

- **Streaming Architecture**
  * Zero-latency triggering
    - Header pre-loading
    - Fade curve preparation
    - Voice pre-allocation
    - Format-native paths
  * Background preparation
    - Predictive loading
    - Pattern analysis
    - Resource balancing
    - Cache optimization 

- **Mixed-Format Management**
  * Format-Native Processing Chains
    - Parallel processing paths
      * 44.1kHz chain (dedicated)
      * 48kHz chain (dedicated)
      * 32-bit float chain
      * 16-bit chain
    - Zero-conversion playback
      * Format-matched voice allocation
      * Native sample rate processing
      * Bit depth preservation
      * Independent buffer management

  * Mixed-Format Optimization
    - Format-specific voice pools
      * Pre-allocated per format type
      * Dedicated memory regions
      * Format-aware cache strategy
      * Zero-conversion paths
    - Parallel summing buses
      * Independent rate processing
      * Final-stage conversion only
      * Dither when needed (16-bit output)
      * Format-aware resource allocation

  * Format-Aware Resource Management
    - Per-format voice reservation
      * 44.1kHz pool
      * 48kHz pool
      * Float32 voices
      * Int16 voices
    - Dynamic pool sizing based on session content
    - Format-specific emergency headroom
    - Zero-latency format switching 

### Emergency Protocol Integration
- **Cache Miss Handling**
  * Hot Cache Miss
    - Trigger emergency protocol Phase 1
    - Immediate voice reallocation
    - Format-native path switching
    - Guaranteed recovery <1ms

  * Warm Cache Miss
    - Background promotion to hot cache
    - Resource reallocation if needed
    - Format-specific voice preparation
    - Recovery within 5ms window

  * Cold Cache Miss
    - User notification if repeated
    - Aggressive cache promotion
    - Resource rebalancing
    - Performance impact logging 

### Operator Guidelines
- **Session Preparation**
  * Pre-load critical clips to hot cache
  * Identify high-priority content
  * Use group-based organization
  * Consider format consolidation 

## Core Systems

### Audio Engine
- Format-native processing
- Resource management
- Emergency protocols

### Timing System
- **Core Architecture**
  * Independent timing thread
  * CPU Resource Management
    - Dedicated Core Assignment
      * Locked affinity
      * Uninterruptible state
      * Real-time priority
      * Continuous monitoring
    - Resource Protection
      * Memory isolation
      * Cache optimization
      * DMA configuration
      * Interrupt steering
    - Zero impact on audio chain
    - Protected memory allocation
    - Emergency protocol protected
  * Time Source Management
    - Primary Sources
      * LTC: Direct audio input
      * MTC: Hardware/Network/Virtual MIDI
        - Equal priority handling
        - Individual monitoring
        - Quality validation per source
    - Secondary: PTP network time
    - Tertiary: System clock
    - Optional: Blackburst/Tri-level sync
    - Source health monitoring
      * Quality metrics
      * Drift detection
      * Auto-switching
  * Display Integration
    - Per-clip counters
      * Sample-accurate tracking
      * Independent source selection
      * Drift compensation
    - Global reference
      * Primary source display
      * Health indicators
      * Emergency status 

### Core Systems Architecture

#### Timing Architecture
- **Master Clock System**
  * Zero-Copy Distribution
    - Lock-free implementation
    - Thread-safe design
    - Emergency protocol protection
  * Source Management
    - Primary Sources
      * LTC: Direct audio input
      * MTC: Hardware/Network/Virtual MIDI
        - Equal priority handling
        - Individual monitoring
        - Quality validation per source
    - Secondary: PTP network time
    - Tertiary: System clock
    - Optional: Blackburst/Tri-level sync

- **Time Division System**
  * Frame Rate Options
    - Film: 24fps (41.67ms)
    - NTSC-DF: 29.97fps drop (33.37ms)
    - NTSC: 29.97fps non-drop (33.37ms)
    - PAL: 30fps (33.33ms)
    - HD: 60fps (16.67ms)
    - Blackburst: 75fps (13.33ms)
  * Tempo Management
    1. Global Tempo Override
       * System-wide tempo lock
       * All clips warp to match
    2. Per-Clip Settings
       * Auto-detected on import
       * Manual user override
       * Default: 120 BPM
    3. Clip Group Override
       * Optional group-level control
       * Affects all member clips 

### Clip Architecture
- **Marker System**
  * Marker Management
    - Cue Markers
      * Position tracking
      * Up to 4 per clip
      * Position anywhere in clip
        - Independent of In/Out points
        - Enables alternate entry points
        - Enables alternate endings
        - Functions in both loop/non-loop modes
      * Time Division snap
    - Warp Markers
      * Tempo alignment
      * Beat detection
      * Time Division sync
    - Fade Markers
      * In/Out points
        - Define clip boundaries
        - Loop region when enabled
      * Custom regions
      * Curve types
      * Time Division snap 