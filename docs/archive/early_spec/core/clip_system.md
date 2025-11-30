# Clip System Specification

## Cross-References
This specification relies on:
- Performance requirements defined in `performance.md`
- Error handling patterns defined in `error_handling.md`
- State management rules defined in `state_management.md`

## Core Components

### Clip Management
For comprehensive metadata specifications, see `clip_metadata.md`
- **Loading System**
  * Format handling
    - Project media folder integration
      * User-selected source location
      * Network/removable storage support
      * Path persistence
      * Missing file detection
    - Format validation
    - Sample rate adaptation
    - Format conversion if needed
  * Resource allocation
    - Format-native voices
    - Memory management
    - Cache strategy

### Playback System
- **Voice Management**
  * Format-native playback
    - Stereo clips = stereo voice
      * Native stereo processing
      * Independent channel control
      * Resource monitoring
    - Mono clips = mono voice with pan
      * Efficient mono processing
      * Pan position control
      * Resource optimization
  * Resource-based scaling
    - Resource Management
      * Dynamic allocation
        - Based on global thresholds
          * Warning: 80% capacity
          * Critical: 90% capacity
          * Recovery: 70% capacity
        - Follows global thresholds
    - Auto-scaling thresholds
      * Warning at 80% capacity
      * Critical at 90% capacity
      * Voice reduction strategy
        - Priority-based scaling
        - Format-aware optimization
        - State preservation

- **Clip Groups**
  * Group assignment (A-D)
  * Routing to output busses
  * Level control
  * Choke behavior
    - Per-clip setting
    - Immediate interrupt
    - Honors fade settings
    - Group-based operation

### Marker System
- **Marker Management**
  * Cue Markers
    - Position tracking
    - Sequence management
    - Trigger behavior
    - Time Division snap
  * Warp Markers
    - Tempo alignment
    - Beat detection
    - Time Division sync
  * Fade Markers
    - Fade Implementation
      * User Fades
        - Custom fades override global defaults
        - When no custom fade:
          * Uses global default fade times
          * If global default is 0s, uses 1ms safety fade
        - Custom: Any length (snaps to time division)
        - Power compensation: User-configurable
      * Protection Fades
        - Clip edges: 1ms fixed linear
        - Edit points: 0.25-10ms adaptive linear
        - Loop points: 0.25-10ms adaptive equal-power

- **Grid Analysis**
  * Time Division Analysis
    - Beat Detection
      - Auto-analysis on import
      - Confidence metrics (0-100)
      - Early termination options
      - Manual override support
    - Marker Validation
      - Position verification
      - Sequence integrity
      - Conflict detection

### Audition System
- **Import Audition**
  * Uses Audition bus
  * Format-native playback
  * Zero-impact design
  * Routable output

- **Edit Audition**
  * Uses Audition bus
  * Waveform preview
  * Marker auditioning
  * Loop testing

## Performance Requirements
All performance requirements for the clip system are defined in `performance.md`, including:
- Voice allocation and management
- Memory and resource utilization
- Timing and latency specifications
- Platform-specific optimizations

## Error Handling
Error handling for the clip system follows patterns defined in `error_handling.md`, including:
- Critical audio chain failures
- Resource exhaustion handling
- Recovery procedures
- User notification requirements

## State Management
State management for the clip system follows patterns defined in `state_management.md`

## Clip Control
- **Clip Management**
  * /clip/{id}/fifo - Controls FIFO position (integer value)
  * /clip/{id}/marker_sequence - Sets marker sequence order (integer value)

### Resource Management
- **Threshold Behavior**
  * Warning State (80%)
    - Voice reduction strategy
      * Format-aware scaling
      * Priority preservation
    - User feedback
      * Visual indicators
      * Status updates
  * Critical State (90%)
    - Emergency protocol
      * Immediate voice reduction
      * Format adaptation
      * State preservation
  * Recovery State (70%)
    - Normal operation resume
      * Voice reallocation
      * Format restoration
      * State recovery

### Format Conversion Strategy
- **Resource Pressure Response**
  * 80% Threshold
    - Identify conversion candidates
      * Least recently played
      * Largest resource usage
      * Non-critical formats
    - Prepare conversion paths
      * Calculate resource savings
      * Estimate conversion time
      * Verify target format
  * 90% Threshold
    - Force immediate conversion
      * Highest resource usage first
      * Critical playback exempt
      * Maintain minimum quality 

## Import/Export Handling

### Export Modes
- Full Session: Complete clip state preservation
- Metadata-only: Property transfer without media
- Tab Export: Tab-specific clip collections
- Clip List: Non-sequential selection handling

### Resource Tracking
- Per-clip resource allocation
- Format-specific requirements
- Tab-level resource grouping
- Selection-based resource calculation 

### Time/Pitch Processing
- **Independent Controls**
  * Time Stretching:
    - Range: -50% to +100%
    - Real-time processing
    - Format-native optimization
    - Resource-aware scaling
  * Pitch Shifting:
    - Range: ±12 semitones
    - Cent-level precision
    - Independent of time
    - Format-specific processing
  * Combined Operations:
    - Independent parameter control
    - Zero-impact preview system
    - Resource monitoring
    - Performance optimization

- **Processing Chain**
  * Format-Native Handling:
    - Stereo preservation
    - Phase correlation
    - Channel integrity
    - Resource allocation
  * Preview System:
    - Real-time monitoring
    - Zero-impact design
    - Format validation
    - Resource tracking 

### Recording System
- **Direct Button Recording**
  * Recording Modes:
    - Direct-to-button capture
      * Immediate capture to target button
      * Optional naming workflow
      * Automatic metadata embedding
        - Timestamp
        - Duration
        - Format properties
        - Routing information
    - Background recording
      * Silent operation during playback
      * Configurable pre-roll buffer
      * Resource-efficient processing
      * State preservation
  * Input Configuration:
    - Source selection via routing matrix
    - Format detection and validation
    - Channel configuration
    - Level monitoring
  * Monitoring System:
    - Real-time level metering
    - Visual recording status
    - Format validation feedback
    - Resource usage indication
  * Metadata Integration:
    - Automatic timestamp capture
    - Duration tracking
    - Format properties
    - Session context
  * Auto-save Features:
    - Continuous state preservation
    - Recovery point creation
    - Session backup integration
    - Resource tracking 

### Audio Processing Chain
- **Format Handling**
  * Format-Native Processing
    - Sample rate preservation
      * Prevents unnecessary resampling
      * Maintains original quality
      * Reduces processing load
    - Bit depth maintenance
      * Preserves dynamic range
      * Prevents quantization errors
      * Maintains headroom
    - Channel configuration respect
      * Preserves spatial information
      * Optimizes voice allocation
      * Reduces unnecessary mixing

- **Resource Management**
  * Voice Allocation
    - Format-specific pools
      * Stereo clips = stereo voice
      * Mono clips = mono voice with pan
    - Resource-aware scaling
      * Warning threshold (80%)
      * Critical threshold (90%)
      * Recovery threshold (70%) 

### Format-Native File Management
- **Media Folder Integration**
  * Format Validation
    - Real-time format detection
    - Automatic metadata extraction
    - Format-specific resource estimation
  * Folder Operations
    - Format-aware file moves
    - Processing chain preservation
    - Resource impact prediction

- **Import Processing**
  * Format-Native Pipeline
    - Direct format detection
    - Resource requirement calculation
    - Processing chain allocation
  * Resource Validation
    - Format-specific capacity check
    - Voice allocation verification
    - Processing headroom calculation 

### Format Management
- **Format Detection**
  * Automatic Detection
    - Initial import analysis
    - Channel configuration detection
    - Format property validation
    - Resource requirement calculation
  * Protection System
    - Format state locked after detection
    - Changes only via Edit panel
    - Explicit user confirmation required
    - Change history maintained
    - No external modification paths 

## Emergency Protocol Integration:
  * Phase 1 (<1ms): Immediate stop
  * Phase 2 (<2ms): Voice release
  * Phase 3 (<10ms): State capture
  * Phase 4 (<100ms): Recovery 

### Edit Operations
- **Time Division System**
  * Group-Level Settings
    - Sync Mode Selection
      * Industry-standard rates:
        - Blackburst (75fps/13.33ms)
        - Film (24fps/41.67ms)
        - NTSC-DF (29.97fps drop/33.37ms)
        - NTSC (29.97fps non-drop/33.37ms)
        - PAL (30fps/33.33ms)
        - HD (60fps/16.67ms)
      * Musical Mode:
        - Default: 120 BPM (80fps/12.5ms)
        - Per-clip manual tempo allowed
  * Clip-Level Override
    - Manual tempo setting
    - Preserves group membership
    - Visual feedback when different from group

### Edit Operations
- **Snap Resolution**
  * Base Resolution: 100fps (10ms)
  * Musical Time Division
    - BPM-synchronized divisions
    - User-selectable subdivisions
  * Precision Control
    - Snap to divisions (default)
    - Loose snap (nearest frame)
    - Free positioning
  * Edit point validation
  * State preservation 

### Clip Timing Management
- **Clock Integration**
  * Source Management
    - Clock subscription handling
    - Source quality monitoring
    - Drift compensation
    * Source Priority
      - User-configurable priority order
        * LTC via audio input
        * MTC sources (as peers)
        * PTP/IEEE 1588
        * System clock
        * Blackburst/Tri-level sync
      - Per-source configuration
        * Enable/disable
        * Priority assignment
        * Quality thresholds
        * Failover behavior
    * Quality Metrics
      - Drift measurement (<100μs warning)
      - Jitter analysis
      - Source switching history
    * Global Time Reference
      - All timing derived from Master Clock
      - Sample-accurate synchronization
      - Unified drift compensation
      - Auto-switching on source failure
    * Group Time Division Context
      - Parent group frame rate/tempo
      * Visual grid configuration
      * Override status tracking
    * Position Tracking
      - Sample-accurate timing
      - Format-aware processing
      - Emergency protocol support 

### Edit Operations
- **Snap Resolution**
  * Base Resolution: 100fps (10ms)
  * Musical Time Division
    - BPM-synchronized divisions
    - User-selectable subdivisions
  * Precision Control
    - Snap to divisions (default)
    - Loose snap (nearest frame)
    - Free positioning
  * Edit point validation
  * State preservation 

### Voice Management
- **Voice Architecture**
  * Format-native allocation
  * 48 mono voices as 24 stereo voices by default
  * +1 dedicated stereo voice for Audition Bus
  * 3 voices per group in choke mode
  * Resource-aware scaling
    - Warning threshold (80% app ceiling)
    - Critical threshold (90% app ceiling)
    - Recovery threshold (70% app ceiling)

### Marker System
- **Marker Types**
  * Cue Markers
    - Up to 4 per clip
    - Position anywhere in clip
    - Independent of In/Out points
    - Grid-aligned placement
  * Warp Markers
    - Up to 4 per clip
    - Tempo alignment points
    - Grid-synchronized
    - Format-aware processing
  * Fade Markers
    - Protection fades (system)
      * 1ms fixed at clip edges
      * 0.25-10ms adaptive at edit points
    - User fades (optional)
      * IN/OUT: 0-3s range
      * Curves: Log/Lin/Exp/S-Curve
      * Power compensation option

### Analysis System
- **Import Analysis**
  * Tempo Detection
    - Multi-pass Analysis
      * 3-6 adaptive passes
      * Early termination at >95% confidence
      * Minimum 3 passes required
      * Additional passes if confidence <85%
    - Section Analysis
      * Intelligent content filtering
        - Exclude non-rhythmic sections
        - Skip low-energy regions
        - Identify transition segments
        - Detect ambient/atmospheric content
      * Musical structure detection
        - Identify distinct tempo regions
        - Detect tempo changes/transitions
        - Mark section boundaries
        - Auto-place warp markers at transitions
    - Warp Marker Automation
      * Automatic placement for:
        - Section boundaries
        - Significant tempo changes
        - Complex rhythm transitions
        - Genre/style changes
      * Marker purpose flags
        - Tempo anchor (no warping)
        - Section boundary
        - Transition point
        - Style change
    - Resource Management
      * Background processing only
      * Results cached permanently
      * No continuous scanning
      * Zero impact on playback
    - Confidence Scoring
      * Primary tempo confidence
      * Alternative tempo suggestions
      * Section boundary detection
      * Musical structure markers

  * Grid Analysis
    - Auto-analysis on import
    - Confidence metrics (0-100)
    - Early termination options
    - Manual override support