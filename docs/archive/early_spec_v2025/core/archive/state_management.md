# State Management Specification

## Cross-References
This specification integrates with:
- Error handling patterns defined in `error_handling.md`
- Performance monitoring defined in `performance.md`

## State Transition Rules

### Playback State Changes
- **Play → Stop**
  * Honor fade settings
  * Preserve position
  * Update UI immediately
  * Notify external control

- **Stop → Play**
  * Validate resources
  * Honor pre-roll
  * Update UI immediately
  * Notify external control

- **Emergency Stop**
  * Immediate audio mute
  * Force all clips stop
  * Clear voice allocation
  * Notify all systems

### Resource State Changes
- **Normal → Warning**
  * Trigger at 80% app ceiling (72% system)
  * Begin format-specific optimization
  * Prepare resource recovery
  * Notify user
  * Log condition

- **Warning → Critical**
  * Trigger at 90% app ceiling (81% system)
  * Force resource reduction
  * Format-priority preservation
  * Emergency notification
  * Preserve state

## Core State Systems

### Runtime State
- **Application State**
  * Page Configuration
    - Single/Dual page mode
    - Active page focus
    - Tab states (8 per page)
    - Grid layouts
  * Audio Engine State
    - Voice allocation status
    - Bus routing states
    - Format conversion status
    - Buffer health

- **Playback State**
  * Clip States
    - Playing clips per group
    - Choke states
    - Fade progressions
    - Position tracking
  * Transport State
    - Global tempo
    - Tempo State Management
      * Master BPM control
      * Per-clip tempo states
        - Original tempo
        - Current stretch/pitch
        - Pre-sync settings
      * Global BPM State
        - Enable/disable transitions
        - Active clip preservation
          * Maintain warped state during playback
          * Reset on stop
        - Inactive clip handling
          * Background preparation
          * Default state restoration
      * Transition ramping
        - Time/pitch preservation
        - Smooth state restoration
    - Sync status
    - Beat position
    - Time position

### Persistent State
- **Session Data**
  * Grid Configuration
    - Clip assignments
    - Group assignments
    - Tab layouts
    - Page settings
  * Audio Settings
    - Interface configuration
    - Buffer settings
    - Routing matrix
    - Level settings

- **Clip Settings**
  * Playback Properties
    - Start/End points
    - Loop settings
    - Time Division Override
      * Manual tempo setting
      * Parent group reference
      * Conflict status
      * Override timestamp
      * Visual feedback state
    - Gain values
    - Pan positions
  * Processing State
    - Time/pitch settings
    - Pitch Lock State
      * Independent from tempo sync
      * Maintains original pitch during BPM changes
      * Per-clip override capability
      * State preservation during playback
    - Independent states
      - Manual stretch/pitch
      - Sync-driven changes
      - State preservation
    - Marker positions
    - Fade configurations
    - Choke settings
    - Grid Configuration
      * Detection State
        - Analyzed BPM
        - Confidence level
        - Grid markers
      * Override State
        - Manual BPM setting
        - Grid adjustments
        - Sync mode
    - Global Settings
      * Sync Preferences
        - Blackburst mode (75fps)
        - Auto-detection defaults
        - Grid visualization

- **Global Preferences**
  * Intelligent Processing
    - Smart Fades
      * Enable/disable neural assistance
      * Default: Disabled
      * Per-feature controls:
        - Phase coherence
        - Tempo matching
        - Timbral analysis
      * Processing limits
        - Playback Priority
          * Always yield to playback
          * Separate resource pools
          * Non-blocking operation
        - Failure Response
          * Zero tolerance for errors
          * Immediate standard fade fallback
          * No retry during active playback
          * Clear state preservation
        - CPU threshold (max 15%)
        - Memory allocation (fixed pool)
        - Cache size (user configurable)
        - Recovery behavior
          * Auto-suspend on load
          * Resume thresholds
          * Background recovery

## State Management

### State Tracking
- **Change Detection**
  * Real-time Updates
    - Playback changes
    - Parameter adjustments
    - UI interactions
    - Resource allocation
    - Critical state changes (<1ms)
    - Resource state changes (<2ms)
    - Format state changes (<10ms)
    - Recovery state changes (<100ms)
  * Deferred Updates
    - Configuration changes
    - Layout modifications
    - Session structure
    - Metadata updates
    - Background optimization
    - Cache promotion
    - Resource balancing

- **State Validation**
  * Integrity Checks
    - Resource availability
    - Parameter bounds
    - Configuration validity
    - State consistency
  * Error Detection
    - State corruption
    - Invalid transitions
    - Resource conflicts
    - Version mismatches

### State Persistence
- **Save Operations**
  * Auto-save System
    - Change-triggered saves
    - Timed backups
    - Crash recovery data
    - Version control
  * Manual Saves
    - Session snapshots
    - Configuration presets
    - Page exports
    - Backup creation

- **Load Operations**
  * Session Recovery
    - State verification
    - Resource validation
    - Error recovery
    - Partial restoration
  * Import Handling
    - Page imports
    - Configuration merging
    - Resource allocation
    - Conflict resolution

## Recovery Systems

### State Recovery
- **Error Recovery**
  * Critical Failures
    - Last known good state
    - Resource reallocation
    - State reconstruction
    - User notification
  * Format-Specific Recovery
    - Stereo State
      * Channel configuration
      * Width settings
      * Resource allocation
    - Mono State
      * Pan position
      * Expansion settings
      * Resource allocation
  * Non-critical Issues
    - Partial recovery
    - State repair
    - Resource cleanup
    - Performance optimization

- **Crash Recovery**
  * Auto-save Recovery
    - Backup validation
    - State reconstruction
    - Resource reallocation
    - User notification
  * Session Restoration
    - File verification
    - Resource validation
    - State consistency
    - Error handling

### State Synchronization
- **Multi-thread Sync**
  * Real-time Systems
    - Audio thread state
    - UI thread state
    - Background operations
    - Resource management
  * State Updates
    - Lock-free operations
    - Atomic updates
    - State broadcasting
    - Change notification

## Multi-Device Management

### Device Coordination
- **Role Management**
  * Primary/Secondary roles
  * Permission system
  * State authority
  * Conflict resolution

- **Connection Management**
  * Device discovery
  * State distribution
  * Error recovery
  * Network resilience

### State Distribution
- **Update Propagation**
  * State broadcast
  * Change notification
  * Conflict resolution
  * Version tracking

- **Emergency State Propagation**
  * Phase 1 (Immediate)
    - Audio thread notification
    - Resource thread notification
    - UI thread notification
  * Phase 2 (Protection)
    - Format state capture
    - Resource state freeze
    - UI state lock
  * Phase 3-4 (Recovery)
    - State verification chain
    - Resource reallocation chain
    - UI update chain

- **Performance**
  * <50ms sync latency
  * State coherence
  * Bandwidth efficiency
  * Error resilience

- **Sync Protocol**
  * Real-time State
    - Voice allocation map
    - Resource utilization
    - Format configuration
  * Emergency State
    - Complete snapshots
    - Resource allocation
    - Recovery points
  * Recovery State
    - Pre-emergency data
    - Resource distribution
    - Format settings

## State Preservation

### Real-time State
- **Volatile State**
  * Buffer states
  * Position tracking
  * Level information
  * Route configurations

- **Recovery Points**
  * Auto-save system
  * Crash recovery
  * State rollback
  * Error correction

### Persistent Storage
- **Configuration**
  * User settings
  * Device preferences
  * Layout information
  * Recent history

- **Session State**
  * Global Time State
    - Active time source
    - Source health metrics
    - Drift compensation state
    - Emergency protocol state
  * Performance Metrics
    - Source switching history
    - Jitter measurements
    - Recovery incidents
    - Health indicators

- **Clip Group State**
  * Time Division Configuration
    - Selected Mode
      * Frame Rate Selection
        - Blackburst (75fps)
        - Film (24fps)
        - NTSC-DF (29.97fps drop)
        - NTSC (29.97fps non-drop)
        - PAL (30fps)
        - HD (60fps)
      * Musical Mode (120 BPM)
    - Override Status
      * Per-clip tempo settings
      * Visual conflict indicators
    - Display Configuration
      * Grid visualization
      * Time division markers
      * Feedback indicators

## Security Model

### Access Control
- **Authentication**
  * User validation
  * Device authentication
  * Session tokens
  * Permission levels

- **State Protection**
  * Connection encryption
  * State validation
  * Audit logging
  * Error tracking

## Platform Integration

### Windows
- **State Storage**
  * Registry integration
  * File system state
  * Driver state management
  * Session preservation

- **Sync Services**
  * Network services
  * State distribution
  * Security context
  * Error handling

### macOS
- **State Storage**
  * UserDefaults integration
  * File system state
  * CoreAudio state
  * Session preservation

- **Sync Services**
  * Network services
  * State distribution
  * Security framework
  * Error handling

### iOS
- **State Storage**
  * UserDefaults integration
  * File system state
  * Audio session state
  * Background state

- **Sync Services**
  * Network services
  * Background sync
  * Power efficiency
  * Error handling

### State Hierarchy
- **Critical States**
  * Emergency State
    - Complete system snapshot
    - Resource allocation state
    - Bus configuration state
  * Recovery State
    - Pre-emergency snapshot
    - Resource distribution
    - Voice allocation map
  * Operational State
    - Current configuration
    - Resource utilization
    - Performance metrics

### State Authority Hierarchy
- **Emergency States**
  * Primary Authority: error_handling.md
  * Secondary: resource_management.md
  * Tertiary: clip_system.md

- **Format States**
  * Primary Authority: clip_metadata.md
  * Secondary: resource_management.md
  * Tertiary: bus_architecture.md

### State Recovery
- **Verification Protocol**
  * State Integrity Check
    - Format state verification
      * Channel configuration
      * Processing chain state
      * Resource allocation
    - Thread State Validation
      * Audio thread state
      * Resource thread state
      * UI thread state
  * Recovery Validation
    - Resource consistency
    - Format integrity
    - UI state coherence

### State Authority Hierarchy
- **Emergency States**
  * Primary Authority: error_handling.md
  * Secondary: resource_management.md
  * Tertiary: clip_system.md

- **Format States**
  * Primary Authority: clip_metadata.md
  * Secondary: resource_management.md
  * Tertiary: bus_architecture.md

- **Response Requirements**
  * Phase 1: <1ms (error_handling.md)
  * Phase 2: <2ms (resource_management.md)
  * Phase 3: <10ms (state preservation)
  * Phase 4: <100ms (recovery)

### Critical State Management
- **Core States**
  * Timing System
    - Master Clock State
      * Active source configuration
      * Source health metrics
      * Drift compensation data
      * Emergency protocol state
    - Per-clip time tracking
      * Current position
      * Source selection
      * Drift compensation
      * Thread State
        - Core affinity status
        - Priority level tracking
        - QoS/MMCSS state
        - Power state monitoring
    - Global time reference
      * Primary source state
      * Source quality metrics
      * Resource Protection
        - Core lock verification
        - DPC/ISR routing status
        - Timer resolution state
      * Emergency status
  * Resource Allocation
    - Memory pools
    - Thread states
    - Cache management

### Voice State Management
  * Voice Pool State
    - Fixed pool tracking:
      * 48 mono voices as 24 stereo voices by default, assignable as mono
      * +1 dedicated stereo voice for Audition Bus
      * 3 voices per group in choke mode
    - Format-native state
    - Resource monitoring

### Security Model:
- **Format State Protection:**
  * Stereo state preservation
  * Mono state preservation
  * Resource allocation security
- **Emergency Protocol Integration:**
  * Phase 1 (<1ms): State capture
  * Phase 2 (<2ms): Resource protection
  * Phase 3 (<10ms): Format preservation
  * Phase 4 (<100ms): Recovery validation

### Clip Group State
  * Time Division Configuration
    - Selected Mode
      * Frame Rate Selection
        - Blackburst (75fps)
        - Film (24fps)
        - NTSC-DF (29.97fps drop)
        - NTSC (29.97fps non-drop)
        - PAL (30fps)
        - HD (60fps)
      * Musical Mode (120 BPM)
    - Override Status
      * Per-clip tempo settings
      * Visual conflict indicators
    - Display Configuration
      * Grid visualization
      * Time division markers
      * Feedback indicators

### Session State
- **Interface Configuration**
  * TAB States
    - CLIP GRID dimensions (up to 12x16)
    - Clip button assignments
    - Group assignments (A-D)
    - View mode settings
      * SINGLE PAGE/DUAL PAGE mode
      * Column reduction preferences
  * Display Configuration
    - Active TAB selection
    - DUAL PAGE TAB pairing
    - Visual feedback states
    - Resource allocation

- **Clip Settings**
  * Playback Properties
    - Start/End points
    - Loop settings
    - Time Division Override
      * Manual tempo setting
      * Parent group reference
      * Conflict status
      * Override timestamp
      * Visual feedback state
    - Gain values
    - Pan positions 