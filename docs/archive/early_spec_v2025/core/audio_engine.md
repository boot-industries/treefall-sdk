# Audio Engine Specification

## Transport System

### Core Transport
- **Playback Control**
  * Play/Stop/Pause
  * Position control
  * Speed control
  * Emergency stop

- **Clock Management**
  * Master Clock Integration
    - Direct subscription to Master Clock Manager
    - Zero-copy time reference
    - Lock-free implementation
    - Format-aware timing
  * Sample clock
  * MIDI clock
  * External sync
  * Clock drift compensation

### Sync Sources
- **Clock Sources**
  * Master Clock Manager (Primary)
    - Single source of truth
    - Thread-safe distribution
    - Emergency protocol protected
  * Internal clock
  * Interface clock
  * External word clock
  * Video reference

- **Position Reference**
  * Time Source Priority Chain
    - LTC/MTC (Primary)
    - PTP Network Time (Secondary)
    - System Clock (Fallback)
    - Blackburst/Tri-level (Optional)
  * Sample position
  * SMPTE timecode
  * MIDI timecode
  * Beat clock

## Core Architecture

### Driver Integration
- **Interface Management**
  * Device enumeration and aggregation
  * Dynamic channel configuration
  * Automatic format adaptation
    - Mono/stereo format handling
    - Automatic mono summing when needed
    - Stereo-to-mono downmix options
    - Sample rate conversion
  * Driver model abstraction
    - ASIO/WASAPI (Windows)
    - CoreAudio (macOS)
    - AVAudioEngine (iOS)

- **Clock Configuration**
  * Interface clock detection
  * Clock source selection
  * Sample rate options
  * Buffer size settings

### Processing Chain
- **Transport Processing**
  * Sample-accurate playback
  * Zero-latency switching
  * Format-Specific Time/Pitch
    - Stereo Processing
      * Independent channel analysis
      * Phase correlation preservation
      * Width maintenance
    - Mono Processing
      * Single channel optimization
      * Pan position preservation
      * Expansion state handling
    - Resource Management
      * Format-aware CPU scaling
      * Memory optimization
      * Quality adaptation
  * Automatic Anti-Click Protection
  * Automatic Fades
    - System-level protection (always active unless overridden)
      * Smart Fade Integration
        - Respects global smart fade preference
        - Falls back to basic fades on ANY issue
        - Zero impact on playback guarantee
      * Clip edges: 1ms fixed
        - Slightly convex curve shape
          * Quick entry (first 30%)
          * Gradual exit (last 70%)
          * Non-linear power curve
      * Edit points: 0.25ms to 10ms (adaptive)
        - Adaptive analysis:
          * Transient detection
          * Beat position analysis
          * Spectral content matching
          * Zero-crossing optimization
        - Fade time selection:
          * Shorter (0.25ms) for matched beats
          * Longer (up to 10ms) for spectral mismatch
          * Medium (~5ms) for ambiguous content
        - Convex curve optimization
          * Faster attack for transients
          * Smoother release for tails
      * Loop points: 0.25ms to 10ms (adaptive)
        - Loop analysis:
          * Beat grid alignment
          * Spectral continuity check
          * Phase correlation
          * Zero-crossing optimization
        - Fade time selection:
          * Minimal (0.25ms) for beat-matched loops
          * Longer for spectral discontinuities
          * Balanced for phase alignment
        - Crossfade curve matching
          * Entry/exit curves complement
          * Power-compensated overlap
  * Position tracking
  * Clock stability
  * Time-stretching engine
    - Independent time/pitch control
    - Time range: -50% to +100%
    - Pitch range: ±12 semitones
    - BPM sync with pitch lock
    - Real-time warping modes
      * Free (time + pitch)
      * Locked pitch (time only)
      * BPM sync (tempo-matched)
    - Quality preservation
    - Formant correction

- **Input Stage**
  * Format detection
    - Stereo: native processing
    - Mono: stereo expansion with pan
    - LTC: dedicated mono path
  * Sample rate conversion
  * DC offset removal
  * Buffer allocation

- **DSP Chain**
  * Group processing
  * Level control
  * Pan/balance
  * Filter implementation
  * Real-time parameter updates

- **Output Stage**
  * Format conversion
    - Stereo-to-mono downmix (at group output only)
    * Per-group output format selection
  * Dither (when needed)
  * Metering integration
  * Driver handoff

- **Parameter Automation**
  * Level automation
  * Pan automation
  * Time/pitch control
  * Real-time modes:
    - Write/Touch/Latch
    - Read (playback)

## Metering System

### Level Metering
- **Peak Metering**
  * True peak detection
  * Sample peak tracking
  * Peak hold options
  * Clip indication

- **RMS Metering**
  * Configurable window
  * Calibrated scales
  * Reference levels
  * Integration times

### Analysis Tools
- **Spectrum Analysis**
  * Real-time FFT
  * Multiple window types
  * Resolution options
  * Update rates

- **Phase Correlation**
  * Stereo correlation
  * Phase alignment
  * Mono compatibility
  * Display options

## Buffer Management
- **Zero-Copy Architecture**
  * Ring buffer implementation
  * Lock-free operations
  * Cache-line alignment
  * Memory mapping
  * Hardware timing buffers

- **Configuration**
  * Primary: 256 samples
  * Maximum: 2048 samples
  * Minimum: 32 samples
  * Format: 32-bit float

## Performance Requirements

### Latency Targets
- **Transport Timing**
  * Transport response: <1ms
  * Position accuracy: sample-accurate
  * Sync stability: <1µs drift
  * Clock jitter: <100ns

- End-to-end: <5ms
- Processing: <1ms
- Driver: <3ms
- Preview system: <2ms

### Resource Management
- CPU usage per instance: <2%
- Memory footprint optimization
- Cache utilization strategy
- Power management

## Error Handling

### Recovery Systems
- Driver failure recovery
- Buffer underrun handling
- Format negotiation
- State preservation

### Error Reporting
- Platform-native logging
- Performance metrics
- Error conditions
- Recovery actions

### Performance Monitoring
- **Logging Integration**
  * Playback Events
    - Start/stop timestamps
    - Clip identification
    - Performance metrics
  * Error Logging
    - Buffer underruns
    - Format issues
    - Resource limits 

* Automatic Fades
  - System-level protection
    * Clip edges: 1ms fixed
* Format-Specific Processing
  - Stereo Fades
    * Channel-linked processing
    * Width preservation
    * Phase correlation
  - Mono Fades
    * Single channel optimization
    * Pan position preservation
    * Expansion state handling 

### Fade System
- **Automatic Protection Fades**
  * Clip Edges:
    - Fixed 1ms fade with convex curve
      * Quick entry (first 30%)
      * Gradual exit (last 70%)
      * Non-linear power curve
  * Edit Points:
    - Adaptive 0.25ms to 10ms
    - Analysis-based duration:
      * Transient detection
      * Beat position analysis
      * Spectral content matching
      * Zero-crossing optimization
  * Loop Points:
    - Adaptive 0.25ms to 10ms
    - Loop-specific analysis:
      * Beat grid alignment
      * Spectral continuity check
      * Phase correlation

- **User-Configurable Fades**
  * Curve Types (MIDI CC 21/23/25):
    - Logarithmic (0-31)
    - Linear (32-63)
    - Exponential (64-95)
    - S-Curve (96-127)
  * Power Compensation:
    - Per fade type compensation
    - MIDI CC 26-28 for enable/disable
    - Automatic level matching
    - Phase-coherent processing 

## Emergency Protocol Implementation
- **Phase Timing**
  * Phase 1 (<1ms)
    - Immediate muting with format-aware fades
    - Voice deallocation
    - Voice pool state capture
    - Group allocation freeze
  * Phase 2 (<2ms)
    - Resource protection
    - Voice pool protection
    - Format state preservation
    - Buffer management
  * Phase 3 (<10ms)
    - Complete state preservation
    - Resource reallocation
    - Format configuration save
  * Phase 4 (<100ms)
    - System recovery
    - Format restoration
    - Resource rebalancing 

### Core Engine Architecture
- **Processing Chain**
  * Time Source Integration
    - Sample-accurate timing
      * CPU Resource Requirements
        - Dedicated Core Assignment
          * Real-time thread priority
          * Locked core affinity
          * Uninterruptible state
          * Platform-Specific Config
            - Windows: MMCSS/DPC handling
            - macOS: Workgroup policy
            - iOS: Audio session QoS
        - Resource Protection
          * Protected memory regions
          * Cache line optimization
          * DMA channel reservation
          * Interrupt steering control
      * Buffer-to-time mapping
        - Sample position to timecode
        - Timecode to sample position
        - Sub-buffer accuracy
        - Per-clip position tracking
      - Master Clock Integration
        * Direct subscription to central clock
        * Zero-copy time reference
        * Lock-free implementation
        * CPU Reservation Integration
          - Guaranteed processing time
          - Zero audio interruption
          - Timing-critical operations
          - Protected thread execution
        * Emergency protocol protection
      * Hardware timer sync
      * Source Validation
        * Quality metrics per source
        * Drift monitoring and compensation
        * Jitter analysis
        * Stability verification
    - Audio Chain Timing
      * Buffer position tracking
      * Latency compensation
      * Source Management
        * Equal priority MIDI sources
        * Quality-based source selection
        * Automatic failover handling
    - Emergency Protocol
      * Time source failure detection
      * Automatic source switching
      * State preservation
      * Recovery procedures

- **Transport Integration**
  * Grid-Aware Playback
    - Sample Position Management
      * Grid-locked positioning
      * Musical boundary detection
      * Section transition handling
    - Timing Sources
      * Internal clock (80fps base)
      * External sync (blackburst)
      * Manual tempo override
    - Preview System Sync
      * Grid-aligned audition
      * Format-aware preview
      * Resource-conscious timing

  * Grid Analysis Integration
    - Real-time Analysis
      * Background processing only
      * Multi-pass detection
      * Confidence scoring
    - Transport Synchronization
      * Beat-locked playback
      * Grid-aware seeking
      * Preview alignment

### Processing Hierarchy
- **Critical Output Path (Inviolable)**
  * Clip Groups A-D Output
    - Basic playback (highest priority)
      * Format-native voice allocation
      * Protected buffer management
      * Zero interruption guarantee
    - Basic time/pitch (second priority)
      * Simple warp algorithm
      * Grid-locked processing
      * <5% CPU guaranteed

- **Optional Processing (Auto-yielding)**
  * Advanced Processing Features
    - Full Warp Algorithm
      * Available below 70% CPU
      * Yields to basic playback
      * Falls back to basic time/pitch
    - Neural DSP Features
      * Available below 80% CPU
      * Independent resource pool
      * Auto-suspends under load

- **Input/Recording Path (Degradable)**
  * Record A/B
    - Continues with artifacts
    - Never impacts output path
    - Logs discontinuities

- **Preview System (Lowest Priority)**
  * Audition Bus
    - Auto-yields first
    - Independent resources
    - Zero impact guarantee

### Resource Management
- **Processing Allocation Strategy**
  * Resource Monitoring
    - All percentages relative to application ceiling (90% of system)
    - Thresholds:
      * Full Processing: <70% (63% system)
      * Basic Processing: 70-90% (63-81% system)
      * Emergency Mode: >90% (>81% system)
  * Critical Resources (Protected)
    - Basic Playback Voices
      * Format-native allocation
      * Never interrupted
    - Basic Time/Pitch DSP
      * Simple algorithm guaranteed
      * <5% app ceiling (4.5% system CPU)
  * Optional Resources (Dynamic)
    - Warp Processing Pool
      * 70% CPU threshold
      * Auto-downgrade path
    - Neural Processing Pool
      * 80% CPU threshold
      * Suspend/resume capability 

### Processing Hierarchy Overview
- Critical Path
  * Basic Playback: Format-native, protected
  * Basic Time/Pitch: Grid-locked, guaranteed
- Optional Features
  * Advanced Processing (yields at 70/80%)
  * Input/Recording (accepts artifacts)
  * Preview System (yields first)

### Processing Hierarchy
- **Critical Output Path (Inviolable)**
  * Clip Groups A-D Output
    - Basic playback (highest priority)
      * Format-native voice allocation
      * Protected buffer management
      * Zero interruption guarantee
    - Basic time/pitch (second priority)
      * Simple warp algorithm
      * Grid-locked processing
      * <5% CPU guaranteed

- **Optional Processing (Auto-yielding)**
  * Advanced Processing Features
    - Full Warp Algorithm
      * Available below 70% CPU
      * Yields to basic playback
      * Falls back to basic time/pitch
    - Neural DSP Features
      * Available below 80% CPU
      * Independent resource pool
      * Auto-suspends under load

- **Input/Recording Path (Degradable)**
  * Record A/B
    - Continues with artifacts
    - Never impacts output path
    - Logs discontinuities

- **Preview System (Lowest Priority)**
  * Audition Bus
    - Auto-yields first
    - Independent resources
    - Zero impact guarantee

### Resource Management
- **Processing Allocation Strategy**
  * Resource Monitoring
    - All percentages relative to application ceiling (90% of system)
    - Thresholds:
      * Full Processing: <70% (63% system)
      * Basic Processing: 70-90% (63-81% system)
      * Emergency Mode: >90% (>81% system)
  * Critical Resources (Protected)
    - Basic Playback Voices
      * Format-native allocation
      * Never interrupted
    - Basic Time/Pitch DSP
      * Simple algorithm guaranteed
      * <5% app ceiling (4.5% system CPU)
  * Optional Resources (Dynamic)
    - Warp Processing Pool
      * 70% CPU threshold
      * Auto-downgrade path
    - Neural Processing Pool
      * 80% CPU threshold
      * Suspend/resume capability 

- **Voice Pool Management**
  * Primary Voice Pool:
    - Session Voice Pool:
      * System capability assessment 
      * Format-native processing
      * Channel configuration tracking
      * Group minimum guarantees
      * Emergency protocol roles
      - Allocation Strategy:
        * Per Group (A-D):
          * Choke mode: max 3 voices per Clip Group, mono or stereo
          * Reserved allocation ensures consistent performance
          * Eliminates inter-group voice competition
          * Free mode: Dynamic allocation from shared pool
            - Uses remaining voices after choke reservations
            - FIFO allocation up to system maximum
        * Format-native allocation preferred
        * Mono optimization when needed
      - Resource Monitoring:
        * Current system load
        * Available voice capacity
        * Format-specific overhead
        * System headroom calculation
        * Continuous CPU measurement
        * Per-voice resource tracking 

### Voice Management
  * Voice Pool Configuration
    - Fixed maximum allocation:
      * 48 mono voices as 24 stereo voices by default, assignable as mono
      * +1 dedicated stereo voice for Audition Bus
    - Format-native processing
    - Resource-aware scaling 