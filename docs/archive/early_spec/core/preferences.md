# Preferences System

## Interface Layout
- **Lower Panel Tab Implementation**
  * Preferences tab alongside:
    - Edit panel
    - Routing matrix
    - Labelling system
  * Persistent across sessions
  * Immediate application of changes
  * Visual feedback for pending changes

## Application Preferences
- **Audio System**
  * Driver Selection
    - ASIO/CoreAudio/AVAudioEngine
    - Buffer size configuration
    - Sample rate settings
    - Format preferences
  * Voice Management
    - Maximum voice limits
    - Resource warning thresholds
    - Auto-scaling behavior
  * Monitoring Setup
    - Default output routing
    - Audition bus configuration
    - Metering preferences

- **Clock Source Management**
  * Source Configuration
    - Available sources list
    - Priority assignment
    - Quality thresholds
    - Failover behavior
  * Monitoring Setup
    - Quality metric display
    - Warning thresholds
    - Logging preferences

- **Interface Defaults**
  * View Modes
    - Default to Single/Dual page
    - Column reduction in Dual page
    - Default grid dimensions
  * Visual Feedback
    - Meter ballistics
    - Peak hold times
    - Warning colors
    - Status indicators
  * Emergency Behavior
    - Visual warning levels
    - Auto-recovery options
    - Notification settings

- **Default Processing**
  * Fade Shapes
    - Default fade curves
      * IN fade shape: 
        - Type: Logarithmic (default)
        - Options: Log/Lin/Exp/S-Curve
        - Power compensation: On
      * OUT fade shape:
        - Type: Exponential (default)
        - Options: Log/Lin/Exp/S-Curve
        - Power compensation: On
      * Crossfade shape:
        - Type: Equal-power S-Curve (default)
        - Options: Log/Lin/Exp/S-Curve
        - Power compensation: Always on
    - Protection fade shapes
      * Edit point shape:
        - Type: Linear (fixed)
        - Duration: 0.25ms-10ms adaptive
      * Loop point shape:
        - Type: Equal-power (fixed)
        - Duration: 0.25ms-10ms adaptive
  * Default Durations
    - Global Default Fades
      * Factory defaults:
        - IN fade: 0.0s (uses 1ms safety fade)
        - OUT fade: 0.3s exponential
        - Crossfade: 0.1s equal-power
      * User adjustable: 0-3.0s in 0.1s steps
    - System Protection Fades
      * Clip edges: 1ms fixed
      * Edit points: 0.25ms-10ms adaptive
      * Loop points: 0.25ms-10ms adaptive

## Session Preferences
- **Clip Group Settings**
  * Default Time Division
    - Frame rates per group
    - Musical tempo options
    - Grid snap behavior
  * Choke Behavior
    - Default group assignment
    - Interrupt modes
    - Fade behavior
  * Resource Management
    - Voice allocation strategy
    - Format-specific settings
    - Processing priorities

- **Group Processing**
  * Filter Configuration
    - High-pass filter
      * Default frequency: 20Hz
      * Default slope: 12dB/oct
      * Q factor: 0.707
    - Low-pass filter
      * Default frequency: 20kHz
      * Default slope: 12dB/oct
      * Q factor: 0.707
  * Dynamic Processing
    - Limiter threshold: -1.0dB
    - Release time: 50ms
    - Lookahead: 1ms
  * Output Processing
    - Mono compatibility check
    - Phase correlation meter
    - Output limiter settings

- **Playback Defaults**
  * Protection Fades
    - Default fade times
    - Curve types
    - Power compensation
  * Loop Behavior
    - Default loop state
    - Crossfade settings
    - Grid alignment
  * Preview System
    - Audition routing
    - Waveform display
    - Scrub behavior

- **Analysis Settings**
  * Tempo Detection
    - Confidence thresholds
    - Additional pass triggers
    - Section analysis sensitivity
  * Grid Analysis
    - Default grid strength
    - Beat detection sensitivity
    - Override behavior
  * Neural Processing
    - Feature detection level
    - Processing priority
    - Resource limits

- **Recording Configuration**
  * Input Setup
    - Default source routing
    - Format settings
    - Channel configuration
  * Auto-naming
    - Pattern templates
    - Counter behavior
    - Timestamp options
  * Background Recording
    - Buffer duration
    - Pre-roll settings
    - Auto-save behavior

## Security Settings
- **Access Control**
  * Feature Lock
    - Critical system functions
    - Configuration changes
    - Emergency controls
  * External Control
    - Remote access limits
    - Protocol restrictions
    - Authentication requirements

## Auto-Save Configuration
- **Session Recovery**
  * Backup Frequency
  * Version retention
  * Recovery points
  * Emergency saves

## Performance Monitoring
- **Resource Display**
  * CPU Usage
    - Application ceiling display
    - System percentage conversion
    - Warning threshold indicators
  * Memory Tracking
  * Voice Allocation
  * Format Status 