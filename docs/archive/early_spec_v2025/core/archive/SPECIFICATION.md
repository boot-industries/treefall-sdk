# Core System Specification

This document serves as the definitive source of truth for all system requirements.
All other documentation and rules must align with these specifications.

## Document Hierarchy
1. This specification defines core requirements
2. Implementation rules detail how to meet these requirements
3. Platform-specific rules extend these for each target platform
4. Development standards ensure consistent implementation

## Core System Requirements

### Audio System
  * Voice Management
    - Dynamic voice allocation
    - Format-native processing
    - Resource-aware scaling
    - Voice Architecture:
      * 48 mono voices as 24 stereo voices by default, assignable as mono
      * +1 dedicated stereo voice for Audition Bus
      * 3 voices per group in choke mode
    - Processing Requirements:
      * Format-native processing
      * Resource-aware scaling
      * Emergency protocol protection

  * Performance Requirements
    - Audio path latency: <5ms end-to-end
    - Emergency response: <1ms to mute
    - Transport control: <1ms

## 1. Performance Requirements

### Timing Guarantees
- Audio Path: <5ms end-to-end
  * This sub-5ms requirement ensures professional-grade audio performance
  * Derived from human perception thresholds - delays above 5ms become noticeable in live monitoring
  * Critical for maintaining sync in multi-output scenarios
  * Enables seamless integration with external audio systems

- UI Feedback: <16ms (60fps)
  * Based on screen refresh rates and human perception of smooth motion
  * 16ms threshold maintains perceived immediacy of user actions
  * Crucial for professional operation where timing is critical
  * Allows for precise visual feedback during performance

- Hardware Control: <1ms
  * Sub-millisecond response essential for hardware integration
  * Enables precise synchronization with external equipment
  * Critical for broadcast environments and live performance
  * Maintains professional reliability standards

- Emergency Protocol:
  * Phase 1: <1ms (immediate)
    - Instantaneous response to critical failures
    - Prevents audio system lockup
    - Protects downstream equipment
    - Essential for broadcast environments
  
  * Phase 2: <2ms (protection)
    - Implements protection fades
    - Prevents audio artifacts
    - Maintains system stability
    - Protects connected equipment
  
  * Phase 3: <10ms (preservation)
    - Captures system state
    - Preserves user data
    - Logs error conditions
    - Enables recovery analysis
  
  * Phase 4: <100ms (recovery)
    - Restores stable system state
    - Reinitializes audio engine
    - Recovers user session
    - Provides error feedback

### Resource Management
- Warning Threshold: 80% app ceiling (72% system)
  * Scale back allocation
    - Proactive resource management prevents critical states
    - Maintains system stability under heavy load
    - Preserves critical functionality
    - Enables graceful degradation
  
  * Format-specific optimizations
    - Voice allocation optimization
    - Adaptive buffer sizing
    - Dynamic sample rate conversion
    - Channel count optimization
  
  * User notification
    - Clear visual feedback
    - Non-intrusive warnings
    - Actionable information
    - Performance impact details

- Critical Threshold: 90% capacity
  * Emergency protocol activation
    - Prevents system failure
    - Protects user data
    - Maintains core functionality
    - Enables recovery
  
  * Format-priority preservation
    - Maintains critical playback
    - Suspends non-essential processing
    - Preserves session integrity
    - Protects audio quality
  
  * Resource recovery
    - Aggressive voice reduction
    - Cache purging
    - Buffer optimization
    - Memory defragmentation

- Recovery Threshold: 70% capacity
  * Resume normal operation
    - Restores full functionality
    - Reactivates suspended features
    - Rebuilds processing chains
    - Resumes optimization
  
  * Resource reallocation
    - Rebuilds voice pools
    - Restores buffer sizes
    - Reactivates processing
    - Optimizes memory usage
  
  * State restoration
    - Recovers user settings
    - Restores processing chains
    - Rebuilds audio routing
    - Reactivates automation

## 2. Format-Native Processing

### Definition
Format-native processing means maintaining audio in its original format throughout the processing chain unless explicitly converted. This architectural decision:
- Minimizes processing overhead
- Reduces conversion artifacts
- Optimizes resource usage
- Maintains audio quality

This includes:
- Sample rate preservation
  * Prevents unnecessary resampling
  * Maintains original quality
  * Reduces processing load
  * Optimizes latency

- Bit depth maintenance
  * Preserves dynamic range
  * Prevents quantization errors
  * Optimizes processing quality
  * Maintains headroom

- Channel configuration respect
  * Preserves spatial information
  * Optimizes voice allocation
  * Reduces unnecessary mixing
  * Maintains separation

- Resource-aware allocation
  * Optimizes memory usage
  * Reduces CPU overhead
  * Enables efficient scaling
  * Maintains performance

### Implementation Requirements
- Direct hardware access where available
  * Minimizes latency
  * Optimizes performance
  * Reduces overhead
  * Enables professional timing

- Zero-copy operations when possible
  * Reduces memory usage
  * Minimizes latency
  * Optimizes throughput
  * Prevents fragmentation

- Format-specific voice allocation
  * Optimizes resource usage
  * Maintains audio quality
  * Enables efficient scaling
  * Preserves processing headroom

- Power-aware processing chains
  * Optimizes battery life
  * Maintains performance
  * Enables thermal management
  * Preserves system stability

## Platform Integration

### Overview
Platform-specific implementations must adhere to the core requirements while optimizing for their respective environments. See [Platform Implementation Specifications](../platform/PLATFORM_SPEC.md) for detailed requirements.

### Platform Documentation
- [Common Requirements](../platform/PLATFORM_SPEC.md) - Cross-platform specifications and requirements
- Platform-Specific Implementations:
  * [Windows](../platform/windows.md) and [Audio](../platform/windows_audio.md)
  * [macOS](../platform/macos.md) and [Audio](../platform/macos_audio.md)
  * [iOS](../platform/ios.md) and [Audio](../platform/ios_audio.md)

### Implementation Requirements
- Format-native processing on all platforms
- Emergency protocol compliance
- Resource-aware scaling
- Power-aware processing chains
  * Optimizes battery life
  * Maintains performance
  * Enables thermal management
  * Preserves system stability

# Technical Specification

## Overview
This document serves as the source of truth for all technical requirements and implementation details.

## Feature Tiers

### Core Tier (Always Available)
- **Audio Engine**
  * Format-native playback
  * Basic waveform analysis
  * Simple fade system
  * Emergency protocols
- **Time Display System**
  * Independent Counter Architecture
    - Per-clip time tracking
    - Sample-accurate start points
    - Independent of system clock
    - Zero-impact implementation
  * Timestamp Sources
    - External Timecode (LTC/MTC)
      * Frame-accurate sync
      * Source quality monitoring
      * Drift compensation
    - Network Time (PTP)
      * Sub-microsecond accuracy
      * Network path monitoring
      * Jitter management
    - System Time
      * Millisecond resolution
      * Drift monitoring
      * Automatic realignment
  * Logging Integration
    - Performance Rights Logging
      * Frame-accurate event capture
      * Source quality tracking
      * Timestamp hierarchy respect
      * Privacy-aware logging
    - System Performance Logging
      * Counter accuracy metrics
      * Drift statistics
      * Source switching events
      * Validation results
  * Validation Requirements
    - Source Quality
      * Continuous monitoring
      * Auto-switching on degradation
      * Recovery tracking
    - Timestamp Integrity
      * Cross-validation between sources
      * Drift compensation
      * Continuity verification
  * Performance Requirements
    - Display update: 100ms max latency
    - Sample-accurate tracking
    - Zero counter drift
    - Scales to 1000+ simultaneous clips
  * Resource Management
    - Dedicated counter thread
    - Independent of audio processing
    - Memory-efficient tracking
    - Emergency protocol protected
  * Display Modes
    - Time remaining (primary)
    - Elapsed time
    - Total duration
    - Loop count (when applicable)

### Enhanced Tier (Optional)
- **Analysis Features**
  * Import-time grid detection
  * Fade optimization
  * Loop point suggestions
  * Advanced tempo analysis
  * BPM Detection Strategy
    - Initial Analysis
      * Adaptive Analysis:
        - Initial 3 passes required
        - Additional passes (up to 6) if:
          * High tempo variance detected
          * Multiple strong candidates found
          * Low confidence across passes
          * Complex rhythmic content detected
        - Tempo Change Detection:
          * Automatic warp marker placement
          * Tempo metadata per section
          * Transition point analysis
          * Musical boundary detection
      * Confidence scoring per pass
      * Statistical averaging of results
      * Minimum confidence threshold: 85%
    - Analysis Termination
      * Early termination if confidence >95%
      * Stop conditions:
        - Maximum 6 passes reached
        - Stable tempo identified across 3 passes
        - No significant variance in last 2 passes
        - High confidence achieved (>95%)
      * Store confidence with result
      * Log analysis attempts
    - Resource Management
      * Background processing only
      * Yields to playback/UI
      * Results cached permanently
      * No continuous scanning

### Premium Tier (Optional)
- **Neural Features**
  * Real-time smart fades
  * Dynamic grid adjustment
  * Advanced beat detection
  * Real-time tempo adaptation
  * Musical phrase analysis

## Implementation Requirements

### Critical Systems
// ... existing critical systems content ...

### Performance Requirements
- **Timing Specifications**
  * Audio Chain
    - End-to-end response: <5ms
    - Emergency protocol phases:
      * Phase 1: <1ms (immediate muting)
      * Phase 2: <2ms (resource protection)
      * Phase 3: <10ms (state preservation)
      * Phase 4: <100ms (recovery)
  * User Interface
    - UI response: <16ms
    - Visual feedback: immediate
    - Animation: 60fps locked
  * External Control
    - MIDI response: <1ms
    - OSC handling: <50ms
    - Network control: <50ms

### Format Handling
// ... rest of existing content ...

### Session & Media Management
- **Logging System**
  * Real-time Event Capture
    - Clip playback timestamps (start/stop)
    - Duration tracking
    - Clip metadata inclusion
    - Performance metrics
  * Export Capabilities
    - Multiple formats (CSV/JSON/XML)
    - Cloud service integration
    - Automated export scheduling
    - Backup management

- **Session Management**
  * State Preservation
    - Complete session state
    - All clip configurations
    - Grid/tab layouts
    - Processing chains
  * Recovery System
    - Automatic state backup
    - Crash recovery
    - Version control
    - Rollback capability

- **Media Management**
  * Project Repository
    - Centralized media folder
    - Path management
    - Storage optimization
    - Access control
  * File Recovery
    - Intelligent file search
    - Metadata-based matching
    - Multiple candidate handling
    - User confirmation workflow
  * Clip Reconciliation
    - Alternative path support
    - Metadata preservation
    - Session integrity maintenance
    - Batch operations

### Time Display System
- **Master Clock Manager**
  * Core Architecture
    - Single source of truth for all timing
    - Zero-copy read interface
    - Lock-free implementation
    - Thread-safe design
  * Source Priority Chain
    - Primary: LTC/MTC
    - Secondary: PTP (IEEE 1588)
    - Tertiary: System Clock
    - Optional: Blackburst/Tri-level Sync
  * Health Monitoring
    - Real-time drift detection
    - Jitter analysis
    - Source quality scoring
    - Automatic failover
  * Subsystem Integration
    - Clip playback timing
    - BPM detection
    - Neural analysis
    - Performance logging
    - All timing-dependent features

- **Timestamp Sources**
  * External Timecode (LTC/MTC)
    - Frame-accurate sync
    - Primary reference source
    - Dropout detection
    - Auto-failover support
  * PTP Network Time
    - IEEE 1588-2019 support
    - Hardware timestamping support
    - Network path monitoring
    - Security verification
  * Blackburst/Tri-level Sync
    - Genlock reference support
    - Frame rate correlation
    - HD/UHD workflow support
    - Format-specific handling
  * System Time
    - Fallback time source
    - Millisecond resolution
    - Drift compensation
    - Stability monitoring

// ... rest of existing content ... 