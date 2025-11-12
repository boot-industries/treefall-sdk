# OCC123: Admin Menu Backend Features - Sprint Plan

**Document Version:** 1.0
**Date:** 2025-01-12
**Sprint Duration:** 3 weeks
**Estimated Story Points:** 38 points

## Executive Summary

This sprint implements the Admin Menu backend features for Clip Composer, providing system-level administration capabilities including folder management, audio device configuration, output patching, licensing, and system diagnostics. These features require password-protected access and control critical system settings.

## Sprint Goals

1. Implement secure admin authentication system
2. Build audio device discovery and configuration
3. Create output device patching and routing
4. Develop licensing and unlock code system
5. Implement system diagnostics and utilities

## Feature Breakdown

### 1. Admin Authentication (3 points)

**Backend Components:**
- Password-protected admin mode
- Session-based authentication
- Role-based access control
- Admin action audit logging

**Data Models:**
```typescript
interface AdminSession {
  id: string;
  userId: string;
  startedAt: Date;
  expiresAt: Date;
  ipAddress: string;
}

interface AdminAuditLog {
  id: string;
  timestamp: Date;
  userId: string;
  action: string;
  details: Record<string, any>;
  ipAddress: string;
}
```

**API Endpoints:**
- `POST /api/admin/login` - Authenticate admin user
- `POST /api/admin/logout` - End admin session
- `GET /api/admin/session` - Check admin session status
- `GET /api/admin/audit-log` - Retrieve audit log entries

### 2. File Folders Management (4 points)

**Backend Components:**
- Folder location configuration
- Path validation and existence checks
- Default folder creation
- Cross-platform path handling

**Data Models:**
```typescript
interface FolderConfig {
  tempFolder: string;              // For external editor copies
  localFolder: string;             // For network/CD copies
  packageRootFolder: string;       // For extracted packages
  playoutLogsFolder: string;       // For playout logs
  sessionBackupFolder: string;     // For auto-backups
  userSessionFolder: string;       // Default session location
  userSessionDefault: boolean;     // Always use default
  audioFilesFolder: string;        // Default audio location
  audioFilesDefault: boolean;      // Always use default
}
```

**API Endpoints:**
- `GET /api/admin/folders` - Get all folder configurations
- `PUT /api/admin/folders` - Update folder configurations
- `POST /api/admin/folders/validate` - Validate folder paths
- `POST /api/admin/folders/create` - Create missing folders

### 3. System Diagnostics (5 points)

**Backend Components:**
- DirectX information retrieval
- Network scanner for SpotOn systems
- Audio device capability analysis
- System information collector

**Data Models:**
```typescript
interface DirectXInfo {
  version: string;
  displayDrivers: DisplayDriver[];
  soundDrivers: SoundDriver[];
  inputDevices: InputDevice[];
}

interface NetworkScanResult {
  hostname: string;
  ipAddress: string;
  version: string;
  runTime: string;
  computerName: string;
  userName: string;
  securityDongle: string;
}

interface AudioModeSupport {
  deviceName: string;
  supportedFormats: AudioFormat[];
  maxChannels: number;
  maxSampleRate: number;
  maxBitDepth: number;
}

interface AudioFormat {
  channels: number;
  sampleRate: number;
  bitDepth: number;
  format: 'PCM' | 'Float';
  supported: boolean;
  errorCode?: string;
}
```

**API Endpoints:**
- `GET /api/admin/diagnostics/directx` - Get DirectX information
- `POST /api/admin/diagnostics/scan-network` - Scan for SpotOn systems
- `GET /api/admin/diagnostics/audio-modes` - Get supported audio modes
- `GET /api/admin/diagnostics/system-info` - Get system information

### 4. Output Device Assignment (8 points)

**Backend Components:**
- Audio device enumeration
- Device patching/routing configuration
- Device masking (enable/disable)
- Device ordering management
- Friendly name assignment
- Speaker configuration (stereo/5.1/7.1)
- Device identification (test tones)

**Data Models:**
```typescript
interface OutputDevice {
  id: string;
  deviceId: string;
  systemName: string;
  friendlyName: string;
  order: number;
  masked: boolean;
  speakerConfig: SpeakerConfig;
  channels: number;
  isDefault: boolean;
  isEmulated: boolean;
}

type SpeakerConfig =
  | 'stereo-20deg'
  | 'stereo-5deg'
  | 'stereo-10deg'
  | 'stereo-180deg'
  | 'quad'
  | 'surround-5.1'
  | 'surround-7.1'
  | 'direct'
  | 'headphone'
  | 'mono';

interface DevicePatch {
  devices: OutputDevice[];
  defaultPatch: OutputDevice[];
  lastModified: Date;
}

interface DeviceIdentConfig {
  type: 'voice' | 'glits' | 'blits';
  deviceId: string;
  channels: number[];
}
```

**API Endpoints:**
- `GET /api/admin/output-devices` - List all output devices
- `GET /api/admin/output-devices/:id` - Get device details
- `PUT /api/admin/output-devices/:id` - Update device configuration
- `POST /api/admin/output-devices/reorder` - Reorder devices
- `POST /api/admin/output-devices/default-patch` - Save default patch
- `POST /api/admin/output-devices/clear-patch` - Reset to Windows defaults
- `POST /api/admin/output-devices/restore-default` - Restore saved default
- `POST /api/admin/output-devices/:id/identify` - Play identification tones
- `GET /api/admin/output-devices/:id/test-audio` - Generate test audio

### 5. Licensing System (5 points)

**Backend Components:**
- Unlock code validation
- Feature flag management
- License verification
- Desktop shortcut creation (5.1 mode)

**Data Models:**
```typescript
interface License {
  baseVersion: string;
  features: LicenseFeature[];
  expirationDate?: Date;
  dongleId?: string;
}

interface LicenseFeature {
  feature: 'surround-5.1' | 'spoton-p2' | 'spoton-xk';
  unlockCode: string;
  activated: boolean;
  activatedAt?: Date;
}

interface UnlockCodeValidation {
  valid: boolean;
  feature: string;
  errorMessage?: string;
}
```

**API Endpoints:**
- `GET /api/admin/license` - Get current license info
- `POST /api/admin/license/unlock-5.1` - Validate and activate 5.1 surround
- `POST /api/admin/license/unlock-p2` - Validate and activate SpotOn P2
- `POST /api/admin/license/unlock-xk` - Validate and activate SpotOn Xk
- `GET /api/admin/license/features` - List activated features

### 6. Audio Configuration (6 points)

**Backend Components:**
- Primary mixer frequency management
- Speed bar limits configuration
- Sample frequency validation
- Volume control interface
- Emulated device filtering

**Data Models:**
```typescript
interface AudioConfig {
  primaryMixerFrequency: 44100 | 48000;
  speedBarLowerLimit: 1 | 2 | 5 | 10 | 15 | 20 | 25; // percentage
  sampleFrequencyLimit: 44100 | 48000 | 96000; // Hz
  ignoreEmulatedDevices: boolean;
}

interface VolumeControl {
  deviceId: string;
  masterLevel: number; // dB
  waveLevel: number; // dB
  masterMuted: boolean;
  waveMuted: boolean;
  controls: VolumeControlChannel[];
}

interface VolumeControlChannel {
  name: string; // 'Wave', 'SW Synth', 'Line In', etc.
  balance: number; // -100 to 100
  volume: number; // dB
  muted: boolean;
}
```

**API Endpoints:**
- `GET /api/admin/audio-config` - Get audio configuration
- `PUT /api/admin/audio-config` - Update audio configuration
- `GET /api/admin/audio-config/volume-controls` - Get volume controls
- `PUT /api/admin/audio-config/volume-controls/:deviceId` - Update volume
- `POST /api/admin/audio-config/check-volumes` - Check if volumes at 0dB

### 7. Miscellaneous Settings (4 points)

**Backend Components:**
- Timer adjustment and calibration
- Cascade links configuration
- Web version check system
- GPI polling configuration
- Web hyperlinks enable/disable
- MIDI message filtering
- File access time logging

**Data Models:**
```typescript
interface MiscConfig {
  timerPeriod: 20 | 31; // milliseconds
  timerError: number; // percentage
  cascadeLinks: boolean;
  webVersionCheck: boolean;
  forceGPIPollCheck: boolean;
  enableWebHyperlinks: boolean;
  volumeControlCheck: boolean;
  filterMidiMessages: boolean;
  fileAccessTimeLogging: boolean;
}

interface VersionCheckResult {
  currentVersion: string;
  latestVersion: string;
  updateAvailable: boolean;
  downloadUrl?: string;
  releaseNotes?: string;
}
```

**API Endpoints:**
- `GET /api/admin/misc` - Get miscellaneous settings
- `PUT /api/admin/misc` - Update miscellaneous settings
- `POST /api/admin/misc/check-version` - Check for updates
- `POST /api/admin/misc/calibrate-timer` - Calibrate system timer
- `GET /api/admin/misc/timer-accuracy` - Get timer accuracy

### 8. Network Discovery (3 points)

**Backend Components:**
- UDP broadcast scanner
- SpotOn system discovery
- IP address resolution
- Network topology mapping

**Data Models:**
```typescript
interface NetworkHost {
  hostname: string;
  ipAddress: string;
  macAddress?: string;
  online: boolean;
  lastSeen: Date;
}

interface SpotOnInstance {
  hostname: string;
  ipAddress: string;
  version: string;
  uptime: number;
  userName: string;
  computerName: string;
  securityInfo: string;
}

interface NetworkScanConfig {
  scanRange: string; // CIDR notation, e.g., "192.168.0.0/24"
  timeout: number; // milliseconds
  port: number; // UDP port for SpotOn discovery
}
```

**API Endpoints:**
- `POST /api/admin/network/scan` - Start network scan
- `GET /api/admin/network/scan/:id` - Get scan results
- `POST /api/admin/network/lookup` - Lookup hostname/IP
- `GET /api/admin/network/discovered` - List discovered systems

## Technical Architecture

### Admin Access Control

```
Request → Auth Middleware → Role Check → Admin Handler → Response
                ↓
            Audit Log
```

### Audio Device Management

```
System → Device Enumeration → Capability Testing →
Configuration Storage → Patch Application → Runtime Routing
```

### Licensing Flow

```
Unlock Code Input → Validation Algorithm → Dongle Check →
Feature Activation → State Persistence → UI Update
```

## Database Schema

```sql
-- Admin sessions
CREATE TABLE admin_sessions (
    id TEXT PRIMARY KEY,
    user_id TEXT NOT NULL,
    started_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    expires_at TIMESTAMP NOT NULL,
    ip_address TEXT,
    user_agent TEXT
);

-- Admin audit log
CREATE TABLE admin_audit_log (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    user_id TEXT NOT NULL,
    action TEXT NOT NULL,
    details TEXT, -- JSON
    ip_address TEXT,
    success BOOLEAN DEFAULT 1
);

-- Folder configuration
CREATE TABLE folder_config (
    id INTEGER PRIMARY KEY,
    folder_type TEXT UNIQUE NOT NULL,
    path TEXT NOT NULL,
    use_as_default BOOLEAN DEFAULT 0,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Output devices
CREATE TABLE output_devices (
    id TEXT PRIMARY KEY,
    device_id TEXT UNIQUE NOT NULL,
    system_name TEXT NOT NULL,
    friendly_name TEXT,
    order_index INTEGER NOT NULL,
    masked BOOLEAN DEFAULT 0,
    speaker_config TEXT NOT NULL,
    channels INTEGER NOT NULL,
    is_default BOOLEAN DEFAULT 0,
    is_emulated BOOLEAN DEFAULT 0,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Default patch (for restore)
CREATE TABLE output_devices_default (
    id TEXT PRIMARY KEY,
    device_id TEXT UNIQUE NOT NULL,
    system_name TEXT NOT NULL,
    friendly_name TEXT,
    order_index INTEGER NOT NULL,
    masked BOOLEAN DEFAULT 0,
    speaker_config TEXT NOT NULL,
    saved_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- License features
CREATE TABLE license_features (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    feature TEXT UNIQUE NOT NULL,
    unlock_code TEXT NOT NULL,
    activated BOOLEAN DEFAULT 0,
    activated_at TIMESTAMP,
    dongle_id TEXT
);

-- Audio configuration
CREATE TABLE audio_config (
    id INTEGER PRIMARY KEY,
    primary_mixer_frequency INTEGER NOT NULL DEFAULT 48000,
    speed_bar_lower_limit INTEGER NOT NULL DEFAULT 25,
    sample_frequency_limit INTEGER NOT NULL DEFAULT 48000,
    ignore_emulated_devices BOOLEAN DEFAULT 0
);

-- Miscellaneous configuration
CREATE TABLE misc_config (
    id INTEGER PRIMARY KEY,
    timer_period INTEGER NOT NULL DEFAULT 20,
    cascade_links BOOLEAN DEFAULT 0,
    web_version_check BOOLEAN DEFAULT 1,
    force_gpi_poll_check BOOLEAN DEFAULT 0,
    enable_web_hyperlinks BOOLEAN DEFAULT 1,
    volume_control_check BOOLEAN DEFAULT 1,
    filter_midi_messages BOOLEAN DEFAULT 0,
    file_access_time_logging BOOLEAN DEFAULT 1
);

-- Network scan results (cache)
CREATE TABLE network_scan_cache (
    id TEXT PRIMARY KEY,
    hostname TEXT NOT NULL,
    ip_address TEXT NOT NULL,
    version TEXT,
    uptime INTEGER,
    user_name TEXT,
    computer_name TEXT,
    security_info TEXT,
    last_seen TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_audit_log_timestamp ON admin_audit_log(timestamp);
CREATE INDEX idx_audit_log_user ON admin_audit_log(user_id);
CREATE INDEX idx_output_devices_order ON output_devices(order_index);
CREATE INDEX idx_network_cache_ip ON network_scan_cache(ip_address);
```

## API Route Structure

```
/api/admin/
├── login                           POST
├── logout                          POST
├── session                         GET
├── audit-log                       GET
├── folders/
│   ├── GET     /
│   ├── PUT     /
│   ├── POST    /validate
│   └── POST    /create
├── diagnostics/
│   ├── GET     /directx
│   ├── POST    /scan-network
│   ├── GET     /audio-modes
│   └── GET     /system-info
├── output-devices/
│   ├── GET     /
│   ├── GET     /:id
│   ├── PUT     /:id
│   ├── POST    /reorder
│   ├── POST    /default-patch
│   ├── POST    /clear-patch
│   ├── POST    /restore-default
│   ├── POST    /:id/identify
│   └── GET     /:id/test-audio
├── license/
│   ├── GET     /
│   ├── POST    /unlock-5.1
│   ├── POST    /unlock-p2
│   ├── POST    /unlock-xk
│   └── GET     /features
├── audio-config/
│   ├── GET     /
│   ├── PUT     /
│   ├── GET     /volume-controls
│   ├── PUT     /volume-controls/:deviceId
│   └── POST    /check-volumes
├── misc/
│   ├── GET     /
│   ├── PUT     /
│   ├── POST    /check-version
│   ├── POST    /calibrate-timer
│   └── GET     /timer-accuracy
└── network/
    ├── POST    /scan
    ├── GET     /scan/:id
    ├── POST    /lookup
    └── GET     /discovered
```

## Implementation Phases

### Phase 1: Core Admin Infrastructure (Week 1)

**Tasks:**
1. Implement admin authentication system
2. Set up audit logging
3. Create folder management API
4. Build configuration persistence

**Deliverables:**
- Admin login/session management
- Audit log system
- Folder configuration CRUD
- Database migrations

### Phase 2: Audio Device Management (Week 2)

**Tasks:**
1. Implement audio device enumeration
2. Build device patching system
3. Create speaker configuration manager
4. Develop device identification system

**Deliverables:**
- Device discovery service
- Patch management API
- Test tone generation
- Volume control interface

### Phase 3: Licensing & Diagnostics (Week 3)

**Tasks:**
1. Implement unlock code validation
2. Build network discovery system
3. Create diagnostics collectors
4. Integrate all admin features

**Deliverables:**
- License activation system
- Network scanner
- System diagnostics API
- Complete admin panel

## Testing Strategy

### Unit Tests

```typescript
describe('AdminAuth', () => {
  it('should authenticate with valid password', async () => {
    const result = await adminAuth.login('admin', 'correct-password');
    expect(result.success).toBe(true);
    expect(result.sessionId).toBeDefined();
  });

  it('should reject invalid password', async () => {
    const result = await adminAuth.login('admin', 'wrong-password');
    expect(result.success).toBe(false);
    expect(result.error).toBe('Invalid credentials');
  });

  it('should expire sessions after timeout', async () => {
    const session = await adminAuth.login('admin', 'password');
    jest.advanceTimersByTime(31 * 60 * 1000); // 31 minutes
    const valid = await adminAuth.validateSession(session.sessionId);
    expect(valid).toBe(false);
  });
});

describe('OutputDeviceManager', () => {
  it('should enumerate audio devices', async () => {
    const devices = await deviceManager.enumerateDevices();
    expect(devices.length).toBeGreaterThan(0);
    expect(devices[0]).toHaveProperty('deviceId');
    expect(devices[0]).toHaveProperty('systemName');
  });

  it('should reorder devices correctly', async () => {
    const devices = await deviceManager.getDevices();
    const reordered = [devices[2], devices[0], devices[1]];
    await deviceManager.reorderDevices(reordered.map(d => d.id));

    const updated = await deviceManager.getDevices();
    expect(updated[0].id).toBe(devices[2].id);
    expect(updated[1].id).toBe(devices[0].id);
  });

  it('should mask/unmask devices', async () => {
    const device = await deviceManager.getDevice('device-1');
    await deviceManager.updateDevice('device-1', { masked: true });

    const updated = await deviceManager.getDevice('device-1');
    expect(updated.masked).toBe(true);
  });
});

describe('LicenseValidator', () => {
  it('should validate correct 5.1 unlock code', () => {
    const result = validator.validate5_1Code('A1CF316F76107CE');
    expect(result.valid).toBe(true);
    expect(result.feature).toBe('surround-5.1');
  });

  it('should reject invalid unlock code', () => {
    const result = validator.validate5_1Code('INVALID123');
    expect(result.valid).toBe(false);
    expect(result.errorMessage).toBeDefined();
  });
});
```

### Integration Tests

```typescript
describe('Admin API', () => {
  let sessionId: string;

  beforeEach(async () => {
    const login = await request(app)
      .post('/api/admin/login')
      .send({ username: 'admin', password: 'test-password' });
    sessionId = login.body.sessionId;
  });

  it('should update folder configuration', async () => {
    const response = await request(app)
      .put('/api/admin/folders')
      .set('X-Admin-Session', sessionId)
      .send({
        tempFolder: '/tmp/spoton',
        localFolder: '/var/lib/spoton/local'
      });

    expect(response.status).toBe(200);
    expect(response.body.tempFolder).toBe('/tmp/spoton');
  });

  it('should scan network for SpotOn systems', async () => {
    const response = await request(app)
      .post('/api/admin/network/scan')
      .set('X-Admin-Session', sessionId)
      .send({ scanRange: '192.168.0.0/24', timeout: 5000 });

    expect(response.status).toBe(200);
    expect(response.body.scanId).toBeDefined();
  });

  it('should activate 5.1 surround license', async () => {
    const response = await request(app)
      .post('/api/admin/license/unlock-5.1')
      .set('X-Admin-Session', sessionId)
      .send({ unlockCode: 'A1CF316F76107CE' });

    expect(response.status).toBe(200);
    expect(response.body.activated).toBe(true);
    expect(response.body.feature).toBe('surround-5.1');
  });
});
```

### Security Tests

```typescript
describe('Admin Security', () => {
  it('should prevent access without authentication', async () => {
    const response = await request(app)
      .get('/api/admin/folders');
    expect(response.status).toBe(401);
  });

  it('should log all admin actions', async () => {
    const session = await adminAuth.login('admin', 'password');
    await request(app)
      .put('/api/admin/audio-config')
      .set('X-Admin-Session', session.sessionId)
      .send({ primaryMixerFrequency: 44100 });

    const logs = await AdminAuditLog.findAll({
      where: { userId: 'admin' },
      order: [['timestamp', 'DESC']],
      limit: 1
    });

    expect(logs[0].action).toBe('update_audio_config');
    expect(logs[0].details).toContain('primaryMixerFrequency');
  });

  it('should prevent path traversal in folder config', async () => {
    const session = await adminAuth.login('admin', 'password');
    const response = await request(app)
      .put('/api/admin/folders')
      .set('X-Admin-Session', session.sessionId)
      .send({ tempFolder: '../../etc/passwd' });

    expect(response.status).toBe(400);
    expect(response.body.error).toContain('Invalid path');
  });
});
```

## Security Considerations

1. **Authentication:**
   - Secure password hashing (bcrypt with salt)
   - Session timeout (30 minutes default)
   - Rate limiting on login attempts
   - Audit logging of all admin actions

2. **Authorization:**
   - Role-based access control
   - Admin-only endpoints protected
   - Feature flags for licensed features
   - Dongle verification for protected features

3. **Input Validation:**
   - Path sanitization for folder configuration
   - Device ID validation
   - Unlock code format validation
   - Network range validation (prevent scan abuse)

4. **Data Protection:**
   - Encrypted storage of unlock codes
   - Secure transmission of credentials
   - Audit log integrity protection
   - Configuration backup encryption

## Performance Requirements

| Feature | Requirement | Target |
|---------|------------|--------|
| Admin Login | <500ms | Fast authentication |
| Device Enumeration | <1s | Quick device discovery |
| Network Scan | <10s | 254 IP addresses |
| Audio Mode Test | <5s per device | Comprehensive testing |
| Patch Application | <100ms | No audio interruption |
| Volume Control | <50ms | Real-time adjustment |

## Dependencies

### External Libraries

- **node-audio-device**: Audio device enumeration (cross-platform)
- **node-netinfo**: Network interface information
- **bcrypt**: Password hashing
- **ip-range-check**: Network range validation
- **platform-folders**: Cross-platform system folders
- **audio-test-generator**: Test tone generation (GLITS/BLITS)

### System Requirements

- **Windows:** DirectX 9.0c or later
- **macOS:** Core Audio support
- **Linux:** ALSA/PulseAudio/JACK

### Internal Services

- Audio routing service
- Session management service
- Configuration persistence service
- Licensing service

## Rollout Plan

### Week 1: Core Admin
- Deploy admin authentication system
- Activate audit logging
- Roll out folder management
- Configure base settings

### Week 2: Device Management
- Deploy audio device discovery
- Activate device patching
- Enable speaker configuration
- Test volume controls

### Week 3: Integration
- Activate licensing system
- Deploy network discovery
- Enable diagnostics
- Full admin panel integration

## Success Criteria

- [ ] Admin authentication functional with session management
- [ ] All folder configurations accessible and validated
- [ ] Audio device enumeration works on all platforms
- [ ] Device patching correctly routes audio
- [ ] Licensing system validates unlock codes
- [ ] Network discovery finds SpotOn instances
- [ ] All tests passing with >90% code coverage
- [ ] Audit log captures all admin actions
- [ ] API documentation complete
- [ ] Security audit passed

## Risk Mitigation

| Risk | Mitigation Strategy |
|------|-------------------|
| Platform-specific audio APIs | Abstract device interface, test on all platforms |
| License key validation | Multiple validation checks, dongle verification |
| Network scan performance | Parallel scanning, configurable timeout |
| Audio device changes | Hot-plug detection, automatic re-enumeration |
| Unauthorized access | Strong authentication, session timeout, audit log |

## Documentation Deliverables

1. **Admin Guide:**
   - Admin panel overview
   - Folder configuration setup
   - Output device patching guide
   - Licensing activation instructions

2. **API Documentation:**
   - Admin endpoint reference
   - Authentication flow
   - Device configuration schema
   - Error codes and troubleshooting

3. **Security Guide:**
   - Admin password management
   - Audit log review
   - Access control configuration
   - Security best practices

## Future Enhancements

- Multi-user admin roles (super-admin, device-admin, etc.)
- Cloud-based license management
- Remote admin interface (web-based)
- Advanced audio routing matrix
- Automatic device profile creation
- Network topology visualization
- Integration with external audio routing systems
- Support for Dante/AVB/AES67 networking

## Compliance & Standards

- **Audio Standards:** ASIO, Core Audio, ALSA, JACK
- **Network Standards:** UDP broadcast, TCP/IP
- **Security Standards:** OWASP Top 10, secure password storage
- **Logging Standards:** Common Log Format, structured JSON

## References

- SpotOn Manual - Section 10: Admin Menu
- Windows Audio API Documentation
- Core Audio Framework (macOS)
- ALSA API Documentation (Linux)
- DirectX SDK Documentation
- Audio device testing standards (GLITS/BLITS specifications)
