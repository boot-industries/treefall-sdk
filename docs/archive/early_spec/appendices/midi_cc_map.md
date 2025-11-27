# MIDI CC Map Specification

## Global Controls (Channel 1)
- **Transport**
  * CC 1: Master Level (0-127)
  * CC 2: Tempo Fine (0-127 = ±1%)
  * CC 3: Tempo Coarse (0-127 = 60-180 BPM)
  * CC 4: Emergency Stop (127 = stop)

- **Monitor**
  * CC 5: Monitor Level (0-127)
  * CC 6: Preview Level (0-127)
  * CC 7: Headphone Level (0-127)

## Selected Clip Controls (Channel 2)
- **Playback**
  * CC 10: Clip Level (0-127)
  * CC 11: Pan Position (0-127 = L100-R100)
  * CC 12: Time Stretch (-64-+63 = -50%-+100%)
  * CC 13: Pitch Shift (-64-+63 = -12-+12)
  * CC 14: Sync Enable (0=off, 127=on)
  * CC 15: Pitch Lock (0=off, 127=on)
  * Format Controls
    - CC 16: Format Type (0=mono, 127=stereo)
    - CC 17: Width Control (stereo, 0-127)
    - CC 18: Expansion Amount (mono, 0-127)
    - CC 19: Channel Mode (0=mono, 1=stereo)

- **Fade Controls**
  * CC 20: IN Fade Time (0-127 = 0-3s)
  * CC 21: IN Fade Curve Type
    - 0-31: Logarithmic
    - 32-63: Linear
    - 64-95: Exponential
    - 96-127: S-Curve
  * CC 22: OUT Fade Time (0-127 = 0-3s)
  * CC 23: OUT Fade Curve Type (same ranges)
  * CC 24: Crossfade Time (0-127 = 0-3s)
  * CC 25: Crossfade Curve Type (same ranges)
  * CC 26-28: Power Compensation (0=off, 127=on)
  * CC 29-31: Quick Access Controls

## Grid Controls (Channel 3)
- **Navigation**
  * CC 40: Tab Select (0-7)
  * CC 41: Group Select (0=none, 1-4=A-D)
  * CC 42: Grid Page (if applicable)

- **FIFO Control**
  * CC 45: FIFO Enable (0=off, 127=on)
  * CC 46: FIFO Clear
  * CC 47: FIFO Position (0-127)

## System Controls (Channel 16)
- **Configuration**
  * CC 120: All Notes Off
  * CC 121: Reset All Controllers
  * CC 122: Local Control
  * CC 123: All Notes Off
  * CC 124: Omni Off
  * CC 125: Omni On
  * CC 126: Mono Mode
  * CC 127: Poly Mode

## System Control CCs
- Voice Management:
  * CC 14: System capability query
  * CC 15: Status request
  * CC 16: Group allocation adjust 