# Timing Management System

## Master Clock Architecture

### Source Management
- **Clock Source Configuration**
  * Available Sources:
    - LTC via audio input
    - MTC sources (as peers)
    - PTP/IEEE 1588
    - System clock
    - Blackburst/tri-level sync
  * Per-Source Settings:
    - Priority assignment (user-configurable)
    - Enable/disable state
    - Quality thresholds
    - Failover behavior

### Time Division System
- **Time Division Overrides**
  * Frame Rate Options:
    - Film (24fps/41.67ms)
    - NTSC-DF (29.97fps drop/33.37ms)
    - NTSC (29.97fps non-drop/33.37ms)
    - PAL (30fps/33.33ms)
    - HD (60fps/16.67ms)
    - Blackburst (75fps/13.33ms)

  * Inheritance Model:
    - Per-clip Time Division settings
    - Optional Clip Group override (A-D)
      * When enabled, all member clips obey group settings
    - All derived from Master Clock

  * Tempo Determination (Priority Order)
    1. Global Tempo (when enabled)
       * Warps all clips to match
    2. Per-Clip Settings
       * Auto-detected on import
       * User-assigned override
       * Default: 120 BPM fallback
    3. Clip Group Override (when enabled)
       * Affects all clips in group

### User Interface Integration
- **Group Controls**
  * Frame Rate Selection
    - Dropdown implementation
    - Visual feedback system
    - Conflict indication
  * Override Management
    - Per-clip controls
    - Visual state tracking
    - Conflict resolution UI
  * Performance Requirements
    - Mode switching: <1 frame
    - Visual updates: 60fps
    - Conflict indication: immediate
  * Visual Feedback
    - Source Status
      * Active source indicator
      * Quality metrics
      * Failover status
    - Time Division Status
      * Frame rate display
      * Tempo correlation
      * Sync indicators
    - Emergency Status
      * Protocol phase
      * Recovery progress

### Performance Requirements
- **Critical Timing**
  * Source switching: <1ms
  * Drift compensation: <1ms/hour
  * Jitter: <100μs
  * Musical Mode transitions: <1ms
  * External sync lock: <1 frame
  * Tempo detection: Background only

### Emergency Protocol Integration
- **Critical Response Times**
  * Phase 1 (<1ms): Immediate muting
    - State capture
    - Protection fade application (1ms)
    - Resource freeze
    - Format protection
  * Phase 2 (<2ms): Resource security
    - Voice release
    - Fade state preservation
    - State preservation
    - Resource isolation
  * Phase 3 (<10ms): State preservation
    - System snapshot
    - Format state capture
    - Recovery preparation
  * Phase 4 (<100ms): System recovery
    - State restoration
    - Format recovery
    - Resource reallocation

### Health Monitoring
- **Quality Metrics**
  * Continuous drift detection
  * Real-time jitter analysis
  * Quality scoring
  * Automatic failover

### Time-Aware Processing
  * Format-Native Operations
    - Sample-accurate timing
    - Format state preservation
    - Resource optimization

  * Protection Systems
    - Fade Processing Chain
      * Protection Fades (Highest Priority)
        - 1ms system fades: <1ms compute
        - Edit point fades: <2ms compute
        - Loop point fades: <2ms compute
        - Never yields resources
      * Global Default Fades
        - Applied after protection
        - Computed during import
        - Cached until override
      * Custom User Fades
        - Lowest priority processing
        - Yields under pressure
        - Falls back to global defaults

  * Clock Source Management
    - Source Priority System
      * User-configurable order:
        - LTC via audio input
        - MTC sources (as peers)
        - PTP/IEEE 1588
        - System clock
        - Blackburst/Tri-level sync
      * Quality Monitoring
        - Drift measurement (<100μs warning)
        - Jitter analysis
        - Source health metrics
      * Failover Behavior
        - Immediate switch on failure
        - State preservation during switch
        - Recovery tracking

  * Grid Alignment
    - Time Division Processing
      * Frame-locked operations
      * Musical tempo correlation
      * Format-aware timing

### Health Monitoring
  * Quality Metrics
    - Continuous drift detection
    - Real-time jitter analysis
    - Quality scoring
    - Automatic failover
  * Protection Systems
    - Fade System Monitoring
      * Protection fade verification
      * Global default integrity
      * Custom fade validation
  * Recovery Actions
    * Immediate protection fade fallback
    * Global default restoration
    * Custom fade clearing if corrupt 