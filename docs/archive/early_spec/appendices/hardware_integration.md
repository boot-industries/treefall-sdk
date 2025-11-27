# Hardware Integration Reference

## GPI/GPO Specifications
- **Input Types**
  * Contact closure
  * Voltage trigger (5-24V)
  * Opto-isolated input
  * Edge detection

- **Output Types**
  * Relay contact (NO/NC)
  * Open collector
  * Voltage output (5V/12V)
  * Opto-isolated output

### Audio Routing
- **Format-Aware Channel Configuration**
  * Input Detection
    - Security Implementation:
      * Format state validation
      * Resource allocation protection
      * Emergency protocol integration
    - Stereo Pair Analysis
      * Channel correlation check
      * Width analysis
      * Resource requirement calculation
    - Mono Channel Detection
      * Single channel validation
      * Pan law configuration
      * Resource optimization
  * Resource-Aware Assignment
    - Format-Based Routing
      * Stereo voice
      * Mono voice
      * Processing chain assignment
    - Emergency Protocol Integration
      * Hardware mute triggers
      * Format state preservation
      * Resource protection paths

### Resource Control Protocol
  - **Voice Management Interface**
    * Session Voice Pool control:
      - System capability query (0x14)
      - Format status request (0x15)
      - Channel config select (0x16)
      - Group allocation adjust (0x17)
    * Response Format:
      - Status byte (0xF0)
      - Data bytes:
        * Current allocation
        * System capability
        * Format status
        * Group assignments
  - **Resource Monitoring**
    * Load Query (0x14):
      - Returns 0-255 scaled to application ceiling
      - 255 = 90% of total system CPU
      - Example conversions:
        * 178 (app 70%) = 63% system CPU
        * 204 (app 80%) = 72% system CPU
        * 229 (app 90%) = 81% system CPU
    * Status Response Format:
      - Current allocation
      - System capability

### Audio Output Configuration
  * Group Outputs (A-D)
    - Hardware output assignment
    * Stereo output pairs
    * Individual left/right assignment
    - Level control
    - Emergency mute

### Clock Integration
- **Time Source Handling**
-   * LTC via audio input (primary)
-   * MTC via MIDI ports (secondary)
+   * Time Source Connections
+     - LTC Input Options:
+       * Audio input (balanced/unbalanced)
+       * Dedicated LTC port (when available)
+     - MTC Sources:
+       * Hardware MIDI ports
+       * Network MIDI
+       * Virtual MIDI routing
+     - Sync Options:
+       * PTP network
+       * Blackburst/tri-level
+       * Word clock
+   * Configuration Interface
+     - Source priority assignment
+     - Quality threshold adjustment
+     - Failover behavior settings

### Control Surface Integration
  * Clip Parameters
    - Level/Pan Control
    - Transport Control
    - Selected Clip Fades
      * IN/OUT time adjust
      * Curve type select
      * Power comp toggle

[... continue with detailed hardware specs] 