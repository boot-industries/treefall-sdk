### Preview System
- **Format-Native Processing**
  * Stereo/Mono voice allocation
  * Independent buffer management
  * Zero-impact design
  * Resource monitoring
    - CPU usage tracking
    - Memory utilization
    - Buffer health
    - Auto-scaling thresholds
  * Processing Priority
    - Lowest in system hierarchy
    - Yields before all other systems
    - Independent resource allocation
    - Never impacts main output path
  * Resource Allocation Strategy
    - Preview Voice Pool
      * Separate from main output voices
      * Dynamic allocation/deallocation
      * Format-native processing
      * Zero impact on Clip Groups A-D
    - Processing Features
      * Basic Preview (Always Available)
        - Simple playback engine
        - Format-native path
        - Minimal resource usage
      * Advanced Preview (Resource Dependent)
        - Full Warp: Available < 70% app ceiling / 63% system
        - Neural: Available < 80% app ceiling / 72% system
        - Auto-disables under load
        - State preservation for recovery
  * Performance Requirements
    - Preview trigger: <2ms
    - Format switching: <2ms
    - Zero impact on playback chain
  * Cache Strategy
    - Preview voices in warm cache
    - Background format preparation
    - Resource-aware scaling
  * Warp/Neural Integration
    - Warp Preview
      * Full mode available below 70% app ceiling / 63% system
      * Basic mode above 70% app ceiling / 63% system
      * Always yields to main output warp
    - Neural Preview
      * Available below 80% app ceiling / 72% system
      * Disabled first under load
      * Independent from main neural pool

## Preview System Requirements

### Core Functionality
- **Zero-Impact Preview**
  * Dedicated preview bus
  * Independent voice allocation
  * Format-native processing
  * Grid-aligned playback points

### Preview Contexts
- **Import Dialog**
  * Playback Controls
    - Space bar: Play/Stop
    - Arrow keys: Navigate
    - Mouse click: Set position
    * Grid Behavior
      - Follows global grid settings
      - Shows grid overlay
      - Default 120 BPM (80fps)
      - Respects blackburst mode

- **Edit Panel**
  * Waveform Display
    - Full clip overview
    - Zoom controls
    - Selection markers
    * Grid Integration
      - Inherits clip's detected tempo
      - Shows grid/tempo overlay
      - Visual sync indicators
      - Grid override feedback
  * Transport Controls
    - Sample-accurate positioning
    - Grid-aligned navigation
    - Loop mode support
    - Preview volume 

"Preview Voice Management:",
  - "Resource Isolation":
    * "Preview Voice Pool":
      - "Independent allocation system",
      - "Format-native processing path",
      - "Channel configuration tracking",
      - "Resource impact monitoring"
    * "Performance Firewall":
      - "Zero impact on main output",
      - "Auto-yielding under load",
      - "Format-native isolation" 