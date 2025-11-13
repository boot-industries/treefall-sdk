# OCC122: Engineering Menu Backend Features - Sprint Plan

**Document Version:** 1.0
**Date:** 2025-01-12
**Sprint Duration:** 3 weeks
**Estimated Story Points:** 34 points

## Executive Summary

This sprint implements the Engineering Menu backend features for Clip Composer, providing advanced configuration capabilities for professional users. The Engineering Menu includes network communication, timecode synchronization, DTMF decoding, session management, and system-level settings.

## Sprint Goals

1. Implement PBus and network communication protocols
2. Build timecode input/processing system (SMPTE/LTC)
3. Create DTMF decoder for external tone triggers
4. Develop playout logging infrastructure
5. Implement advanced session management features

## Feature Breakdown

### 1. Global Properties System (5 points)

**Backend Components:**

- Property schema definition and validation
- Multi-page property management
- Default value restoration system
- Property persistence layer

**Data Models:**

```typescript
interface PropertyPage {
  id: string;
  name: string;
  properties: Property[];
}

interface Property {
  id: string;
  name: string;
  type: 'boolean' | 'number' | 'string' | 'enum';
  defaultValue: any;
  currentValue: any;
  validation?: ValidationRule;
}
```

**API Endpoints:**

- `GET /api/engineering/properties` - Retrieve all properties
- `GET /api/engineering/properties/:page` - Get specific page
- `PUT /api/engineering/properties/:id` - Update property value
- `POST /api/engineering/properties/reset` - Reset to defaults

### 2. PBus Port Settings (3 points)

**Backend Components:**

- Serial port communication handler
- PBus protocol parser/encoder
- Baud rate configuration (9600/19200/38400)
- Address range management (1-200)

**Data Models:**

```typescript
interface PBusConfig {
  portNumber: number; // 1-16
  baudRate: 9600 | 19200 | 38400;
  parity: 'none' | 'odd' | 'even';
  stopBits: 1 | 2;
  baseAddress: number;
  addressCount: number;
}
```

**API Endpoints:**

- `GET /api/engineering/pbus` - Get PBus configuration
- `PUT /api/engineering/pbus` - Update PBus settings
- `POST /api/engineering/pbus/test` - Test PBus connection

### 3. DTMF Decoder (5 points)

**Backend Components:**

- Audio input stream processor
- DTMF tone detection algorithm (Goertzel)
- Frequency pair validation (697-1633 Hz)
- MIDI note mapping (100-115 on channel 16)
- Debug logging system

**Data Models:**

```typescript
interface DTMFConfig {
  inputSource: string; // Audio device ID
  channel: 'left' | 'right' | 'unity' | 'difference';
  sampleRate: 48000;
  bitDepth: 16;
  preFilter: 'narrow' | 'wide';
  twistLimit: number; // dB
  tone3Threshold: number; // dB
  harmonicThreshold: number; // dB
}

interface DTMFDetection {
  timestamp: number;
  tone: string; // '0'-'9', 'A'-'F'
  tone1Freq: number;
  tone2Freq: number;
  tone3Level: number;
  twist: number;
  error?: string;
}
```

**API Endpoints:**

- `GET /api/engineering/dtmf` - Get DTMF configuration
- `PUT /api/engineering/dtmf` - Update DTMF settings
- `GET /api/engineering/dtmf/status` - Get current detection status
- `POST /api/engineering/dtmf/reset` - Reset decoder parameters

### 4. Int/Ext Timecode System (8 points)

**Backend Components:**

- LTC (Longitudinal Timecode) decoder
- SMPTE format handler (25/30/29.97 fps)
- Button elapsed time calculator
- PC clock synchronization
- Timecode chase engine for AutoPlay

**Data Models:**

```typescript
interface TimecodeConfig {
  source: 'external' | 'button' | 'internal';
  audioInput?: string; // For LTC
  buttonNumber?: number; // 0-321
  channel: 'left' | 'right' | 'unity' | 'difference';
  format: '25NDF' | '30NDF' | '29.97DF';
  gainCoarse: number; // dB
  gainFine: number; // dB
}

interface TimecodeChaseConfig {
  enabled: boolean;
  offset: string; // HH:MM:SS:FF
  errorThreshold: number; // frames (1-5)
}

interface Timecode {
  hours: number;
  minutes: number;
  seconds: number;
  frames: number;
  format: string;
  dropFrame: boolean;
}
```

**API Endpoints:**

- `GET /api/engineering/timecode` - Get timecode configuration
- `PUT /api/engineering/timecode` - Update timecode settings
- `GET /api/engineering/timecode/current` - Get current timecode value
- `POST /api/engineering/timecode/sync-pc-clock` - Sync PC clock to LTC
- `GET /api/engineering/timecode/chase` - Get chase configuration
- `PUT /api/engineering/timecode/chase` - Update chase settings

### 5. Timecode Trigger List (5 points)

**Backend Components:**

- Trigger list management (up to 320 entries)
- EDL (Edit Decision List) parser
- Timecode comparison engine
- Trigger state machine (armed/playing/elapsed)

**Data Models:**

```typescript
interface TimecodeTrigger {
  id: string;
  timecode: Timecode;
  buttonNumber: number;
  enabled: boolean;
  state: 'armed' | 'playing' | 'elapsed';
}

interface EDLEntry {
  editNumber: number;
  playerIn: Timecode;
  playerOut: Timecode;
  recorderIn: Timecode;
  recorderOut: Timecode;
  buttonNumber?: number; // From dissolve field
}
```

**API Endpoints:**

- `GET /api/engineering/timecode-triggers` - Get all triggers
- `POST /api/engineering/timecode-triggers` - Add trigger
- `PUT /api/engineering/timecode-triggers/:id` - Update trigger
- `DELETE /api/engineering/timecode-triggers/:id` - Delete trigger
- `POST /api/engineering/timecode-triggers/import-edl` - Import from EDL
- `POST /api/engineering/timecode-triggers/compact` - Remove blanks and sort

### 6. Network Target (3 points)

**Backend Components:**

- UDP/TCP network protocol handler
- Master/Slave synchronization
- IP address lookup service
- MIDI message relay over network

**Data Models:**

```typescript
interface NetworkConfig {
  mode: 'master' | 'slave' | 'none';
  protocol: 'udp' | 'tcp';
  targetAddress: string; // IP or hostname
  targetPort: number;
  midiInOverNetwork: boolean;
}
```

**API Endpoints:**

- `GET /api/engineering/network` - Get network configuration
- `PUT /api/engineering/network` - Update network settings
- `POST /api/engineering/network/lookup` - Lookup IP address by hostname
- `GET /api/engineering/network/status` - Get connection status

### 7. Session Management Features (3 points)

**Backend Components:**

- Remote file location scanner
- Session file lock management
- Menu disable/enable system

**Data Models:**

```typescript
interface SessionConfig {
  tryRemoteFilename: boolean;
  lockSessionFiles: boolean;
  disableUserMenus: boolean;
}
```

**API Endpoints:**

- `GET /api/engineering/session` - Get session configuration
- `PUT /api/engineering/session` - Update session settings
- `POST /api/engineering/session/lock-files` - Lock all session files
- `POST /api/engineering/session/unlock-files` - Unlock session files

### 8. Playout Logs (4 points)

**Backend Components:**

- Playout event logger
- XML log generator
- Delayed backup system
- Log rotation and archival

**Data Models:**

```typescript
interface PlayoutLogEntry {
  timestamp: Date;
  buttonNumber: number;
  trackName: string;
  duration: number;
  midiChannel?: number;
  output: string;
  playDuration?: number; // Actual play time
}

interface PlayoutLogConfig {
  enabled: boolean;
  saveXML: boolean;
  delayBackup: boolean; // Wait until all tracks stopped
  logDirectory: string;
}
```

**API Endpoints:**

- `GET /api/engineering/playout-logs` - Get log configuration
- `PUT /api/engineering/playout-logs` - Update log settings
- `GET /api/engineering/playout-logs/entries` - Get log entries
- `POST /api/engineering/playout-logs/backup` - Trigger backup now

### 9. Miscellaneous Settings (3 points)

**Backend Components:**

- NumLock state controller
- Embedded track name handler
- Button numbering scheme manager
- Clipboard file copy integration

**Data Models:**

```typescript
interface MiscConfig {
  numLockOn: boolean;
  ignoreEmbeddedTrackNames: boolean;
  buttonNumberingScheme:
    | 'normal'
    | 'paged-rows'
    | 'paged-columns'
    | 'paged-rows-columns'
    | 'paged-columns-rows'
    | 'paged-2-blocks'
    | 'normal-column-row';
  autoCopyFileToClipboard: boolean;
}
```

**API Endpoints:**

- `GET /api/engineering/misc` - Get misc configuration
- `PUT /api/engineering/misc` - Update misc settings

### 10. System Utilities (2 points)

**Backend Components:**

- Recent lists manager
- MIDI message filter
- Sample frequency validator

**API Endpoints:**

- `POST /api/engineering/clear-recent-lists` - Clear all recent lists
- `GET /api/engineering/sample-frequency-limit` - Get limit
- `PUT /api/engineering/sample-frequency-limit` - Update limit (44.1/48/96 kHz)

## Technical Architecture

### Audio Processing Pipeline

```
Audio Input → Sample Rate Conversion → Channel Selection →
Pre-Filter → Tone Detection (DTMF/LTC) → Protocol Parser →
Event Generation → MIDI Mapping
```

### Timecode Processing

```
LTC Input → Signal Level Check → Timecode Decoder →
Format Validator → Chase Engine → Trigger Comparator →
Button Activation
```

### Network Communication

```
Local Event → Message Encoder → Network Protocol →
Checksum/Validation → Remote Receiver → Event Replay
```

## Database Schema

```sql
-- Engineering configuration
CREATE TABLE engineering_config (
    id INTEGER PRIMARY KEY,
    config_key TEXT UNIQUE NOT NULL,
    config_value TEXT NOT NULL,
    data_type TEXT NOT NULL,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- PBus configuration
CREATE TABLE pbus_config (
    id INTEGER PRIMARY KEY,
    port_number INTEGER,
    baud_rate INTEGER,
    parity TEXT,
    stop_bits INTEGER,
    base_address INTEGER,
    address_count INTEGER
);

-- DTMF configuration
CREATE TABLE dtmf_config (
    id INTEGER PRIMARY KEY,
    input_source TEXT,
    channel TEXT,
    pre_filter TEXT,
    twist_limit REAL,
    tone3_threshold REAL,
    harmonic_threshold REAL
);

-- Timecode configuration
CREATE TABLE timecode_config (
    id INTEGER PRIMARY KEY,
    source TEXT,
    audio_input TEXT,
    button_number INTEGER,
    channel TEXT,
    format TEXT,
    gain_coarse REAL,
    gain_fine REAL
);

-- Timecode triggers
CREATE TABLE timecode_triggers (
    id INTEGER PRIMARY KEY,
    timecode TEXT NOT NULL,
    button_number INTEGER NOT NULL,
    enabled BOOLEAN DEFAULT 1,
    state TEXT DEFAULT 'armed',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Network configuration
CREATE TABLE network_config (
    id INTEGER PRIMARY KEY,
    mode TEXT,
    protocol TEXT,
    target_address TEXT,
    target_port INTEGER,
    midi_in_over_network BOOLEAN
);

-- Playout logs
CREATE TABLE playout_logs (
    id INTEGER PRIMARY KEY,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    button_number INTEGER NOT NULL,
    track_name TEXT NOT NULL,
    duration REAL,
    midi_channel INTEGER,
    output TEXT,
    play_duration REAL
);

CREATE INDEX idx_playout_timestamp ON playout_logs(timestamp);
CREATE INDEX idx_timecode_triggers_button ON timecode_triggers(button_number);
```

## API Route Structure

```
/api/engineering/
├── properties/
│   ├── GET     / (list all)
│   ├── GET     /:page
│   ├── PUT     /:id
│   └── POST    /reset
├── pbus/
│   ├── GET     /
│   ├── PUT     /
│   └── POST    /test
├── dtmf/
│   ├── GET     /
│   ├── PUT     /
│   ├── GET     /status
│   └── POST    /reset
├── timecode/
│   ├── GET     /
│   ├── PUT     /
│   ├── GET     /current
│   ├── POST    /sync-pc-clock
│   ├── GET     /chase
│   └── PUT     /chase
├── timecode-triggers/
│   ├── GET     /
│   ├── POST    /
│   ├── PUT     /:id
│   ├── DELETE  /:id
│   ├── POST    /import-edl
│   └── POST    /compact
├── network/
│   ├── GET     /
│   ├── PUT     /
│   ├── POST    /lookup
│   └── GET     /status
├── session/
│   ├── GET     /
│   ├── PUT     /
│   ├── POST    /lock-files
│   └── POST    /unlock-files
├── playout-logs/
│   ├── GET     /
│   ├── PUT     /
│   ├── GET     /entries
│   └── POST    /backup
├── misc/
│   ├── GET     /
│   └── PUT     /
└── utilities/
    ├── POST    /clear-recent-lists
    ├── GET     /sample-frequency-limit
    └── PUT     /sample-frequency-limit
```

## Implementation Phases

### Phase 1: Core Infrastructure (Week 1)

**Tasks:**

1. Set up engineering configuration database tables
2. Implement property management system
3. Create base API route structure
4. Develop configuration persistence layer

**Deliverables:**

- Database migrations
- Property CRUD operations
- Configuration storage service

### Phase 2: Audio Processing (Week 2)

**Tasks:**

1. Implement DTMF decoder with Goertzel algorithm
2. Build LTC timecode decoder
3. Create audio input routing system
4. Develop signal level monitoring

**Deliverables:**

- DTMF detection engine
- Timecode parsing service
- Audio routing configuration

### Phase 3: Network & Integration (Week 3)

**Tasks:**

1. Implement network communication protocols
2. Build timecode trigger system
3. Create playout logging infrastructure
4. Integrate with existing clip management

**Deliverables:**

- Network master/slave synchronization
- Timecode chase engine
- Playout log generation
- Complete API endpoints

## Testing Strategy

### Unit Tests

```typescript
describe('DTMFDecoder', () => {
  it('should detect valid DTMF tones', async () => {
    const decoder = new DTMFDecoder(config);
    const result = await decoder.processTone(mockAudioBuffer);
    expect(result.tone).toBe('5');
    expect(result.tone1Freq).toBeCloseTo(770, 1);
    expect(result.tone2Freq).toBeCloseTo(1336, 1);
  });

  it('should reject invalid twist ratio', async () => {
    const decoder = new DTMFDecoder(config);
    const result = await decoder.processTone(mockBadTwistBuffer);
    expect(result.error).toBe('revtw');
  });
});

describe('TimecodeDecoder', () => {
  it('should parse 25fps non-drop frame timecode', () => {
    const decoder = new TimecodeDecoder();
    const tc = decoder.parse('10:23:45:12', '25NDF');
    expect(tc.hours).toBe(10);
    expect(tc.frames).toBe(12);
    expect(tc.dropFrame).toBe(false);
  });

  it('should handle drop frame timecode', () => {
    const decoder = new TimecodeDecoder();
    const tc = decoder.parse('00:01:00:02', '29.97DF');
    // Frame 0 and 1 are dropped at start of each minute except every 10th
    expect(decoder.isValidDropFrame(tc)).toBe(true);
  });
});
```

### Integration Tests

```typescript
describe('Engineering API', () => {
  it('should update PBus configuration', async () => {
    const response = await request(app).put('/api/engineering/pbus').send({
      portNumber: 1,
      baudRate: 38400,
      baseAddress: 10,
      addressCount: 200,
    });
    expect(response.status).toBe(200);
    expect(response.body.baudRate).toBe(38400);
  });

  it('should import EDL timecode triggers', async () => {
    const edlContent = readFileSync('test-fixtures/test.edl', 'utf-8');
    const response = await request(app)
      .post('/api/engineering/timecode-triggers/import-edl')
      .send({ edlContent, column: 'recorderIn' });
    expect(response.status).toBe(200);
    expect(response.body.triggers.length).toBeGreaterThan(0);
  });
});
```

### Performance Tests

```typescript
describe('Performance', () => {
  it('should process DTMF in real-time (<25ms)', async () => {
    const decoder = new DTMFDecoder(config);
    const start = Date.now();
    await decoder.processTone(mockAudioBuffer);
    const duration = Date.now() - start;
    expect(duration).toBeLessThan(25);
  });

  it('should handle 1000 timecode triggers efficiently', async () => {
    const triggers = generateMockTriggers(1000);
    await TimecodeTrigger.bulkCreate(triggers);

    const start = Date.now();
    const result = await timecodeService.checkTriggers('10:00:05:00');
    const duration = Date.now() - start;

    expect(duration).toBeLessThan(10); // <10ms for trigger check
  });
});
```

## Security Considerations

1. **Network Communication:**
   - Validate all network messages
   - Implement rate limiting on UDP/TCP
   - Use checksums for message integrity

2. **File System Access:**
   - Validate file paths for remote filename search
   - Prevent directory traversal attacks
   - Implement file lock timeout protection

3. **Audio Input:**
   - Sanitize audio device identifiers
   - Limit audio buffer sizes
   - Prevent resource exhaustion

4. **Configuration:**
   - Validate all configuration values
   - Implement rollback on invalid settings
   - Log all configuration changes

## Performance Requirements

| Feature                   | Requirement | Target                     |
| ------------------------- | ----------- | -------------------------- |
| DTMF Detection Latency    | <25ms       | Real-time audio processing |
| Timecode Decode Latency   | <10ms       | Frame-accurate triggering  |
| Network Message Latency   | <5ms        | Synchronized playback      |
| Trigger Check Performance | <10ms       | 1000+ triggers             |
| Playout Log Write         | <1ms        | No audio disruption        |

## Dependencies

### External Libraries

- **dtmf-decoder**: DTMF tone detection
- **ltc-decoder**: LTC timecode parsing
- **serialport**: PBus serial communication
- **dgram/net**: UDP/TCP networking
- **xml2js**: XML log generation
- **edl-parser**: EDL file parsing

### Internal Services

- Audio routing service
- Session management service
- MIDI system
- Button management service

## Rollout Plan

### Week 1: Infrastructure

- Deploy database schema
- Roll out property management API
- Configure logging and monitoring

### Week 2: Audio Features

- Deploy DTMF decoder service
- Activate timecode processing
- Enable audio input routing

### Week 3: Integration

- Activate network synchronization
- Enable playout logging
- Deploy timecode triggers
- Full system integration testing

## Success Criteria

- [ ] All engineering menu features accessible via API
- [ ] DTMF decoder detects tones with >95% accuracy
- [ ] Timecode decoder handles all standard formats
- [ ] Network synchronization maintains <10ms latency
- [ ] Playout logs generated without audio glitches
- [ ] All tests passing with >90% code coverage
- [ ] API documentation complete
- [ ] Performance benchmarks met

## Risk Mitigation

| Risk                          | Mitigation Strategy                        |
| ----------------------------- | ------------------------------------------ |
| Audio processing latency      | Optimize algorithms, use worker threads    |
| Network synchronization drift | Implement clock sync, adjust for latency   |
| DTMF false positives          | Strict validation, configurable thresholds |
| Timecode decode errors        | Multiple format support, error recovery    |
| File system performance       | Cache remote lookups, async operations     |

## Documentation Deliverables

1. **API Documentation:**
   - Engineering endpoints reference
   - Configuration schema definitions
   - Example request/response payloads

2. **Integration Guide:**
   - Audio routing setup
   - Network configuration
   - Timecode synchronization

3. **Troubleshooting Guide:**
   - DTMF decoder tuning
   - Timecode format issues
   - Network connectivity problems

## Future Enhancements

- Advanced DTMF validation modes
- Support for additional timecode formats (VITC, MTC)
- Distributed network synchronization (>2 systems)
- Machine learning for DTMF accuracy improvement
- Real-time playout log streaming
- Integration with external broadcast systems

## References

- SpotOn Manual - Section 09: Engineering Menu
- DTMF ITU-T Q.23 Specification
- SMPTE 12M Timecode Standard
- PBus Protocol Documentation (if available)
- EDL File Format Specification
