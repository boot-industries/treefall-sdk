# System Terminology Guide

## Version: 1.0.0
## Status: Draft
## Last Updated: [Current Date]

## Time Management Terms

### Time Division System
- **Core Concepts**
  * Time Division: Frame-based timing system for clip playback and UI updates
  * Frame Rate: Base timing unit for Time Division system
  * Musical Tempo: BPM-based timing that can be correlated with Time Division
  * Time Division Correlation: Relationship between frame timing and musical tempo
  * Time Division Markers: Visual indicators of frame boundaries and tempo sync points

- **Implementation Details**
  * Derived from Master Clock
  * Unique per clip button
  * Optional Clip Group override
  * Purpose: Frame rate and musical timing management
  * Scope: Clip playback, UI updates, time-based operations
  * Configuration: Per clip with group inheritance
  * Applies to: Both primary and audition players

## State Management Terms

### Core Concepts
- State Preservation: Maintaining current system state during operations
  * Includes: Memory state, audio buffer state, UI state
  * Applies to: All system operations, emergency protocols, resource management
  * Priority: Critical (P0)

- State Restoration: Recovering system state after interruption
  * Includes: System recovery, emergency recovery, user session recovery
  * Applies to: Post-emergency, post-crash, manual recovery scenarios
  * Priority: Critical (P0)

- State Tracking: Monitoring and logging state changes
  * Includes: System events, user actions, resource utilization
  * Applies to: All system operations, debugging, performance monitoring
  * Priority: High (P1)

- Format State Security: Protection of format-specific processing states
  * Includes: Stereo/mono state, resource allocation, processing chain
  * Priority: Critical (P0)
  * Response Time: <0.1ms

- Resource Security Model: Protection of system resource allocation
  * Includes: Voice allocation, memory pools, processing chains
  * Priority: Critical (P0)
  * Response Time: <0.5ms

- Time Division Processing: Frame-based timing and correlation system

## Audio Processing Terms

### Core Concepts
- Format-Native Processing: Direct processing in original audio format without conversion
  * Applies to: All audio operations except cross-format operations
  * Performance Impact: Minimal
  * Resource Priority: Maximum

- Zero-Impact Processing: Processing that doesn't affect system performance
  * Applies to: Background operations, preview systems
  * Resource Allocation: Dedicated
  * Priority: Dynamic based on system load

- Zero-Impact Preview: Real-time preview system with dedicated resources
  * Applies to: Audition system, pre-listen functions
  * Resource Allocation: Independent
  * Priority: User-interactive

## Resource Management Terms

### System States
- Warning State (80%)
  * Trigger Point: 80% resource utilization
  * Required Actions: 
    - Scale back allocation
    - User notification
    - Format-specific optimizations
  * Priority: High (P1)

- Critical State (90%)
  * Trigger Point: 90% resource utilization
  * Required Actions:
    - Emergency protocol activation
    - Format-priority actions
    - Immediate resource recovery
  * Priority: Critical (P0)

- Recovery State (70%)
  * Trigger Point: Below 70% resource utilization
  * Required Actions:
    - Resume normal operation
    - Resource reallocation
    - State restoration
  * Priority: High (P1)

### Voice Allocation
- **Core Concepts**
  * Voice Pool: Total available playback voices
  * Voice Assignment: Allocation of voices to clips
  * Voice Protection: Resource security for critical voices
  * Time Division Voice Management: Frame-based voice allocation
  * Voice Recovery: Reallocation after emergency protocols

### Resource Protection
- **Core Concepts**
  * Format Protection: Maintaining format-native processing
  * State Security: Preservation of critical system states
  * Time Division Protection: Frame-timing state security
  * Voice Security: Protected voice allocation states

## Cross-Reference Requirements
All documentation referencing these terms must:
1. Use exact terminology as defined here
2. Link to this document for term definitions
3. Maintain consistent priority levels
4. Follow state transition protocols

## Change Management
Changes to this terminology require:
1. Technical review
2. Impact assessment
3. Documentation update plan
4. Migration strategy

# Standard Terminology

## Interface Components

### Time Division Interface Elements
- **Frame Rate Display**: Shows current Time Division frame rate
  * Primary display in transport section
  * Per-clip indicator in clip properties
  * Group override status when active

- **Tempo Correlation Display**: Shows musical timing relationship
  * BPM display with confidence metric
  * Time Division alignment status
  * Sync point visualization

- **Time Division Controls**
  * Frame Rate Selector: Primary rate control
  * Tempo Override: Musical timing control
  * Sync Point Editor: Correlation markers
  * Group Assignment: Inheritance controls

- **Visual Feedback Elements**
  * Frame Markers: Time Division boundaries
  * Tempo Indicators: Musical beat/bar markers
  * Correlation Display: Sync relationship
  * Status Indicators: Health/accuracy metrics

### Primary Interface Elements
- **Tab**: One of up to 8 configurable workspaces in a session
- **Clip Grid**: The configurable button matrix within each Tab
  * Maximum dimensions: 12 rows x 16 columns
  * Independently configurable per Tab

### View Modes
- **Single Page Mode**: Full-pane display of one Tab
- **Dual Page Mode**: Side-by-side display of two Tabs
  * Optional: "Half" columns preference
  * Maintains clip button order during reduction

### Legacy Terms (Deprecated)
- "Matrix" (use "clip grid" instead)
- "Grid" alone (specify "clip grid" or "time division" as appropriate)
- "Snap Resolution" (use "time division" instead)
- "Grid Analysis" (use "Time Division Analysis" instead)
- "Grid Processing" (use "Time Division Processing" instead)
- "Grid Alignment" (use "Time Division Alignment" instead)

### Term Usage Clarification
- "Matrix": Use "Clip Grid" when referring to the clip button layout. 
  However, "Matrix" remains the correct term for the Routing Matrix.
- "Grid": Always specify either "Clip Grid" or "Time Division" to avoid ambiguity
- "Snap Resolution": Use "Time Division" instead
- "Time Division": Frame-based timing system
  * Derived from Master Clock
  * Manages frame rates and musical tempo correlation
  * Provides timing for clip playback and UI updates
  * Configurable per clip or clip group
  * Includes:
    - Frame rate management
    - Musical tempo correlation
    - Sync point handling
    - Format-aware timing

### Important Note
While "Page" is deprecated when referring to Tabs themselves, it remains the correct term when describing view modes (Single Page Mode and Dual Page Mode).