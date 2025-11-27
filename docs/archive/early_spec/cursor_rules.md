{
  "metadata": {
    "purpose": "Global Development Standards and Cross-Cutting Requirements",
    "version": "1.1",
    "related_rules": [
      "rules/core/*",
      "rules/implementation/*",
      "rules/platform/*",
      "rules/development/*"
    ]
  },

  "rules": [
    "Global Performance Requirements:",
      - "Critical Timing Chain:",
        * "Real-time operations must meet specified latency targets",
        * "Emergency responses must be deterministic",
        * "UI responsiveness must maintain human-perceivable thresholds",
        * "Resource allocation: <1ms",
        * "State preservation: <5ms",

      - "Resource Thresholds:",
        * "Systems must define clear resource boundaries",
        * "Recovery points must be below warning thresholds",
        * "Resource allocation limits:",
          - "Format-native requirements",
          - "Dynamic allocation rules",

    "Global Architecture Requirements:",
      - "Core Audio Engine:",
        * "Transport Requirements:",
          - "Must implement standard transport interface:",
            * "Play/stop/pause with sample accuracy",
            * "Seek with format-appropriate precision",
            * "Time source management and drift control",
          - "Must provide consistent timing model:",
            * "Sample position to time mapping",
            * "Format-aware timing calculations",
            * "Drift compensation mechanisms"

        * "All processing must be format-native",
        * "All conversions must be explicit",
        * "All resource allocation must be format-aware",
        * "All state preservation must maintain format integrity",

      - "Resource Hierarchy:",
        * "Processing Chain Requirements:",
          - "Core functionality must be inviolable",
          - "Resource allocation must be hierarchical",
          - "System degradation must be graceful",
          - "Module interfaces must be clearly defined",
        * "Input/Recording:",
          * "Must define clear failure modes",
          * "Must protect primary functionality",
          * "Must maintain data integrity",
        * "Optional Processing:",
          * "Must define clear resource boundaries",
          * "Must implement graceful degradation",
          * "Must preserve core functionality",
        * "Resource Protection:",
          - "Format-native guarantee",
          - "Voice allocation protection",
          - "Buffer management isolation",
          - "Emergency protocol integration"

      - "Thread Management:",
        * "Audio thread must be real-time priority",
        * "UI thread must never block audio",
        * "Resource operations must be async",
        * "State operations must be atomic",
        * "Processing Priority must be enforced:",
          - "Critical paths must be protected",
          - "Optional features must yield gracefully",
          - "Resource thresholds must be respected",
          - "System stability must be maintained"

      - "State Management:",
        * "State Integrity:",
          - "Must define clear state boundaries",
          - "Must maintain transactional consistency",
          - "Must preserve system stability",
        * "Recovery Requirements:",
          - "Must define recovery paths",
          - "Must maintain data consistency",
          - "Must protect against corruption",
        * "Operation Safety:",
          - "Must define operation boundaries",
          - "Must implement safe rollback",
          - "Must preserve system state"

    "Global Error Handling:",
      - "Error Hierarchy:",
        * "Critical Failures:",
          - "Must define system-critical operations",
          - "Must implement immediate responses",
          - "Must preserve system integrity",
        * "Major Issues:",
          - "Must define acceptable degradation",
          - "Must implement graceful fallbacks",
          - "Must maintain core functionality",
        * "Minor Issues:",
          - "Must not impact critical operations",
          - "Must implement self-recovery",
          - "Must maintain system stability"

      - "Recovery Requirements:",
        * "Recovery Strategy:",
          - "Must define clear recovery paths",
          - "Must maintain system consistency",
          - "Must provide appropriate feedback",

      - "Error Implementation:",
        * "Emergency Protocol Phases:",
          - "Phase 1: <1ms (immediate muting)",
          - "Phase 2: <2ms (resource protection)",
          - "Phase 3: <10ms (state preservation)",
          - "Phase 4: <100ms (recovery)",
        * "All UI components must use error boundaries",
        * "All errors must follow standard logging format",
        * "All error messages must be user-friendly",
        * "All errors must include debugging context",
        * "All critical errors must trigger monitoring alerts",

    "Global Resource Management:",
      - "Resource Allocation:",
        * "Global Resource Ceiling":
          - "Warning threshold: 80% app ceiling (72% system)",
          - "Critical threshold: 90% app ceiling (81% system)",
          - "Recovery threshold: 70% app ceiling (63% system)",
          - "All internal metrics relative to ceiling",
          - "UI/logging shows application percentages"
        * "All allocations must be tracked",
        * "All resources must be reclaimable",
        * "All resources must be format-tagged",
        * "All resources must be priority-ranked",

      - "Resource Protection:",
        * "All critical resources must be reserved",
        * "All shared resources must be locked",
        * "All resource states must be preserved",
        * "All resource limits must be enforced",

    "Global Platform Requirements:",
      - "Platform Integration:",
        * "All audio paths must be native:",
          - "Windows: ASIO/WASAPI - See PLATFORM_SPEC.md, windows.md",
          - "macOS: CoreAudio - See PLATFORM_SPEC.md, macos.md",
          - "iOS: AVAudioEngine - See PLATFORM_SPEC.md, ios.md",
        * "All graphics must be native:",
          - "Windows: DirectX with hardware acceleration",
          - "macOS/iOS: Metal with Neural Engine support",
        * "All file operations must be native",
        * "All security must be platform-appropriate",

      - "Accessibility Requirements:",
        * "All interfaces must meet WCAG 2.1 AA standards",
        * "All interactive elements must support keyboard navigation",
        * "All content must have proper ARIA attributes",
        * "All media must have appropriate alternatives",
        * "All color combinations must meet contrast requirements",

    "Global Development Standards:",
      - "Code Quality:",
        * "All code must be format-aware",
        * "All operations must be resource-tracked",
        * "All states must be preserved",
        * "All errors must be handled",

      - "Testing Requirements:",
        * "All timing requirements must be verified",
        * "All resource limits must be tested",
        * "All error paths must be exercised",
        * "All formats must be validated",

      - "Documentation Requirements:",
        * "All APIs must be documented",
        * "All formats must be specified",
        * "All resource usage must be declared",
        * "All error handling must be detailed",
        * "All components must include usage examples",
        * "All documentation must follow JSDoc standards",
        * "All UI components must have Storybook entries",
        * "All version changes must be documented with metadata",

      - "Version Control Standards:",
        * "Commit messages must follow conventional commits format",
        * "Branch names must follow project naming convention",
        * "All PRs must include documentation updates",
        * "All commits must be signed",

      - "Code Organization:",
        * "Imports must be grouped (external/internal/shared)",
        * "Files must follow established directory structure",
        * "Component architecture must follow atomic design",
        * "Utilities must be centralized and shared",

      - "Design System Requirements:",
        * "All UI components must use design tokens",
        * "All styling must follow brand guidelines",
        * "All components must be responsive by default",
        * "All animations must follow motion guidelines",
        * "All visual elements must maintain brand consistency",

    "Global Security Requirements:",
      - "Data Protection:",
        * "All user data must be protected",
        * "All network traffic must be encrypted",
        * "All credentials must be secured",
        * "All logs must be sanitized",

      - "Access Control:",
        * "All operations must be authenticated",
        * "All resources must be protected",
        * "All interfaces must be secured",
        * "All exports must be validated"

    "Global Architecture Requirements:",
      - "Audio Processing Core:",
        * "Transport Interface:",
          - "Must provide standard transport primitives:",
            * "Sample-accurate playback control",
            * "Format-aware seeking",
            * "Drift management",
            * "Time source abstraction"

      - "Resource Management Core:",
        * "Voice Allocation:",
          - "Must implement standard voice interface:",
            * "Format-native processing",
            * "Priority-based allocation",
            * "Resource monitoring",
            * "Graceful degradation"

      - "Preview System Core:",
        * "Audition Interface:",
          - "Must implement zero-impact preview:",
            * "Independent resource pool",
            * "Format-native processing",
            * "Auto-yielding under load",
            * "State preservation"

      - "Basic DSP Toolkit:",
        * "Standard Processors:",
          - "Must provide consistent interfaces for:",
            * "Level control (with interpolation)",
            * "Basic filters (with smoothing)",
            * "Fade curves (click-free)",
            * "Format conversion"

      - "Intelligent Audio Processing:",
        * "Adaptive Fade System:",
          - "Must implement context-aware processing:",
            * "Content analysis (transients, spectrum)",
            * "Adaptive curve selection",
            * "Zero-crossing optimization",
            * "Phase coherence preservation"
          - "Must provide standard interfaces:",
            * "Fixed protection fades",
            * "Adaptive edit point fades",
            * "Loop point optimization",
            * "Crossfade generation"

        * "Analysis Framework:",
          - "Must provide standard analyzers:",
            * "Transient detection",
            * "Spectral content",
            * "Rhythm detection",
            * "Pitch content"
          - "Must support composition:",
            * "Chainable analysis",
            * "Resource-aware processing",
            * "Format-native operation"

        * "Musical Processing:",
          - "Must implement tempo framework:",
            * "Grid analysis/detection",
            * "Beat matching utilities",
            * "Tempo map generation",
            * "Sync point management"

      - "Standard DSP Modules:",
        * "Time/Pitch Processing:",
          - "Must provide consistent algorithm interface",
          - "Must define quality/resource trade-offs",
          - "Must implement standard parameter mapping"
        * "Filter Implementation:",
          - "Must follow standard filter design patterns",
          - "Must provide consistent parameter interface",
          - "Must define CPU usage boundaries"
        * "Fade Processing:",
          - "Must implement standard curve types",
          - "Must provide consistent envelope control",
          - "Must guarantee click-free operation"

      - "Media Management:",
        * "Import System:",
          - "Must implement standard browser interface",
          - "Must provide consistent metadata extraction",
          - "Must support standard preview mechanisms"
        * "Metadata Framework:",
          - "Must implement standard tag structure",
          - "Must provide consistent search interface",
          - "Must maintain format-native metadata"

      - "Format Handling:",
        * "All processing must be format-native",
        * "All conversions must be explicit",
        * "All resource allocation must be format-aware",
        * "All state preservation must maintain format integrity",

      - "Audio Edit Framework:",
        * "Core Edit Operations:",
          - "Must implement standard edit interface:",
            * "Non-destructive editing paradigm",
            * "Format-aware edit operations",
            * "Resource-conscious processing",
            * "State preservation guarantees"
          - "Must support extensible edit types:",
            * "Basic operations (cut/copy/paste)",
            * "Fade processing (standard curves)",
            * "Time operations (stretch/compress)",
            * "Future operation types (expandable)"

        * "Edit Preview System:",
          - "Must implement standard preview interface:",
            * "Zero-impact audition chain",
            * "Format-native preview processing",
            * "Resource-aware degradation",
            * "Independent buffer management"
          - "Must support multiple preview modes:",
            * "Quick preview (minimal resources)",
            * "Full preview (when resources available)",
            * "Hybrid modes (resource dependent)"

        * "Edit State Management:",
          - "Must maintain edit history:",
            * "Non-destructive state tracking",
            * "Resource-efficient storage",
            * "Format-aware state preservation"
          - "Must support standard undo/redo:",
            * "Operation-based state management",
            * "Resource-conscious history depth",
            * "Format-native state restoration"

      - "Standard Logging Framework:",
        * "Event Logging Interface:",
          - "Must implement hierarchical logging:",
            * "System events (core operations)",
            * "User events (interactions)",
            * "Resource events (allocation/deallocation)",
            * "Performance events (measurements)"
          - "Must support multiple contexts:",
            * "Development debugging",
            * "Production monitoring",
            * "Performance analysis",
            * "User behavior tracking"

      - "UI/UX Framework:",
        * "Interface Components:",
          - "Must implement standard control types:",
            * "Transport controls (consistent behavior)",
            * "Timeline interfaces (standard markers)",
            * "Level controls (standard scaling)",
            * "Format-aware displays"
          - "Must support extensible layouts:",
            * "Grid-based arrangements",
            * "Responsive scaling",
            * "Format-specific views"

        * "Interaction Patterns:",
          - "Must implement standard behaviors:",
            * "Preview mechanisms",
            * "Edit operations",
            * "Navigation models",
            * "Selection paradigms"
          - "Must maintain consistency:",
            * "Visual feedback",
            * "Error handling",
            * "State indication"

      - "State Container Framework:",
        * "Core Requirements:",
          - "Must implement transactional interface:",
            * "Resource tracking",
            * "State preservation",
            * "Recovery paths",
            * "Version management"

      - "Session Management Framework:",
        * "State Container Requirements:",
          - "Must implement standard session interface:",
            * "Resource tracking and allocation",
            * "Format-native state preservation",
            * "Hierarchical data organization",
            * "Extensible metadata support"
          - "Must support multiple contexts:",
            * "Development environment",
            * "Production deployment",
            * "Recovery scenarios"

        * "Save/Load Operations:",
          - "Must implement standard persistence:",
            * "Atomic save operations",
            * "Incremental state updates",
            * "Format-aware serialization",
            * "Resource-conscious storage"

        * "Auto-save System:",
          - "Must implement reliable backup:",
            * "Configurable intervals",
            * "Resource-aware triggers",
            * "Emergency state capture"
          - "Must preserve system stability:",
            * "Background processing",
            * "Non-blocking operations",
            * "Format-native storage"

      - "Musical Grid Framework:",
        * "Grid Analysis System:",
          - "Must provide tempo detection:",
            * "Multi-algorithm analysis",
            * "Confidence scoring",
            * "Alternative tempo suggestions",
            * "Musical boundary detection"
          - "Must support flexible mapping:",
            * "Default grid (80fps/120 BPM)",
            * "Detected tempo mapping",
            * "Manual override capability",
            * "Blackburst sync modes"

        * "Grid Visualization:",
          - "Must implement standard markers:",
            * "Beat divisions",
            * "Musical subdivisions",
            * "Sync points",
            * "Override indicators"
          - "Must support composition:",
            * "Multiple grid layers",
            * "Format-aware rendering",
            * "Resource-conscious drawing"

        * "Grid Integration:",
          - "Must provide standard interfaces:",
            * "Transport synchronization",
            * "Edit point snapping",
            * "Playback quantization",
            * "Visual feedback"

      - "Voice Allocation Framework:",
        * "Resource Management Core:",
          - "Must implement dynamic voice pool:",
            * "System-aware allocation",
            * "Format-native processing",
            * "Resource monitoring",
            * "Guaranteed minimums"
          - "Must define scaling strategy:",
            * "Clear threshold boundaries",
            * "Degradation paths",
            * "Recovery mechanisms",
            * "Format optimization rules"
        * "Voice Management":
          - "Session Voice Pool":
            * "System capability assessment",
            * "Format-native processing",
            * "Channel configuration tracking",
            * "Group minimum guarantees"
          - "Resource Monitoring":
            * "Current system load",
            * "Available voice capacity",
            * "Format-specific overhead"
          - "Allocation Strategy:",
            * "System-wide limit: 24 stereo voices (48 mono) across Groups A-D",
            * "Audition System: Single dedicated stereo voice",
            * "Total system output: 25 stereo voices maximum",
            * "Format-native processing required",
            * "Dynamic reallocation permitted"
          - "Group Configuration:",
            * "2-4 voices per group (choke mode)",
            * "Mixed mono/stereo allocation from same pool",
            * "Automatic scaling based on system load"

  "Format-Native Processing:",
    - "Definition: Maintain original format unless explicitly converted",
    - "Requirements:",
      * "Sample rate preservation",
      * "Bit depth maintenance",
      * "Channel configuration respect",
      * "Resource-aware allocation"

    "Global Requirements:",
      - "Performance Standards:",
        * "Performance Requirements - See PLATFORM_SPEC.md:",
          - "Audio chain: <5ms end-to-end",
          - "Emergency protocol phases aligned",
          - "Resource thresholds standardized",
      - "Resource Management:",
        * "Voice Allocation:",
          - "24 stereo voices (48 mono) across Groups A-D",
          - "Single stereo voice for Audition Bus",
          - "Format-native processing required"
  ]
}