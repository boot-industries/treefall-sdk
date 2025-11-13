# ORP072 AES67 Network Audio Driver Integration Plan

AES67-native audio driver implementation for Orpheus SDK, enabling professional network audio infrastructure with broadcast-safe, deterministic IP audio streaming compatible with Dante, Ravenna, Q-LAN, and other AES67-compliant systems.

## Context

### Strategic Motivation

Orpheus SDK is designed as professional audio infrastructure for 10+ year deployment horizons. AES67 support transforms the SDK from local-only audio processing into network-native audio infrastructure, addressing real-world requirements for:

- **Broadcast facilities:** Multi-room audio distribution without dedicated hardware
- **Theater installations:** Distributed speaker systems via standard Ethernet
- **Live sound:** Integration with existing Dante/Ravenna equipment
- **Cloud workflows:** Multi-machine audio collaboration over networks

AES67 is an open standard (not proprietary) that provides interoperability across vendor ecosystems [1], aligning with the SDK's "sovereign audio ecosystem" philosophy.

### Architectural Fit

AES67 integrates as a standard audio driver implementing the `IAudioDriver` interface:

```
src/platform/audio_drivers/
├── coreaudio/         # macOS (local hardware)
├── wasapi/            # Windows (local hardware)
├── asio/              # Windows pro (local hardware)
├── dummy/             # Testing/simulation
└── aes67/             # Network audio (IP-based) ⭐
    ├── aes67_driver.h
    ├── aes67_driver.cpp
    ├── rtp_packet.h       # RFC 3550/3551 RTP transport
    ├── ptp_sync.h         # IEEE 1588 PTP clock sync
    └── sdp_parser.h       # SDP session description
```

The rest of the SDK remains host-neutral and deterministic - transport, routing, and application layers do not differentiate between local hardware audio and network audio.

### Synergy with Routing Matrix

The routing matrix implementation (ORP070, Phase 2) is inspired by Dante Controller's N×M routing capabilities [2]. AES67 driver support makes this **actually be network routing**, not just local mixing:

- Subscribe to AES67 streams from any networked device
- Route streams through the routing matrix (64 channels → 16 groups → 32 outputs)
- Publish routing matrix outputs as AES67 streams for distribution
- Multi-building audio without dedicated audio cabling

This elevates Orpheus from "DAW" to "network audio infrastructure."

## Technical Foundation

### AES67 Standard Overview

**AES67-2018** defines interoperable IP audio transport using open standards [1]:

| Component               | Standard                      | Purpose                          |
| ----------------------- | ----------------------------- | -------------------------------- |
| **Audio Transport**     | RTP/RTSP (IETF RFC 3550/3551) | Real-time audio packet delivery  |
| **Clock Sync**          | PTP (IEEE 1588-2008)          | Sample-accurate timing alignment |
| **Session Description** | SDP (IETF RFC 4566)           | Stream format advertisement      |
| **Discovery**           | SAP (IETF RFC 2974)           | Optional stream announcement     |

**Key Parameters:**

- Sample rates: 48 kHz, 96 kHz (broadcast standard)
- Bit depths: 16-bit, 24-bit PCM (L16, L24 payload types)
- Latency: 0.125ms - 5ms typical (configurable)
- Channels: Up to 8 per stream (32+ streams supported)

### Design Principles

Following Orpheus SDK core principles:

**1. Offline-First**

- Local multicast discovery (no internet required)
- Manual stream subscription via SDP files
- Network failures gracefully degrade (silence, not crashes)

**2. Deterministic**

- PTP clock synchronization for sample-accurate alignment
- Fixed-size RTP packet buffers (pre-allocated)
- Jitter buffer with configurable latency targets
- Deterministic sample delivery (±1 sample tolerance)

**3. Host-Neutral**

- Platform-agnostic UDP/RTP implementation
- PTP client works on Windows/macOS/Linux
- No vendor-specific extensions (pure AES67)

**4. Broadcast-Safe**

- Zero allocations in audio callback
- Packet loss concealment (hold-last-sample)
- Graceful network degradation (meters show underruns)
- Watchdog timer for stalled streams

## Implementation Plan

### Milestone 1.4: AES67 Driver Foundation (9 days)

**Objective:** Implement basic AES67 RTP sender/receiver with PTP clock sync, enabling network audio I/O alongside existing drivers.

#### Task 1.4.1: RTP Transport Layer (3 days)

**Deliverables:**

- `rtp_packet.h/cpp` - RTP packet encode/decode (RFC 3550)
- `rtp_session.h/cpp` - UDP socket management, multicast subscription
- L16/L24 payload support (48 kHz, 24-bit PCM)
- Jitter buffer with configurable latency (0.5ms - 5ms)

**Acceptance Criteria:**

- Send/receive RTP packets on multicast 239.69.x.x
- Packet loss < 0.01% on gigabit LAN
- Jitter buffer compensates for ±2ms network variance
- Zero allocations in RTP receive path

**Implementation Notes:**

```cpp
// RFC 3550 RTP header (fixed 12 bytes)
struct RTPHeader {
    uint8_t version : 2;      // Version (2)
    uint8_t padding : 1;
    uint8_t extension : 1;
    uint8_t csrc_count : 4;
    uint8_t marker : 1;
    uint8_t payload_type : 7; // 11 = L16, custom = L24
    uint16_t sequence_number;
    uint32_t timestamp;       // Sample timestamp
    uint32_t ssrc;            // Stream identifier
};

// Pre-allocated packet buffer (audio thread)
class RTPReceiver {
    std::array<uint8_t, MAX_PACKET_SIZE> m_packet_buffer;
    JitterBuffer m_jitter_buffer; // Ring buffer, fixed size
};
```

#### Task 1.4.2: PTP Clock Synchronization (4 days)

**Deliverables:**

- `ptp_sync.h/cpp` - IEEE 1588 PTP slave implementation
- PTP v2 message parsing (Sync, Follow_Up, Delay_Req, Delay_Resp)
- Clock offset calculation with moving average filter
- Sample rate converter (SRC) for clock drift correction

**Acceptance Criteria:**

- Synchronize to PTP master within 100µs (±4.8 samples @ 48kHz)
- Maintain sync for 1+ hour continuous operation
- Graceful fallback to local clock if PTP unavailable
- Expose PTP status via driver API

**Implementation Notes:**

```cpp
// PTP timestamp (nanosecond precision)
struct PTPTimestamp {
    uint64_t seconds;
    uint32_t nanoseconds;
};

// Clock offset tracking
class PTPClient {
    std::atomic<int64_t> m_clock_offset_ns; // Audio thread reads
    std::array<int64_t, 8> m_offset_history; // Moving average filter

    // UI thread: Update offset from PTP messages
    void updateOffset(int64_t measured_offset_ns);

    // Audio thread: Query current time
    int64_t getPTPTimeNanos() const;
};
```

**PTP Message Flow:**

1. Master broadcasts **Sync** (t1)
2. Master sends **Follow_Up** (precise t1 timestamp)
3. Slave sends **Delay_Req** (t3)
4. Master responds **Delay_Resp** (t4)
5. Offset calculation: `offset = ((t2 - t1) - (t4 - t3)) / 2`

#### Task 1.4.3: AES67 Driver Implementation (2 days)

**Deliverables:**

- `aes67_driver.h/cpp` - IAudioDriver implementation
- SDP session parsing for stream subscription
- Multi-stream receive (up to 16 streams × 8 channels = 128 inputs)
- Single stream transmit (configurable channel count)

**Acceptance Criteria:**

- Implements all IAudioDriver interface methods
- Subscribe to streams via SDP URL or manual configuration
- Publish streams with auto-generated SDP
- Start/stop without audio dropouts
- Meters show network underruns (not silent failures)

**Driver Configuration:**

```cpp
struct AES67DriverConfig {
    // Network
    std::string interface_address; // "0.0.0.0" = auto-detect
    uint16_t base_port;            // Default: 5004

    // RTP
    uint32_t sample_rate;          // 48000 or 96000
    uint8_t bit_depth;             // 16 or 24
    uint8_t channels_per_stream;   // 2, 4, or 8
    float jitter_buffer_ms;        // 0.5 - 5.0ms latency target

    // PTP
    bool enable_ptp;               // Default: true
    std::string ptp_domain;        // Default: "0"

    // Streams
    std::vector<std::string> subscribe_urls; // SDP URLs
    std::string publish_name;                // Stream name for TX
};
```

**Public API:**

```cpp
// include/orpheus/aes67_driver.h
class AES67Driver : public IAudioDriver {
public:
    // Standard IAudioDriver interface
    ErrorCode initialize(const AudioDriverConfig& config) override;
    ErrorCode start() override;
    ErrorCode stop() override;

    // AES67-specific configuration
    ErrorCode subscribeStream(const std::string& sdp_url);
    ErrorCode unsubscribeStream(uint32_t stream_id);
    ErrorCode publishStream(const std::string& stream_name,
                           uint16_t num_channels);

    // Status queries
    struct StreamStatus {
        uint32_t stream_id;
        std::string name;
        std::string source_address;
        uint16_t channels;
        uint32_t packets_received;
        uint32_t packets_lost;
        float jitter_ms;
        bool ptp_synced;
    };
    std::vector<StreamStatus> getStreamStatus() const;

    // PTP status
    struct PTPStatus {
        bool synced;
        int64_t offset_ns;
        std::string master_address;
    };
    PTPStatus getPTPStatus() const;
};

// Factory function
std::unique_ptr<IAudioDriver> createAES67Driver();
```

### Build Integration

**CMake Configuration:**

```cmake
# CMakeLists.txt
option(ORPHEUS_ENABLE_DRIVER_AES67 "Build AES67/RTP network audio driver" OFF)

if(ORPHEUS_ENABLE_DRIVER_AES67)
    add_subdirectory(src/platform/audio_drivers/aes67)
    list(APPEND ORPHEUS_CORE_TARGETS orpheus_audio_driver_aes67)
endif()
```

**Build Command:**

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DORPHEUS_ENABLE_DRIVER_AES67=ON

cmake --build build --target orpheus_audio_driver_aes67
```

### Testing Strategy

#### Unit Tests (`tests/audio_drivers/aes67_driver_test.cpp`)

**Test Cases:**

1. **RTP Packet Encode/Decode** - Verify RFC 3550 compliance
2. **Jitter Buffer** - Test packet reordering, loss concealment
3. **PTP Clock Sync** - Mock PTP messages, verify offset calculation
4. **SDP Parsing** - Parse valid/invalid SDP files
5. **Stream Subscription** - Subscribe/unsubscribe without leaks
6. **Graceful Degradation** - Network loss → silence (not crash)

#### Integration Tests

**Loopback Test:**

```cpp
// Send/receive on same machine (multicast loopback)
TEST(AES67DriverIntegration, LocalLoopback) {
    auto tx_driver = createAES67Driver();
    auto rx_driver = createAES67Driver();

    // TX: Publish 1kHz sine wave
    tx_driver->publishStream("test-stream", 2);

    // RX: Subscribe to same stream
    rx_driver->subscribeStream("239.69.1.1:5004");

    // Verify: Received audio matches transmitted
    // Latency: < 5ms
    // Packet loss: < 0.01%
}
```

**Interop Test (requires Dante Virtual Soundcard or AES67 hardware):**

```bash
# TX from Orpheus → RX in Dante DVS
./build/tests/audio_drivers/aes67_interop_test --mode=tx

# RX from Dante DVS → Orpheus
./build/tests/audio_drivers/aes67_interop_test --mode=rx
```

#### Performance Tests

**Stress Test:**

- 16 simultaneous RX streams (128 channels)
- 1 TX stream (8 channels)
- 48kHz/24-bit
- Run for 1 hour
- **Success criteria:** < 10 underruns/hour, PTP offset < 200µs

## Dependencies

### Implementation Approach: From Scratch (Recommended)

**Rationale:**

- RTP is simple (RFC 3550 is 104 pages, core logic ~500 LOC)
- PTP slave is manageable (~800 LOC for v2 basic profile)
- No GPL/LGPL licensing concerns
- Full control over real-time performance

**Estimated Implementation Effort:**

- RTP transport: 500 LOC
- PTP client: 800 LOC
- AES67 driver: 600 LOC
- Tests: 400 LOC
- **Total: ~2,300 LOC over 9 days**

### Alternative: Use Existing Library

**Option 1: AES67-Linux** [3]

- **License:** GPL-3.0 (requires Orpheus SDK to be GPL)
- **Pros:** Battle-tested, includes PTP daemon integration
- **Cons:** Linux-only, GPL viral licensing

**Option 2: Merging Technologies' AES67 Daemon** [4]

- **License:** Proprietary (evaluation builds available)
- **Pros:** Production-ready, Windows/macOS/Linux
- **Cons:** Licensing fees, binary blob (not auditable)

**Recommendation:** Implement from scratch for full control and MIT licensing.

## Deferred Features (Post-MVP)

These features are valuable but not required for initial release:

### SAP Discovery

- **What:** Automatic stream discovery via SAP/SDP announcements (RFC 2974)
- **Why defer:** Manual SDP configuration is sufficient for initial deployments
- **Timeline:** Milestone 2.x (after routing matrix integration)

### Multicast DNS (mDNS)

- **What:** Zeroconf service discovery for AES67 streams
- **Why defer:** SAP already provides discovery, mDNS is redundant
- **Timeline:** Only if user-requested

### RTSP Control Protocol

- **What:** Dynamic stream setup/teardown via RTSP (RFC 2326)
- **Why defer:** Static SDP configuration covers 95% of use cases
- **Timeline:** Milestone 3.x (if broadcast workflows require it)

### AES67 High-Performance Mode

- **What:** SMPTE ST 2110 compatibility (video sync, < 1ms latency)
- **Why defer:** Standard AES67 (5ms latency) is sufficient for audio-only
- **Timeline:** Milestone 4.x (if video workflows emerge)

### Redundant Streams (SMPTE 2022-7)

- **What:** Dual-path audio for fault tolerance
- **Why defer:** Single-path is reliable on gigabit LANs
- **Timeline:** Milestone 3.x (if mission-critical deployments require it)

## Integration with Orpheus Applications

### Clip Composer (OCC)

**Use Case:** Theater soundboard with distributed speakers

**Configuration:**

```json
{
  "audio_driver": "aes67",
  "aes67_config": {
    "publish_name": "OCC-Cue-Bus",
    "channels": 8,
    "subscribe_urls": [
      "file:///etc/orpheus/streams/house-left.sdp",
      "file:///etc/orpheus/streams/house-right.sdp"
    ]
  },
  "routing_matrix": {
    "num_channels": 16,
    "num_groups": 4,
    "num_outputs": 8
  }
}
```

**Workflow:**

1. Clips play through routing matrix
2. Routing matrix outputs → AES67 TX stream (8 channels)
3. Network amplifiers receive AES67 → drive speakers
4. Operator receives monitor feed via AES67 RX

### Wave Finder

**Use Case:** Analyze network audio streams without local audio hardware

**Configuration:**

```json
{
  "audio_driver": "aes67",
  "aes67_config": {
    "subscribe_urls": ["file:///Users/chris/streams/orchestra-mix.sdp"]
  }
}
```

### Orpheus SDK Minhost CLI

**Example Commands:**

```bash
# List available AES67 streams on network
./orpheus_minhost --driver aes67 --discover

# Record AES67 stream to file
./orpheus_minhost --driver aes67 \
  --subscribe "239.69.100.1:5004" \
  --record output.wav \
  --duration 60

# Transmit file as AES67 stream
./orpheus_minhost --driver aes67 \
  --publish "MyStream" \
  --input click-track.wav \
  --loop
```

## Documentation Updates

### New Documents

- `docs/DRIVER_INTEGRATION_GUIDE.md` - Add AES67 section (update existing)
- `README.md` - Add "Network Audio (AES67)" to feature list

### Updated Documents

- `ROADMAP.md` - Add Milestone 1.4: AES67 Driver (9 days)
- `docs/ADAPTERS.md` - Mention AES67 driver availability

## Next Actions

- [ ] **Approval checkpoint:** Confirm AES67 implementation priority (before/after routing matrix completion)
- [ ] **Create Milestone 1.4 tasks** in ORP070 or separate sprint document
- [ ] **Prototype RTP transport** (2 days exploratory work to validate complexity estimates)
- [ ] **Research PTP libraries** - Evaluate ptpd vs from-scratch implementation
- [ ] **Draft AES67 driver API** in `include/orpheus/aes67_driver.h` (header-only spec)
- [ ] **Update ROADMAP.md** with Milestone 1.4 timeline

## References

[1] https://www.aes.org/publications/standards/search.cfm?docID=96
[2] https://www.audinate.com/products/software/dante-controller
[3] https://github.com/bondagit/aes67-linux
[4] https://www.merging.com/products/aes67
