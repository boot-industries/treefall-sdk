# MIDI Implementation Chart

## Channel Assignments
- Channel 1: Global Controls
- Channel 2: Selected Clip
- Channel 3: Grid Navigation
- Channel 4-15: Direct Clip Access
- Channel 16: System Controls

[Previous CC map content...]

## NRPN Controls
- **High Resolution Parameters**
  * NRPN 0: Time Stretch (14-bit)
  * NRPN 1: Pitch Shift (14-bit)
  * NRPN 2: Fade Times (14-bit)
  * NRPN 3: Position Control (14-bit)

## Note Messages
- **Grid Triggers**
  * Note 0-127: Direct clip triggers
  * Velocity: Launch velocity
  * Release: Stop behavior

## Program Changes
- **Scene/Tab Selection**
  * PC 0-7: Tab selection
  * PC 8-127: Reserved

## System Real Time
- **Clock Sync**
  * Timing Clock
  * Start/Stop/Continue
  * Active Sensing 

### Resource Control
- Voice Management:
  * Session Voice Pool control:
    - System capability query (CC 14)
    - Format status request (CC 15)
    - Channel config select (CC 16)
    - Group allocation adjust (CC 17) 
  * CC 50: Format State Request
    - 0: Query current state
    - 1: Force stereo
    - 2: Force mono
    - 3: Auto-select

### Resource Monitoring
  * CC 14: Voice usage (0-127)
    * 0-96: Main pool usage (24 stereo max)
    * 97-100: Preview voice status
  * CC 15: Format status
  * CC 16: Group allocation
  - Returns 0-127 scaled to application ceiling
  - 127 = 90% of total system CPU 

### Fade Control Messages
- **Required Fade Control**
  * Loop Control
    - CC 60: Loop state enable/disable

- **Custom Fade Control** (Selected Clip Only)
  * Basic Fade Control
    - CC 61: IN fade time (0-127 = 0-3.0s)
    - CC 62: OUT fade time (0-127 = 0-3.0s)
    - CC 63: Curve type select
      * 0-31: Logarithmic
      * 32-63: Linear
      * 64-95: Exponential
      * 96-127: S-Curve
  * Power Compensation
    - CC 64: Toggle power comp (0=off, 127=on) 