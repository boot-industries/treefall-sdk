# Session Metadata Specification

## Core Structure

### Layout Configuration
- **Page Configuration**
  * View mode
    - Single/Dual page
    - Active page focus
  * Per-page state
    - Tab states (8 per page)
    - Grid layouts
    - Active tab selection
  * Grid dimensions
    - Current size
    - Previous size
    - Resize timestamp
    - Layout version
  * Security Implementation:
    - Process isolation per core_security.md
    - State preservation protocols
    - Format integrity validation

### Audio Configuration
- **Bus Architecture**
  * Output Busses
    - Clip Groups A-D
      * Group assignments
      * Level settings
      * Routing state
    - Audition (Import/Edit)
      * Level settings
      * Current routing
      * Last used state
  * Input Busses
    - Record A/B
      * Format (stereo)
      * Level settings
      * Routing state
    - LTC
      * Format (mono)
      * Sync status
      * Error state

- **Routing Matrix**
  * Physical Outputs
    - Output assignments
    - Level settings
    - Format configuration
  * Monitor Configuration
    - Output routing
    - Level settings
    - Audition routing
    - Monitor states

### Clip Management
- **Grid Assignment**
  * Clip references
    - File paths
    - Clip IDs
    - Grid positions
  * Group assignments
    - Clip Group (A-D)
    - Choke states
    - Level settings
  * Visual properties
    - Button labels
    - Color coding
    - Status indicators

### Transport State
- **Tempo Settings**
  * Global tempo
    - Current BPM
    - Sync status
    - Source selection
  * Clip sync states
    - Sync enabled clips
    - Original tempos
    - Stretch/pitch states

## Performance Data

### Resource State
- **Time Source Configuration**
  * Source Priority Settings
    - Primary source selection
    - Fallback order configuration
    - Quality thresholds
    - Auto-failover settings
  * Health Monitoring Config
    - Drift tolerance limits
    - Jitter thresholds
    - Quality scoring parameters
    - Recovery triggers
  * Security Settings
    - Source authentication
    - Packet verification
    - Access control lists
    - Secure distribution

- **Emergency Protocol Configuration**
  * Phase Timing Requirements
    - Phase 1: <1ms response
    - Phase 2: <2ms response
    - Phase 3: <10ms response
    - Phase 4: <100ms response
  * Thread Configuration
    - Audio thread: real-time priority
    - UI thread: non-blocking
    - Resource thread: async operations
  * State Preservation
    - Atomic operation tracking
    - Recovery point management
    - Format state preservation

- **Voice Allocation**
  * Format-Native Configuration
    - Processing requirements
    - Conversion policies
    - Resource tagging
    - Allocation strategies
  * Current usage
    - Active stereo/mono voices
    - System pool status (24 stereo/48 mono)
    - Dedicated audition voice status
    - FIFO allocation order
    - Resource load relative to 90% ceiling
  * Limits
    - System maximum: 24 stereo/48 mono voices
    - Choke mode: max 3 voices per Clip Group, mono or stereo
      * Reserved allocation ensures consistent performance
      * Eliminates voice competition between groups
    - Resource thresholds:
      * Warning: 80% app ceiling
      * Critical: 90% app ceiling
      * Recovery: 70% app ceiling
    - FIFO allocation rules
    - Audition bus protection status

- **Memory Usage**
  * Buffer allocation
    - Audio buffers
    - Cache usage
    - Temp storage
  * Resource tracking
    - CPU usage
    - Memory stats
    - Cache efficiency

### Error State
- **System Health**
  * Error conditions
    - Current errors
    - Warning states
    - Recovery status
  * Performance metrics
    - Buffer health
    - Dropout history
    - Sync accuracy

## Session Management

### Save State
- **Version Control**
  * Session version
  * File format version
  * Compatibility info
  * Update history

- **Recovery Data**
  * Auto-save state
    - Last save time
    - Change history
    - Recovery points
  * Backup status
    - Backup locations
    - Version tracking
    - Integrity status
  * Missing file status
  * Search candidates
  * Reconciliation history

### Platform Data
- **System Integration**
  * Platform specifics
    - OS version
    - Driver versions
    - Hardware config
  * Performance settings
    - Buffer sizes
    - Priority states
    - Resource limits

## Storage Implementation

### Database Structure
- **Core Tables**
  * Grid configuration
  * Audio routing
  * Control mapping
  * State history

- **Performance Data**
  * Real-time state
  * Cached values
  * Quick-access fields
  * Search indexes

### Recovery System
- **State Preservation**
  * Auto-save points
  * Backup rotation
  * Version control
  * Recovery markers

### Media Management
- **Project media folder**
  * Project media selection
  * Central repository management
- **File reconciliation state**
  * Clip file tracking
  * Directory structure
  * Storage optimization
  * Path management
- **Search history**
  * Metadata-based search
  * Proximity search
  * Multiple candidate handling
  * User confirmation
- **Project Media Path**
  * Primary folder location
  * Alternate search paths
  * Last known good paths
  * Network share mappings

## Platform Integration

### Windows
- **Session Storage**
  * Project file format
  * Auto-save location
  * Backup management
  * Search integration

### macOS
- **Session Storage**
  * Bundle structure
  * Version management
  * Spotlight integration
  * Backup handling

### iOS
- **Session Storage**
  * Container management
  * iCloud integration
  * State preservation
  * Backup strategy

### Recovery Data
- **File Location History**
  * Original file paths
  * Alternative locations found
  * Search patterns used
  * Reconciliation history

### Logging Configuration
- **Export Settings**
  * Format preferences (CSV/JSON/XML)
  * Cloud service credentials
  * Auto-export schedules
  * Retention policies

### Clip Data
- **Timing Metadata**
  * Tempo Analysis Data
    - Musical Tempo Map
      * Section-by-section BPM
      * Warp marker positions
      * Transition confidences
      * Section boundaries
    - Confidence scores
    - Alternative tempos
    - Musical structure markers
    - Sync point history
  * Performance Data
    - Time division alignment accuracy
    - Sync stability metrics
    - Recovery event history
  * Timestamp Records
    - Start time source and value
    - Source switching history
    - Drift correction data
    - Validation results

### Timing Configuration
- **Master Clock Settings**
  * Thread Configuration
    - CPU Reservation
      * Core affinity settings
      * Priority configuration
      * Isolation parameters
      * Uninterruptible status
    - Platform-Specific Settings
      * Windows: THREAD_PRIORITY_TIME_CRITICAL config
      * macOS: THREAD_TIME_CONSTRAINT_POLICY params
      * iOS: QOS_CLASS_USER_INTERACTIVE settings
  * Source Configuration
    - LTC Input
      * Device selection
      * Buffer configuration
      * Quality thresholds
    - MIDI Sources
      * Source Management
        - Individual configuration
        - Equal priority handling
        - Per-source monitoring
        - Independent validation
      * Quality Settings
        - Source-specific thresholds
        - Individual drift limits
        - Jitter tolerances
        - Stability requirements
    - PTP Configuration
      * Network settings
      * Sync parameters
      * Monitoring thresholds
  * Health Monitoring
    - Quality Metrics
      * Per-source tracking
      * Historical data
      * Performance trends
    - Failover Configuration
      * Trigger conditions
      * Recovery procedures
      * State preservation

### Audio Processing State
  * Global Settings
   - Fade Defaults
     * IN fade: {time: 0-3.0s, curve_type, power_comp}
     * OUT fade: {time: 0-3.0s, curve_type, power_comp}
     * Crossfade: {time: 0-3.0s, curve_type}
   - Clock Configuration
     * Source priorities: [ordered_source_list]
     * Per-source settings: {
       enabled: boolean,
       quality_threshold: float,
       failover_behavior: enum
     }

### Global Configuration
  * Time Management
  - Time Division Settings
    * Frame rates per group
    * Musical tempo options
    * Time division snap behavior
  - Clock Source Priority
    * Source order list
    * Quality thresholds
    * Failover behavior 

### Timing Configuration
  * Master Clock Settings
  - Time Division Data
    * Frame rate settings
    * Musical tempo mapping
    * Time division markers 

### Performance Metrics
  * Timing Analysis
    - Source Quality
      * Drift measurements
      * Jitter analysis
      * Time Division correlation
    - Resource Impact
      * CPU utilization
      * Memory footprint
      * Voice allocation

### Format Processing
  * Native Processing
    - Format state security
    - Voice allocation
    - Time Division status
    - Protection fade system 