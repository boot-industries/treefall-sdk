# Session Structure Specification

## Core Storage Structure

### Application Folders
- **Global Storage**
  * Sessions Folder
    - Default location for session files
    - Session file management
    - Auto-save location
    - Backup storage
  * Exports Folder
    - Default export destination
    - Packaged session exports
    - Page exports
    - Clip list exports
  * Logs Folder
    - Application logs
    - Playback history
    - Error logs
    - Performance metrics
    - Export formats:
      * CSV
      * JSON
      * XML
    - Cloud sync options:
      * Google Drive
      * Dropbox
      * AWS S3

### Project Media Management
- **Media Folder Configuration**
  * Initial Setup:
    - Mandatory folder selection dialog
      * Required on new session creation
      * Default location suggestion
      * Network share support
      * Removable media handling
    - Validation:
      * Path accessibility check
      * Write permission verification
      * Space availability check
      * Network stability test
  * Folder Reassignment:
    - Accessible via:
      * Main menu
      * Session settings
      * Quick access toolbar
    - Migration Options:
      * Move files to new location
      * Keep files in place
      * Copy files to new location
    - Status Display:
      * File count/total size
      * Migration estimate
      * Potential issues
      * Required actions

- **Intelligent File Recovery**
  * Search Strategy:
    - Metadata matching
    - Hash verification
    - Audio property comparison
    - Proximity search
  * Recovery Process:
    - Automatic candidate detection
    - User confirmation interface
    - Batch reconciliation options
    - Recovery audit trail
  * Reconciliation System:
    - Multiple location support
    - Metadata preservation
    - Settings migration
    - User confirmation flow

### File Recovery
- **Missing File Handling**
  * Detection Process
    - Load-time verification
    - Runtime path validation
    - Format validation
    - Hash verification
  * Format-Specific Recovery
    - Stereo Files
      * Channel verification
      * Width preservation
      * Resource validation
    - Mono Files
      * Pan position recovery
      * Expansion settings
      * Resource validation

### Format-Native Media Management
- **Project Media Operations**
  * Format-Aware File Handling
    - Move Operations
      * Processing chain preservation
      * Resource impact calculation
      * State verification
    - Copy Operations
      * Format validation
      * Resource requirement check
      * Processing chain duplication
    - Delete Operations
      * Resource reclamation
      * Format pool updates
      * State cleanup

- **Format Recovery System**
  * Missing File Resolution
    - Format-Specific Search
      * Channel configuration matching
      * Sample rate verification
      * Bit depth validation
    - Resource-Aware Recovery
      * Processing chain reconstruction
      * Voice allocation adjustment
      * Resource rebalancing

### Audition System Integration
- **Resource Isolation**
  * Dedicated Preview Bus
    - Complete isolation from Groups A-D
    - Independent DSP chain allocation
    - Separate buffer management
    - Zero resource sharing with main outputs
  * Performance Guarantees
    - Groups A-D performance firewall
    - Preview system auto-yields under load
    - No preview impact on emergency protocol
    - Format-native isolation per bus
  * Format-Native Processing:
    * Independent voice allocation per format
    * Zero-impact buffer management
    * Sample-accurate preview timing
    * Format-specific DSP chains

- **Preview System**
  * Warp Preview Capabilities
    - Full Warp Mode (below 70% CPU)
      * Complete algorithm preview
      * Real-time parameter updates
    - Basic Warp Mode (above 70% CPU)
      * Simple algorithm fallback
      * Grid-locked updates only
  * Resource Management
    - Independent from main playback
    - Automatic tier downgrade
    - Emergency protocol protection

## Core Structure

### Session Container
- **Core Data**
  * Grid state
    - Button assignments (configurable matrix)
    - Grid dimensions
    - Current rows/columns
    - Last resize timestamp
    - Previous dimensions (for undo)
    - Layout version
    - Group assignments (A-D)
    - Tab configurations (8 tabs)
  * Musical Analysis State
    - Grid Analysis Results
      * Per-Clip Analysis:
        - Primary tempo detection
        - Confidence metrics
        - Alternative suggestions
        - Section boundaries
      * Session-Level Grid:
        - Global tempo map
        - Override states
        - Sync configurations
    - Analysis Cache Management
      * Processing status tracking
      * Resource usage monitoring
      * Background analysis queue
    - Grid Integration State
      * Transport synchronization
      * Edit point alignment
      * Preview system mapping
      * Visual feedback state
  * Audio references
    - File paths
    - Metadata links
    - Format info
  * Processing state
    - Time/pitch settings
    - Level/pan values
    - Marker positions (cue/warp)
  * Page Data
    - Page configuration
      * Single/Dual page mode
      * Active page focus
    - Per-page state
      * Tab configurations (8 tabs)
      * Grid layouts
      * Clip assignments

### Save Format
- **File Structure**
  * JSON manifest
    - Grid configuration
    - Audio references
    - Processing states
  * Metadata database
    - Clip information
    - Marker data
    - Usage history
  * Platform Compatibility
    - Format Translation
      * Windows → macOS
        - ASIO → CoreAudio mapping
        - Resource state adaptation
      * Desktop → iOS
        - Format optimization rules
        - Resource scaling policies
      * Version compatibility
        - Forward compatibility
        - Backward compatibility

For additional session structure details, see `data_specs/session_metadata.md`

### Auto-save System
- **Recovery Points**
  * State Storage
    * Sessions folder: Manual saves
    * Auto-save directory
      - Periodic snapshots
      - Emergency saves
      - Recovery points
  * Save Triggers
    - Periodic (configurable)
    - On significant changes
    - Before risky operations
    - Emergency saves

### Page Export Format
- **Export Package**
  * Storage Location
    - Within Exports folder
    - Subfolder by date/session
    - Version tracking
  * Package Contents
    - JSON manifest
      * Page metadata
      * Tab configurations
      * Grid layouts
    - Audio references
      * Clip metadata
      * File dependencies
    - Import settings
      * Merge options
      * Conflict handling
    - Format State Export
      - Stereo/Mono configurations
      - Processing chain states
      - Resource requirements
    - Import Validation
      - Format compatibility check
      - Resource availability check
      - State consistency verification

### Core Data
- **Processing State**
  * Format Management
    - Native format tracking
    - Channel configuration
    - Resource allocation state
    - Bus routing preferences
  * Voice Allocation
    - Current voice usage
    - Resource monitoring
    - Scaling thresholds
  * Processing Configuration
    - Format-native preferred
    - Dynamic allocation enabled
    - Mono optimization allowed
  * Fade System
    - Global Defaults
      - IN fade: {time: number, curve: string, power: boolean}
      - OUT fade: {time: number, curve: string, power: boolean}
      - Crossfade: {time: number, curve: string}
    - Protection Settings
      - Edge fade duration: 1ms (fixed)
      - Edit point range: 0.25-10ms
      - Loop point range: 0.25-10ms
    - Neural Processing
      - Protection fade optimization only
      - No user fade analysis
      - Zero impact guarantee

### Resource State
- **CPU Reservation State**
  * Thread Configuration
    - Core Assignment
      * Assigned core ID
      * Lock verification status
      * Parking prevention state
      * ISR/DPC routing map
    - Priority Settings
      * Current thread priority
      * QoS class status
      * Scheduler parameters
      * Preemption settings
  * Performance Metrics
    - Core Utilization
      * Reserved core load
      * System interference count
      * Recovery incidents
      * Violation history
    - Timing Accuracy
      * Deviation measurements
      * Jitter statistics
      * Recovery latencies
      * Impact assessment

- **Voice Management**
  * System Configuration
    - "Voice Pool Configuration:",
      * "System Allocation:",
        - "24 stereo voices (48 mono) across Groups A-D",
        - "1 dedicated stereo voice for Audition Bus",
        - "Choke mode: max 3 voices per Clip Group, mono or stereo",
          * "Reserved allocation ensures consistent performance",
          * "Eliminates voice competition between groups",
        - "Format-native processing required"
  * Resource Management
    - Warning threshold: 80% app ceiling
    - Critical threshold: 90% app ceiling
    - Recovery threshold: 70% app ceiling
  * Processing Configuration
    - Format-native preferred
    - Dynamic allocation enabled
    - Mono optimization allowed

## Import/Export Operations

### Import/Export Modes
1. Full Session
   - Complete session state including all assets
   - Resource allocations
   - Grid configurations
   - All metadata
   
2. Metadata Only
   - Applies to available or selected clips
   - Updates properties without media transfer
   - Maintains existing media links
   
3. Tab Export
   - Single tab configuration and content
   - Associated clip metadata
   - Tab-specific resource allocations
   
4. Clip List
   - Non-sequential clip selections
   - Metadata and media options
   - Preserves selection order

### Performance Rights Management
- **Session Context**
  * Show Information
    - Production name
    - Venue details
    - Performance dates
    - Rights context
  * Reporting Configuration
    - Default report depth
    - Preferred formats
    - Auto-export settings
    - PRO preferences

- **Log Management**
  * Storage Strategy
    - Performance logs (priority)
      * Separate from technical logs
      * Redundant storage
      * Backup requirements
    - Export Schedule
      * Auto-export options
      * Retention policy
      * Archive format
  * Data Integrity
    - Validation system
    - Backup strategy
    - Recovery procedures
    - Audit trail

### Performance Documentation
- **Cue Sheet Management**
  * Session Context
    - Show/production details
    - Default templates
    - Export preferences
  * Data Collection
    - Real-time performance logs
    - Manual annotations
    - Usage categorization
  * Review State
    - Verification status
    - Missing data flags
    - Export readiness

### Bus Architecture
- **Output Busses**
  * Clip Groups A-D
    - Group assignments
    - Level settings
    - Routing state
    - Performance guarantees:
      * Absolute output priority
      * Zero impact from input operations
      * Protected voice allocation
      * Guaranteed buffer integrity
  * Audition (Import/Edit)
    // ... existing content ...

- **Input Busses**
  * Record A/B
    - Format (stereo)
    - Level settings
    - Routing state
    - Resource Behavior:
      * Yields to Clip Group output
      * Accepts artifacts over interruption
      * Independent buffer management
      * Failure modes:
        - Continue recording with artifacts
        - Log discontinuities
        - Maintain output integrity
        - Never interrupt Clip Groups A-D
  * LTC
    // ... existing content ...

### Processing Priority
- **Resource Allocation Hierarchy**
  * Critical Path (Protected)
    - Clip Groups A-D Output
      * Basic playback (highest priority)
        - Format-native voice allocation
        - Protected buffer management
        - Zero interruption guarantee
      * Basic time/pitch (second priority)
        - Simple warp algorithm
        - Grid-locked processing
        - <5% CPU guaranteed
    - Resource Protection
      * Format-native processing guaranteed
      * Basic DSP always available
      * State preservation ensured

  * Optional Features (Degradable)
    - Advanced Processing
      * Full Warp Algorithm (yields at 70% CPU)
      * Neural DSP Features (yields at 80% CPU)
      * Auto-downgrade paths defined
    - Input/Recording
      * Accepts artifacts over interruption
      * Never impacts output path
      * Logs discontinuities
    - Preview System
      * Lowest priority
      * Auto-yields first
      * Zero impact guarantee

### Resource State
- **Voice Management**
  * Group-based limits:
    - 3 voices per group (choke mode)
    - DSP-limited allocation (free mode)
    - Format-native processing required

  * Limits
    - System maximum: 24 stereo/48 mono voices
    - Per-group capacity: 3 voices (choke mode)
    - Resource thresholds:
      * Warning: 80% app ceiling
      * Critical: 90% app ceiling
      * Recovery: 70% app ceiling

  * Voice Assignment
    - "Processing Configuration:"
      * "Channel configuration status"
      * "Format-native processing state"
      * "Resource optimization flags"
      * "Emergency protocol priority"
    - Allocation Strategy:
      * Per Group (A-D): 3 voices in choke mode
      * Dynamic additional allocation
      * Format-native preferred
      * Mono optimization allowed

### Time Management
- **Master Clock Management**
  * Clock Manager Architecture
    - Zero-copy distribution system
    - Lock-free time access
    - Thread-safe design
    - Emergency protocol integration
  * Time Source Hierarchy
    - Primary Sources
      * LTC via audio input
      * MTC Sources:
        - All MIDI sources operate as peers
        - Individual quality monitoring
        - Source-specific validation
        - Equal priority handling
    - Secondary: PTP/IEEE 1588
    - Tertiary: System Clock
    - Optional: Blackburst/Tri-level
  * Health Monitoring
    - Source Quality Metrics
      * Drift measurement (<100μs warning)
      * Jitter analysis
      * Stability scoring
    - Automatic Failover
      * Quality threshold monitoring
      * Source validation
      * State preservation
      * Recovery procedures

### Subsystem Time Distribution
- **Subscription Model**
  * Direct Clock Access
    - Clip playback engine
    - Time Division system
    - Neural processing
    - Performance logging
  * Format-Specific Integration
    - Stereo timing alignment
    - Mono positioning
    - Channel synchronization
    - Phase correlation

### Emergency Protocol
- **Time Source Failure**
  * Immediate failover (<1ms)
  * Source validation
  * State preservation
  * Recovery procedures
* Format State Protection
  * Timing integrity
  * Channel synchronization
  * Time Division alignment
  * Recovery points

### Core Configuration
  * Time Management
    - Clock Source Priority
      * Source configuration
      * Quality thresholds
    - Time Division Configuration
      * Time division settings
      * Processing state
    - Format Security
      * Protection systems
      * State preservation

// ... rest of session structure specification 