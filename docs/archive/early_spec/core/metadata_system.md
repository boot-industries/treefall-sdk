# Metadata System Specification

## Core Architecture

### Database Structure
- **Primary Store**
  * SQLite implementation
  * JSON for complex data
  * Binary for performance data
  * Cache for quick access

### Performance Design
- **Access Requirements**
  * Query Performance
    - Hot metadata: <1ms
    - Warm metadata: <5ms
    - Cold metadata: Background load
  * Thread State Records
    - Core Assignment History
      * Core ID tracking
      * Migration events
      * Parking incidents
      * Power state changes
    - Priority Level History
      * Level transitions
      * QoS/MMCSS changes
      * Scheduler events
      * Preemption incidents
    - Protection State History
      * Memory isolation events
      * Cache optimization changes
      * DPC/ISR routing updates
  * Cache Strategy
    - L1: Active session data
    - L2: Recent/likely data
    - L3: Background indexed data

## Data Categories

### Clip Metadata
- **Basic Information**
  * Name and format
  * Technical specifications
  * User annotations
  * Performance settings

- **Technical Data**
  * Sample rate/bit depth
  * Channel configuration
  * Duration and size
  * Format details

- **User Data**
  * Tags and categories
  * Notes and comments
  * Custom attributes
  * Usage history

### Session Metadata
- **Project Information**
  * Session configuration
  * Grid layout state
  * Bus routing setup
  * User preferences

- **Performance Data**
  * Gain settings
  * Routing states
  * Group assignments
  * FIFO configurations

### Marker Metadata
- **Grid Marker Data**
-  * Beat positions
-  * Snap points
-  * Fade regions
+  * Cue Marker Data
+    - Position in clip
+    - Up to 4 per clip
+    - Position anywhere in clip
+      * Independent of In/Out points
+      * Supports alternate entry/exit points
+    - Time Division snap state
  * Warp Marker Data
    - Position in clip
    - Tempo relationship
    - Beat detection confidence
    - Time Division alignment
  * Fade Marker Data
    - Fade regions
      * Modify clip in/out amplitude
      * Optional entry/exit treatment
    - Custom region bounds
    - Fade curve type
    - Time Division snap state

### Marker Relationships
  * Sequence Management
    - Order preservation
    - Position validation
    - Conflict detection
  * Time Division Integration
    - Snap point validation
    - Beat alignment status
    - Override tracking

## Platform Integration

### Windows
- **Storage Implementation**
  * File system metadata
  * Alternate streams
  * Search indexing
  * Cache management

- **Performance**
  * Memory mapping
  * Cache optimization
  * Search acceleration
  * Recovery handling

### macOS
- **Storage Implementation**
  * Extended attributes
  * Spotlight integration
  * Search indexing
  * Cache location

- **Performance**
  * Memory mapping
  * Cache management
  * Search optimization
  * Recovery handling

### iOS
- **Storage Implementation**
  * App container
  * iCloud integration
  * Search indexing
  * Cache management

- **Performance**
  * Memory efficiency
  * Cache strategy
  * Search optimization
  * Recovery handling

## Batch Operations
- **Operation Types**
  * Multi-clip updates
  * Property cloning
  * Bulk tagging
  * State synchronization

- **Performance**
  * Parallel processing
  * Progress tracking
  * Error handling
  * State preservation

### Core Metadata
- **Clip Properties**
  * Timing Information
    - Time source configuration
      * Primary source selection
      * Backup source priority
      * Quality thresholds
    - Thread Configuration
      - Core assignment history
      - Priority state changes
      - QoS/MMCSS records
      - Power state transitions
    - Source quality history
      * Source switching events
      * Quality measurements
      * Failure incidents
    - Drift compensation data
      * Measured drift values
      * Correction timestamps
      * Stability metrics
    - Sync point records
      * Resync events
      * Manual overrides
      * Validation results
  * Basic Properties
    - Filename
    - Duration
    - Format type
    - Channel config

## Security Model
- **Format state encryption**
- **Resource map protection**
- **Voice allocation security**

## Security Implementation
- **Timestamp validation per core_security.md**
- **Source integrity verification**
- **Format-state protection**

* Clip Region Data
  - In/Out points
    * Default: Full clip bounds
    * User-definable region
  - Loop State
    * Enable/disable
    * Automatic safety fades
    * Artifact prevention

### Protected Metadata Fields
  * Timing Configuration
   - Time Division State
      * Frame rate settings
      * Musical tempo mapping
      * Time Division correlation
    - Clock Source State
      * Active source
      * Quality metrics
      * Failover history 

### Protected State Fields
  * Format Security
    - Format-native status
    - Resource allocation
    - Time Division state
    - Protection fade config

  * Voice Management
    - Allocation map
    - Resource usage
    - Time Division correlation
    - Format metrics