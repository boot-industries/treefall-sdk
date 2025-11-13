# ORP108 Network Audio Claims Cleanup

**Date:** November 9, 2025
**Type:** Technical Debt Resolution
**Status:** Complete

## Executive Summary

Removed misleading claims about native AES67/Dante protocol support from the Orpheus SDK documentation. The SDK supports network audio devices via manufacturer-provided drivers (e.g., Dante Virtual Soundcard), not through direct protocol implementation. This cleanup eliminates confusion and sets accurate expectations.

## Background

The codebase contained multiple references to "AES67/Dante network audio support" that implied protocol-level integration:

- ARCHITECTURE.md claimed "Network Audio (AES67/RTP)" with PTP sync
- ROADMAP.md described a 9-day "AES67 Network Audio Driver" implementation plan
- docs/orp/README.md listed ORP071 as a complete planning document for AES67 integration
- Wireframe documents described network audio as a planned SDK feature
- docs/early_marketing/orp_st2110int.md provided detailed ST 2110 integration guidance

**Reality:** The Orpheus SDK has no network audio protocol implementation and no active plans to build one.

## Technical Analysis

### What We Actually Support

**Current behavior:**

1. SDK uses `IAudioDriver` interface for all audio I/O
2. Driver implementations: CoreAudio (macOS), WASAPI (Windows, planned), ALSA (Linux, planned)
3. Any class-compliant audio device appears in the OS audio device list
4. User selects device via standard OS audio settings or application audio preferences

**Network audio devices that work today:**

- Dante Virtual Soundcard (appears as "Dante Virtual Soundcard" in CoreAudio/WASAPI)
- Dante hardware interfaces (via USB/Thunderbolt, appear as audio devices)
- AES67 devices with manufacturer-provided drivers (Lawo, Merging, etc.)
- Ravenna interfaces (same mechanism)

**What we're NOT doing:**

- ❌ Implementing RTP audio transport (RFC 3550)
- ❌ PTP clock synchronization (IEEE 1588)
- ❌ SAP/SDP session discovery
- ❌ Direct network socket management for audio streams
- ❌ AES67-specific stream management

### Why This Matters

**Marketing vs. Reality:**

- **Claim:** "Network Audio (AES67/Dante support)"
- **Reality:** "Works with network audio devices via OS drivers"

**Effort estimate for real implementation:**

- RTP transport layer: 800-1,200 LOC
- PTP synchronization: 400-600 LOC
- Session discovery (SAP/SDP): 300-500 LOC
- Network jitter buffering: 200-400 LOC
- Integration testing: 2-4 weeks
- **Total:** 6-12 months for production-ready implementation

**Alternative approaches:**

1. **Current (honest):** Support via manufacturer drivers (0 LOC, works now)
2. **Enterprise:** License Dante SDK from Audinate ($10k-50k + royalties)
3. **Moonshot:** Build full AES67 stack (6-12 months, ongoing maintenance)

## Changes Made

### Files Modified

**1. ARCHITECTURE.md**

- **Removed:** "Network Audio (AES67/RTP)" section (lines 604-612)
- **Impact:** Eliminates false roadmap promise

**2. ROADMAP.md**

- **Removed:** "AES67 Network Audio Driver (Optional)" section with implementation details
- **Removed:** Reference to ORP071 in documentation links
- **Added:** Clarification that network audio devices work via manufacturer-provided drivers
- **Impact:** Honest about what we support

**3. docs/orp/README.md**

- **Removed:** ORP071 section describing AES67 driver integration plan
- **Impact:** No longer advertising exploratory research as active planning

**4. wireframes/v2025-11-08/architecture-overview.notes.md**

- **Removed:** "Network Audio (AES67/RTP)" from planned features list
- **Removed:** "Network Audio (v1.0)" section from future architecture
- **Impact:** Wireframes match actual capabilities

### Files NOT Modified

**Archived documents:**

- `docs/orp/archive/ORP072 AES67 Network Audio Driver.md` - Left as-is (archived)
- `docs/orp/archive/ORP081.md` - Contains AES67 references but archived
- `docs/orp/archive/ORP070B Progress Tracker.md` - Historical, archived

**Early marketing/research:**

- `docs/early_marketing/orp_st2110int.md` - Exploratory research document
- `docs/early_marketing/orp_timeMgmt_v1.md` - Contains network audio claims but marked as early marketing

**Rationale:** These documents represent historical exploration or are clearly marked as non-authoritative. Archiving them preserves research context without implying active development.

## What We Should Say Instead

### Recommended Language

**Instead of:**

> "Network Audio (AES67/Dante support)"

**Use:**

> "Compatible with network audio devices (Dante Virtual Soundcard, AES67 interfaces with manufacturer-provided drivers)"

**In technical documentation:**

```markdown
## Audio I/O Support

### Platform Drivers

- **macOS:** CoreAudio (all class-compliant devices)
- **Windows:** WASAPI (all class-compliant devices), ASIO (planned)
- **Linux:** ALSA (planned)

### Network Audio Compatibility

Orpheus SDK works with network audio systems that present as OS-level audio devices:

- Dante Virtual Soundcard (DVS)
- Dante hardware interfaces (via USB/Thunderbolt)
- AES67 devices with driver software (Lawo, Merging, etc.)
- Ravenna interfaces with manufacturer drivers

**Note:** Orpheus does not implement native AES67/Dante protocols.
Use manufacturer-provided drivers or virtual soundcard software for
network audio routing.
```

## Impact Assessment

### Documentation Accuracy

- ✅ No longer claiming unimplemented features
- ✅ Clear about actual capabilities
- ✅ Honest about how network audio works

### Development Scope

- ✅ Removes 6-12 month feature from implicit roadmap
- ✅ Focuses team on core audio SDK (transport, routing, DSP)
- ✅ No user-facing functionality lost (it never existed)

### User Expectations

- ✅ Users understand what "network audio support" actually means
- ✅ Clear guidance on using Dante/AES67 devices
- ✅ No surprise when users discover we don't have native protocol support

## Future Considerations

**If we ever decide to build native AES67 support:**

1. **Create new ORP document** (e.g., ORP150+) with full technical plan
2. **Requirements analysis:**
   - Target use case (broadcast automation, live sound, etc.)
   - Dante SDK licensing vs. from-scratch implementation
   - Performance requirements (latency, channel count)
   - Platform support (macOS/Windows/Linux)
3. **Prototype phase:** 1-2 months to validate approach
4. **Production implementation:** 6-12 months
5. **Maintenance:** Ongoing support for network protocol changes

**Current recommendation:** Don't build it. Dante DVS solves 95% of use cases, and enterprise users can license Dante SDK if needed.

## Verification

### Claims Removed

- ✅ "AES67/RTP for audio-over-IP"
- ✅ "PTP (IEEE 1588) for clock sync"
- ✅ "Interoperability with Dante, Ravenna, Q-LAN"
- ✅ "Network Audio Driver (9-day implementation)"
- ✅ "RTP transport layer (RFC 3550)"
- ✅ "Up to 128 network input channels"

### Remaining References

```bash
$ rg -i '(aes67|dante|network audio)' --type md docs/ wireframes/ \
    --glob '!**/archive/**' --glob '!docs/early_marketing/**'
```

**Expected results:**

- `ROADMAP.md`: Clarification about manufacturer-provided drivers ✅
- `routing_matrix.h`: Dante Controller mentioned as design inspiration (not a claim) ✅
- Archived/early marketing docs excluded from search ✅

## Lessons Learned

1. **Don't claim features before implementation** - Research ≠ Roadmap
2. **Archive vs. Delete** - Preserve research, remove from active docs
3. **Marketing language matters** - "Supports" vs. "Compatible with" has different meanings
4. **Scope creep indicators** - 9-day estimate for 6-month project = red flag
5. **Honest documentation builds trust** - Users prefer truth over aspirations

## Related Documents

- `ARCHITECTURE.md` - Updated to remove network audio claims
- `ROADMAP.md` - Updated to clarify driver-based compatibility
- `docs/orp/README.md` - Updated to remove ORP071 references
- `wireframes/v2025-11-08/architecture-overview.notes.md` - Updated to match reality

## Commit Information

**Files changed:** 4
**Lines removed:** ~50
**Lines added:** ~5 (clarification in ROADMAP.md)
**Net change:** -45 lines of misleading documentation

---

**Document Status:** Complete
**Maintained By:** SDK Core Team
**Next Review:** If considering network audio protocol implementation
