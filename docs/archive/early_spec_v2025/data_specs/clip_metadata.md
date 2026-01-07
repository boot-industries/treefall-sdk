# Clip Metadata Specification

## Core Metadata Structure

### Performance Data
- **Playback Properties**
  * Start/End points
  * Loop points
  * Gain settings
  * Pan position (mono sources only)
  * Group assignment
    - Clip Group (A-D)
    - Choke behavior
      * Enable/disable
      * Interrupt mode:
        - Immediate start of new clip
        - Force-fade existing clips in group
        - Honors user fade settings
  * Location data
    - Page index
    - Tab index
    - Clip Grid position
  * Marker order
    - Cue marker sequence
    - Warp marker sequence

- **Processing State**
  * Format details
  * Sample rate
  * Channel config
  * Processing chain state

### Technical Metadata
- **Audio Properties**
  * Format Configuration
    - Format-native state
    - Bit depth
    - Sample rate
    - Duration
    - File size
    - Format-specific state
      * Stereo: 1 stereo voice
      * Mono: 1 mono voice + pan
    - Resource requirements
      * Voice allocation
      * Memory footprint
      * Processing overhead

  * Time/Pitch State
    - Time stretch value
    - Range: -50% to +100%
    - Default: 0%
    - Pitch shift value
    - Range: -12 to +12 semitones
    - Default: 0
    - Pitch lock state
    - Tempo sync state
      * Original tempo
      * Current stretch/pitch

  * Markers
    - Cue markers (max 4)
    - Warp markers (max 4)
    * Purpose flags:
      - Tempo anchor (no warping)
      - Section boundary
      - Transition point
      - Style change
    - Time Division Configuration
      * Frame Rate Override
        - Selected rate (if any)
        - Parent group reference
        - Override timestamp
      * Time Division Settings
        - Time division snap points
        - Sync reference markers
        - Format-specific timing
      * Performance Data
        - Time Division alignment accuracy
        - Sync stability metrics
        - Recovery history

  * Analysis Results
    - Content Classification
      * Rhythmic/Non-rhythmic sections
      * Energy level mapping
      * Transition points
      * Style markers

### Audio Processing State
  * Format State Security
    - Protection Fades
      * Edge fades: {
        status: boolean,
        timestamp: number,
        verified: boolean
      }
      * Edit points: {
        duration: number,
        adaptive_state: string,
        analysis: {
          transients: boolean,
          beats: boolean,
          spectral: boolean,
          zero_cross: boolean
        },
        verification: {
          timestamp: number,
          status: boolean
        }
      }
      * Loop points: {/* same structure as edit points */}
    - User Fades
      * IN fade: {
        active: boolean,
        duration: number,
        curve_type: "log"|"lin"|"exp"|"s-curve",
        power_comp: boolean,
        timestamp: number
      }
      * OUT fade: {/* same as IN fade */}
      * Crossfade: {/* same minus power_comp */}

- **Edit Properties**
  * Protection Fades (System Level)
    * Clip edges: 1ms fixed fade (non-configurable)
    * Edit points: 0.25-10ms adaptive fade
      - Analysis based on:
        * Transient detection
        * Beat position
        * Spectral content
        * Zero-crossing optimization
    * Loop points: 0.25-10ms adaptive fade
  * User Fade Settings
    * IN/OUT fades: 0-3s range (0.1s steps)
    * Crossfades: 0-3s range (0.1s steps)
    * Curve types:
      - Logarithmic (default)
      - Linear
      - Exponential
      - S-Curve
    * Power compensation toggle
  * Zero-crossing status

- **Source Information**
  * Original filename
  * Creation date
  * Modification date
  * Source path
  * Hash verification

### User Metadata
- **Organization**
  * Custom labels
  * Color coding
  * Categories/tags
  * Notes/comments
  * Rating/flags

- **Usage Data**
  * Play count
  * Last used
  * Error history
  * Performance notes

## Database Implementation

### Storage Format
- **Primary Store**
  * SQLite tables
  * JSON for complex data
  * Binary for performance data
  * Cache for quick access

### Performance Requirements
- **Access Times**
  * Read: <5ms
  * Write: <10ms
  * Search: <50ms
  * Batch operations: optimized

## Platform Integration

### Windows
- **Storage**
  * NTFS streams
  * Shell properties
  * Search indexing
  * Cache location

### macOS
- **Storage**
  * Extended attributes
  * Spotlight integration
  * Quick Look data
  * Cache management

### iOS
- **Storage**
  * App container
  * iCloud sync
  * Search indexing
  * Cache policy

## Relationships
- **Parent sessions**
- **Related clips**
- **Group membership**
- **Marker sequences**

### Technical Metadata
- **Usage Data**
  * Playback History
    - Play timestamps
    - Duration logs
    - Error events
  * File Management
    - Original path
    - Current path
    - Repository status
    - Recovery history

### Metadata Fields
For complete database schema details, see `session_structure.md`

### Technical Data
- **Format Properties**
  * Channel Configuration
    - Format type (stereo/mono)
    - Original channel layout
    - Processing requirements
      * Voice allocation needs
      * Resource scaling preferences
    - Bus routing configuration
      * Default group assignment
      * Format-specific routing 

## Import/Export Metadata Handling

### Mode-Specific Metadata
1. Full Session
   - Complete metadata preservation
   - Resource allocation state
   - Clip Grid position data
   
2. Metadata-only
   - Property definitions
   - Update rules
   - Validation requirements
   
3. Tab Export
   - Tab-specific metadata
   - Position preservation
   - Resource state
   
4. Clip List
   - Selection order preservation
   - Partial metadata options
   - Batch update rules 

### Performance Rights Metadata
- **Rights Management**
  * Mandatory Fields
    - Composer information
    - Publisher details
    - ISRC codes
    - Rights holder data
    - Territory restrictions
  * Optional Fields
    - PRO affiliations
    - Cue sheet categories
    - Usage restrictions
    - Reporting preferences

- **Performance Tracking**
  * Playback History
    - Timestamp logging
    - Duration tracking
    - Context recording
    - Modification tracking
  * Export Options
    - Cue sheet formats
    - PRO-specific reports
    - Custom report templates
    - Filtering preferences 

### Cue Sheet Metadata
- **Performance Context**
  * Show Information
    - Production name/ID
    - Performance date
    - Venue/location
  * Usage Details
    - Scene description
    - Usage type/category
    - Territory information
  * Manual Annotations
    - Production notes
    - Special instructions
    - Rights exceptions 

### Emergency Protocol State
  * Phase Data
    - Phase 1 state (<1ms)
      * Muting status
      * Voice deallocation state
      * Emergency notification flags
    - Phase 2 state (<2ms)
      * Format preservation data
      * Resource protection state
      * Bus configuration snapshot
    - Phase 3 state (<10ms)
      * Complete state snapshot
      * Resource state capture
      * Recovery preparation flags
    - Phase 4 state (<100ms)
      * Recovery validation data
      * Resource reallocation state
      * Format restoration flags

### Thread Management Data
  * Thread States
    - Audio thread priority
    - UI thread state
    - Resource thread status
    - Background operations
  * Lock States
    - Critical state locks
    - Resource state locks
    - UI state locks

### Format Processing State
- **Format Configuration**
  * Stereo State
    - Channel Configuration
      * Native stereo processing flag
      * Channel width setting
      * Balance position
      * Resource allocation ID
    - Processing Chain
      * Chain identifier
      * DSP state snapshot
      * Resource requirements
      * Emergency protocol priority
    - Resource Impact
      * Voice allocation: One stereo voice
      * Memory footprint
      * Cache requirements
      * Processing overhead

  * Mono State
    - Channel Configuration
      * Native mono processing flag
      * Pan position (-100 to +100)
      * Expansion setting
      * Resource allocation ID
    - Processing Chain
      * Chain identifier
      * Pan law configuration (-4.5dB center)
      * Resource requirements
      * Emergency protocol priority
    - Resource Impact
      * Voice allocation: One mono voice
      * Memory footprint
      * Cache requirements
      * Processing overhead

### Resource Management Metadata
- **Allocation State**
  * Voice Assignment
    - "Processing Configuration:"
      * "Channel configuration status"
      * "Format-native processing state"
      * "Resource optimization flags"
      * "Emergency protocol priority"
  * Resource Thresholds
    - "Warning trigger (80% app ceiling)"
    - "Critical trigger (90% app ceiling)"
    - "Recovery point (70% app ceiling)"
    - "FIFO allocation status"
    - "Yield priority timestamp"
    - "Format-specific actions"

- **Emergency Protocol Data**
  * State Preservation
    - Format state snapshot
    - Processing chain capture
    - Resource allocation map
    - Recovery checkpoints
  * Recovery Strategy
    - Format restoration path
    - Voice reallocation plan
    - Processing chain rebuild
    - Resource rebalancing 

### File Location Tracking
- **Path History**
  * Original import path
  * Current active path
  * Previous valid locations
  * Network share mappings

- **Search Metadata**
  * File fingerprint
  * Audio characteristics
  * Creation timestamps
  * Platform-specific identifiers 

### Processing States
  * Warp Configuration
    * Current Mode:
      - Full Warp (when < 70% CPU)
      * Complex algorithm state
      * Musical analysis data
      * Phase coherence settings
      - Basic Warp (when > 70% CPU)
      * Simple algorithm parameters
      * Grid-lock state
      * Transient markers
    * Fallback Settings:
      - Preserved full warp state
      - Basic warp parameters
      - Mode transition points
    * Subject to 90% application resource ceiling
    * FIFO allocation priority timestamp
    * Yield priority (newest clips yield first)

  * Neural Processing
    * Current State:
      - Active (when < 80% CPU)
        * Model parameters
        * Processing chain state
        * Resource allocation
      - Suspended (when > 80% CPU)
        * Preserved model state
        * Fallback parameters
        * Recovery points
    * Resource Tags:
      - CPU allocation
      - Memory footprint
      - Priority level 

"Tempo Analysis Data:"
  * "Tempo Information:"
    - "Primary Detection:"
      * "Detected BPM value"
      * "Confidence score (0-100)"
      * "Analysis method used"
      * "Detection timestamp"
    - "Alternative Tempos:"
      * "Ordered by confidence"
      * "Related musical ratios"
      * "Source algorithms"
    - "Section Analysis:"
      * "Tempo boundaries"
      * "Transition points"
      * "Stability metrics"

  * "Time Division Alignment:"
    - "Default Settings:"
      * "Frame rate: 80fps"
      * "Musical tempo: 120 BPM"
      * "Sync state"
    - "Override Data:"
      * "Manual BPM value"
      * "Override timestamp"
      * "User preference flags"

"Format Processing State":
  - "Channel Configuration":
    * "Channel configuration status",
    * "Format-native processing flags",
    * "Resource optimization state",
    * "Processing chain identifiers"
  - "Resource Impact":
    * "System capability factors",
    * "Channel configuration impact",
    * "Memory footprint",
    * "Cache requirements" 

"Processing State:",
  * "Format Configuration:",
  - "Security Requirements:",
    * "Format state protection",
    * "Resource allocation security",
    * "Emergency protocol compliance",
  - "Channel Configuration:",
    * "Native stereo processing flag" 

"Resource Impact":
  * Voice allocation tracking
    - Launch timestamp for FIFO priority
    - Yield status (will yield to older clips)
    - Protection status (audition bus exempt from FIFO)
  * Voice allocation cost
  * Memory footprint
  * Cache requirements
  * Processing overhead 

### Resource Requirements
- Voice Allocation:
  * Choke mode: max 3 voices per Clip Group, mono or stereo
  * Reserved allocation ensures consistent performance
  * Format-native processing 

### Timing Metadata
- **Clock Subscription Data**
  * Source Configuration
    - Primary source selection
    - Backup source priority
    - Quality thresholds
    - Validation requirements
  * Master Clock Integration
    - Global time reference
    - Sample-accurate sync
    - Drift compensation data
  * Time Division Settings
    - Parent Group Settings
      * Selected frame rate/tempo
      * Time Division visualization
    - Clip-Specific Override
      * Manual tempo setting
      * Override timestamp
      * Visual feedback state
  * Performance Metrics
    - Source switching history
    - Drift measurements
    - Jitter analysis
    - Quality scores

### Time Division Integration
- **Time-Aware Analysis**
  * Master Clock Correlation
    - Time division alignment
    - Beat detection metrics
    - Sync point markers
    - Format-specific timing
  * Quality Metrics
    - Alignment confidence
    - Drift history
    - Recovery incidents
    - Performance scoring

### Tempo Analysis
- **Timing Analysis**
  * Tempo Detection
    - Master Clock reference
    - Beat detection confidence
    - Time division alignment quality
    - Format-specific metrics
  * Sync Points
    - Time source validation
    - Time division correlation
    - Drift history
    - Quality scoring 

### Technical Metadata
  * Processing State
   - Fade Configuration
     * User Fade Settings
       - IN fade: {duration: null|value, curve_type, power_comp}
       - OUT fade: {duration: null|value, curve_type, power_comp}
       - Crossfade: {duration: null|value, curve_type}
       - Global defaults: {inherited: true|false}
     * Protection Fade State
       - Edge fades: Applied/Missing
       - Edit point fades: {duration, adaptive_state}
       - Loop point fades: {
          duration: number,
          adaptive_state: string,
          analysis: {
            time_division_align: boolean,
            spectral_cont: boolean,
            phase_corr: boolean
          }
        } 

### Audio Processing State
  * Format Configuration
    - Native format status
    - Channel configuration
    - Resource requirements
    * Fade Configuration
      - Protection Fades
        * Edge fades: {status: boolean, timestamp: number}
        * Edit points: {
          duration: number,
          adaptive_state: string,
          analysis: {
            transients: boolean,
            beats: boolean,
            spectral: boolean,
            zero_cross: boolean
          }
        }
        * Loop points: {
          duration: number,
          adaptive_state: string,
          analysis: {
            time_division_align: boolean,
            spectral_cont: boolean,
            phase_corr: boolean
          }
        }
      - User Fades
        * IN fade: {
          active: boolean,
          duration: number,
          curve_type: string,
          power_comp: boolean,
          timestamp: number
        }
        * OUT fade: {/* same structure as IN fade */}
        * Crossfade: {/* same structure minus power_comp */} 

### Clip Region Data
  * Region Properties
    - Start/End points
    - IN/OUT points
    - Loop points
    - Time Division position
    - Format markers 