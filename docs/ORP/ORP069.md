# ORP069 Implementation Plan v1.0: OCC-Aligned SDK Enhancements

**Version:** 1.0
**Date:** October 12, 2025
**Status:** Authoritative
**Counterpart:** ORP068 (SDK Integration Plan v2.0)
**References:** OCC029 (SDK Enhancement Recommendations), OCC030 (SDK Status Report)

---

## Document Purpose

This document defines the implementation plan for Orpheus SDK enhancements required to support **Orpheus Clip Composer (OCC) MVP** and future applications requiring real-time audio playback.

**Relationship to ORP068:**
- **ORP068** = SDK integration infrastructure (drivers, contracts, client architecture)
- **ORP069** = SDK core audio capabilities (routing, performance monitoring, platform drivers)
- Both plans run in parallel, coordinated timelines

**Scope:** Months 1-6 of OCC MVP development (aligned with OCC029 recommendations)

---

## Executive Summary

### Current Status (Oct 12, 2025)

**SDK Readiness:** 90% complete for OCC MVP (3 of 5 critical modules implemented)

**Already Implemented (Oct 2025):**
- ✅ `ITransportController` - Real-time clip playback with sample-accurate timing
- ✅ `IAudioFileReader` - Audio file decoding (WAV/AIFF/FLAC via libsndfile)
- ✅ `IAudioDriver` - Abstract interface + dummy driver (testing)

**Remaining Work (Months 1-6):**
- ⏳ Platform audio drivers (CoreAudio, WASAPI) - Months 1-2
- ⏳ `IRoutingMatrix` - Multi-channel routing (4 Clip Groups) - Months 3-4
- ⏳ `IPerformanceMonitor` - CPU/latency diagnostics - Months 4-5
- ⏳ ASIO driver (Windows professional) - Month 5 (optional)

### Success Criteria

**By End of Month 2:**
- ✅ CoreAudio driver (macOS) with latency <10ms
- ✅ WASAPI driver (Windows) with latency <10ms
- ✅ OCC playing real audio on both platforms

**By End of Month 4:**
- ✅ Routing matrix complete (4 Clip Groups → Master)
- ✅ 16 simultaneous clips tested
- ✅ CPU usage <30% with 16 active clips

**By End of Month 6:**
- ✅ Performance monitor complete
- ✅ OCC MVP beta (10 users)
- ✅ 24-hour stability tests passing

---

## Phase Breakdown

### Phase 1 (Months 1-2): Platform Audio Drivers [CRITICAL]

**Objective:** Enable real audio output on macOS and Windows

#### P1.AUDIO.001: CoreAudio Driver (macOS) [CRITICAL]
**Owner:** SDK Team
**Timeline:** Weeks 1-4
**Estimated Effort:** 3 days implementation + 2 days testing

**Tasks:**
1. Create `src/platform/audio_drivers/coreaudio/coreaudio_driver.{h,cpp}`
2. Implement `IAudioDriver` interface using AudioUnit API
3. Device enumeration via `AudioObjectGetPropertyData`
4. Buffer size configuration (default: 512 samples = 10.7ms @ 48kHz)
5. Automatic sample rate conversion if needed
6. Unit tests (GoogleTest)
7. Integration test: Verify latency <10ms

**Acceptance Criteria:**
- [ ] Compiles on macOS (x86_64 + arm64)
- [ ] Enumerates available audio devices
- [ ] Starts/stops audio stream without crashes
- [ ] Latency <10ms measured via `getLatencySamples()`
- [ ] Audio callback invoked at correct sample rate
- [ ] Unit tests pass (device enumeration, start/stop, latency)
- [ ] Integration test: Play 1-second sine wave, verify output

**Dependencies:** None (CoreAudio is built into macOS)

**Blockers:** None

---

#### P1.AUDIO.002: WASAPI Driver (Windows) [CRITICAL]
**Owner:** SDK Team
**Timeline:** Weeks 3-6
**Estimated Effort:** 4 days implementation + 2 days testing

**Tasks:**
1. Create `src/platform/audio_drivers/wasapi/wasapi_driver.{h,cpp}`
2. Implement `IAudioDriver` interface using WASAPI Exclusive Mode
3. Shared Mode fallback (higher latency, universal compatibility)
4. Device enumeration via `IMMDeviceEnumerator`
5. Buffer size configuration (default: 480 samples = 10ms @ 48kHz)
6. Handle device hot-plug/unplug gracefully
7. Unit tests (GoogleTest)
8. Integration test: Verify latency <10ms (Exclusive Mode)

**Acceptance Criteria:**
- [ ] Compiles on Windows (MSVC x64)
- [ ] Enumerates available audio devices
- [ ] Starts/stops audio stream without crashes
- [ ] Latency <10ms (Exclusive Mode) or <20ms (Shared Mode)
- [ ] Audio callback invoked at correct sample rate
- [ ] Unit tests pass (device enumeration, start/stop, latency)
- [ ] Integration test: Play 1-second sine wave, verify output
- [ ] Gracefully falls back to Shared Mode if Exclusive unavailable

**Dependencies:** Windows 10+ (WASAPI is built into Windows)

**Blockers:** None

---

#### P1.AUDIO.003: Platform Driver Factory [CRITICAL]
**Owner:** SDK Team
**Timeline:** Week 6
**Estimated Effort:** 1 day

**Tasks:**
1. Create `src/platform/audio_drivers/audio_driver_factory.{h,cpp}`
2. Implement platform detection (macOS → CoreAudio, Windows → WASAPI)
3. Factory function: `createPlatformAudioDriver()`
4. Update `IAudioDriver` documentation with platform notes

**Acceptance Criteria:**
- [ ] `createPlatformAudioDriver()` returns correct driver on each platform
- [ ] Compiles on macOS, Windows, Linux (dummy driver fallback on Linux)
- [ ] Documentation updated in `include/orpheus/audio_driver.h`

**Dependencies:** P1.AUDIO.001 (CoreAudio), P1.AUDIO.002 (WASAPI)

**Blockers:** None

---

#### P1.AUDIO.004: CMake Build Integration [CRITICAL]
**Owner:** SDK Team
**Timeline:** Week 6
**Estimated Effort:** 1 day

**Tasks:**
1. Add CMake options:
   - `ORPHEUS_ENABLE_COREAUDIO` (ON by default on macOS)
   - `ORPHEUS_ENABLE_WASAPI` (ON by default on Windows)
2. Link CoreAudio framework on macOS (`target_link_libraries(... "-framework CoreAudio")`)
3. Link mmdevapi.lib on Windows (`target_link_libraries(... "mmdevapi.lib")`)
4. Update `CMakeLists.txt` in `src/platform/audio_drivers/`

**Acceptance Criteria:**
- [ ] CMake builds CoreAudio driver on macOS
- [ ] CMake builds WASAPI driver on Windows
- [ ] CMake builds dummy driver on Linux
- [ ] CI builds pass on all platforms

**Dependencies:** P1.AUDIO.001, P1.AUDIO.002, P1.AUDIO.003

**Blockers:** None

---

#### P1.DOC.001: OCC Integration Guide [HIGH]
**Owner:** SDK Team
**Timeline:** Week 8
**Estimated Effort:** 2 days

**Tasks:**
1. Create `docs/integration/OCC_SDK_Integration_Guide.md`
2. Document initialization sequence (audio driver → transport → routing)
3. Threading model (UI thread vs audio thread)
4. Error handling best practices
5. Example: Complete clip playback workflow
6. Platform-specific notes (CoreAudio on macOS, WASAPI on Windows)

**Acceptance Criteria:**
- [ ] Documentation complete and reviewed
- [ ] Code examples compile and run
- [ ] OCC team confirms guide is clear

**Dependencies:** P1.AUDIO.001, P1.AUDIO.002, P1.AUDIO.003

**Blockers:** None

---

#### P1.TEST.001: Phase 1 Validation Checkpoint [CRITICAL]
**Owner:** SDK Team
**Timeline:** Week 8 (End of Month 2)
**Estimated Effort:** 1 day

**Tasks:**
1. Run full validation suite:
   - All existing tests pass (transport, audio file reader, dummy driver)
   - CoreAudio tests pass (macOS)
   - WASAPI tests pass (Windows)
2. Integration test: OCC plays audio on both platforms
3. Performance test: Measure latency on reference hardware
4. Update `.claude/progress.md` with Phase 1 completion

**Acceptance Criteria:**
- [ ] All unit tests pass (C++ GoogleTest)
- [ ] Integration test: 1-second sine wave plays correctly
- [ ] Latency <10ms verified on macOS (CoreAudio)
- [ ] Latency <10ms verified on Windows (WASAPI Exclusive Mode)
- [ ] OCC team confirms real audio output works

**Dependencies:** All Phase 1 tasks

**Blockers:** None

---

### Phase 2 (Months 3-4): Routing Matrix [CRITICAL]

**Objective:** Enable multi-channel routing (4 Clip Groups → Master Output)

#### P2.ROUTE.001: Routing Matrix Interface [CRITICAL]
**Owner:** SDK Team
**Timeline:** Week 9-10
**Estimated Effort:** 2 days

**Tasks:**
1. Create `include/orpheus/routing_matrix.h` (matches OCC027 spec)
2. Define `IRoutingMatrix` interface:
   - 4 Clip Groups with independent busses
   - Master output bus (stereo default, configurable channels)
   - Per-clip routing to group
   - Per-group routing to master
   - Real-time gain control (smooth, no clicks/pops)
   - Mute/solo per group
   - Master gain/mute control
3. Add Doxygen comments
4. Review with OCC team for API compatibility

**Acceptance Criteria:**
- [ ] Header compiles on all platforms
- [ ] API matches OCC027 specification
- [ ] Doxygen comments complete
- [ ] OCC team approves interface

**Dependencies:** None (interface definition only)

**Blockers:** None

---

#### P2.ROUTE.002: Routing Matrix Implementation [CRITICAL]
**Owner:** SDK Team
**Timeline:** Week 11-13
**Estimated Effort:** 5 days implementation + 2 days testing

**Tasks:**
1. Create `src/core/routing/routing_matrix.{h,cpp}`
2. Implement `IRoutingMatrix` interface
3. Clip → Group routing (track which clips belong to which groups)
4. Group → Master summing (double-precision accumulation, float output)
5. Gain smoothing (10ms ramp to avoid clicks/pops)
6. Lock-free atomic operations for gain/mute state
7. Optional soft-clip limiter on master output
8. Unit tests (GoogleTest)
9. Audio tests (verify no clicks/pops on gain changes)

**Acceptance Criteria:**
- [ ] Compiles on all platforms
- [ ] Clip-to-group assignment works correctly
- [ ] Group-to-master routing works correctly
- [ ] Gain changes are smooth (no audible clicks)
- [ ] Mute/solo work correctly
- [ ] Unit tests pass (routing correctness)
- [ ] Audio tests pass (no artifacts)

**Dependencies:** P2.ROUTE.001

**Blockers:** None

---

#### P2.ROUTE.003: Integration with Transport Controller [CRITICAL]
**Owner:** SDK Team
**Timeline:** Week 14
**Estimated Effort:** 2 days

**Tasks:**
1. Update `ITransportController` to use `IRoutingMatrix`
2. Integrate routing into audio callback:
   - Fetch active clips
   - Route each clip to its assigned group
   - Sum groups to master output
3. Performance optimization (minimize CPU usage)
4. Integration tests (16 simultaneous clips)

**Acceptance Criteria:**
- [ ] Transport controller routes audio through routing matrix
- [ ] 16 simultaneous clips play correctly
- [ ] CPU usage <30% with 16 active clips
- [ ] No audio dropouts or buffer underruns
- [ ] Integration tests pass

**Dependencies:** P2.ROUTE.002, existing `ITransportController`

**Blockers:** None

---

#### P2.ROUTE.004: CMake and CI Integration [HIGH]
**Owner:** SDK Team
**Timeline:** Week 14
**Estimated Effort:** 1 day

**Tasks:**
1. Add `orpheus_routing` library to CMake
2. Link against `orpheus_session` and `orpheus_transport`
3. Update CI to build and test routing module
4. Update documentation in `docs/ARCHITECTURE.md`

**Acceptance Criteria:**
- [ ] CMake builds routing module on all platforms
- [ ] CI tests pass on all platforms
- [ ] Documentation updated

**Dependencies:** P2.ROUTE.002, P2.ROUTE.003

**Blockers:** None

---

#### P2.TEST.001: Phase 2 Validation Checkpoint [CRITICAL]
**Owner:** SDK Team
**Timeline:** Week 16 (End of Month 4)
**Estimated Effort:** 2 days

**Tasks:**
1. Run full validation suite:
   - All Phase 1 tests pass
   - Routing matrix tests pass
   - Integration test: 16 simultaneous clips
2. Performance test: CPU usage <30% with 16 clips
3. Stress test: Rapid gain changes (no clicks/pops)
4. OCC integration test: Clip Groups UI works correctly
5. Update `.claude/progress.md` with Phase 2 completion

**Acceptance Criteria:**
- [ ] All unit tests pass (C++ GoogleTest)
- [ ] Integration test: 16 clips play with routing
- [ ] CPU usage <30% measured on reference hardware
- [ ] No audible artifacts (clicks, pops, dropouts)
- [ ] OCC team confirms Clip Groups work correctly

**Dependencies:** All Phase 2 tasks

**Blockers:** None

---

### Phase 3 (Months 5-6): Performance Monitor & Polish [HIGH]

**Objective:** Enable diagnostics for OCC beta testing and optimization

#### P3.PERF.001: Performance Monitor Interface [HIGH]
**Owner:** SDK Team
**Timeline:** Week 17-18
**Estimated Effort:** 2 days

**Tasks:**
1. Create `include/orpheus/performance_monitor.h`
2. Define `IPerformanceMonitor` interface:
   - CPU usage tracking (audio thread percentage)
   - Buffer underrun detection (dropout counting)
   - End-to-end latency reporting
   - Memory usage tracking (debug builds)
   - Per-clip CPU breakdown (optional, profiling mode)
3. Add Doxygen comments
4. Review with OCC team for API compatibility

**Acceptance Criteria:**
- [ ] Header compiles on all platforms
- [ ] API matches OCC029 recommendations
- [ ] Doxygen comments complete
- [ ] OCC team approves interface

**Dependencies:** None (interface definition only)

**Blockers:** None

---

#### P3.PERF.002: Performance Monitor Implementation [HIGH]
**Owner:** SDK Team
**Timeline:** Week 19-21
**Estimated Effort:** 4 days implementation + 2 days testing

**Tasks:**
1. Create `src/core/diagnostics/performance_monitor.{h,cpp}`
2. Implement `IPerformanceMonitor` interface
3. CPU measurement: High-resolution timer (RDTSC on x86, Mach Absolute Time on macOS)
4. Buffer underrun detection: Track missed audio callbacks
5. Latency calculation: Input + processing + output
6. Memory tracking: Hook into allocators (debug builds only)
7. Minimal overhead: <0.5% CPU impact when profiling disabled
8. Unit tests (GoogleTest)
9. Stress tests (induce underruns by overloading CPU, verify detection)

**Acceptance Criteria:**
- [ ] Compiles on all platforms
- [ ] CPU usage calculation is accurate (verified against system monitor)
- [ ] Buffer underrun detection works correctly
- [ ] Latency reporting matches driver latency
- [ ] Profiling overhead <0.5% CPU
- [ ] Unit tests pass
- [ ] Stress tests pass (underrun detection)

**Dependencies:** P3.PERF.001

**Blockers:** None

---

#### P3.PERF.003: Integration with OCC UI [HIGH]
**Owner:** SDK Team (coordinate with OCC team)
**Timeline:** Week 22
**Estimated Effort:** 2 days

**Tasks:**
1. Provide example UI code for OCC integration:
   - CPU meter (visual bar, percentage text)
   - Latency display (ms)
   - Buffer underrun indicator (flashing red icon)
2. Export metrics to CSV for diagnostics
3. Update OCC Integration Guide with performance monitoring examples

**Acceptance Criteria:**
- [ ] Example UI code compiles in JUCE
- [ ] OCC team confirms examples are useful
- [ ] Documentation updated

**Dependencies:** P3.PERF.002

**Blockers:** OCC UI framework implementation

---

#### P3.AUDIO.005: ASIO Driver (Windows Professional) [MEDIUM]
**Owner:** SDK Team
**Timeline:** Week 20-22
**Estimated Effort:** 3 days implementation + 2 days testing

**Tasks:**
1. Create `src/platform/audio_drivers/asio/asio_driver.{h,cpp}`
2. Implement `IAudioDriver` interface using ASIO SDK
3. User must provide ASIO SDK path (CMake flag: `ASIO_SDK_PATH`)
4. Default buffer size: 256 samples (5.3ms @ 48kHz)
5. Device enumeration via `ASIOInit()`, `ASIOGetChannels()`
6. Unit tests (GoogleTest)
7. Integration test: Verify latency <5ms

**Acceptance Criteria:**
- [ ] Compiles on Windows when `ASIO_SDK_PATH` provided
- [ ] Enumerates ASIO devices (if available)
- [ ] Starts/stops audio stream without crashes
- [ ] Latency <5ms measured via `getLatencySamples()`
- [ ] Falls back to WASAPI if no ASIO drivers installed
- [ ] Unit tests pass
- [ ] Integration test: Play 1-second sine wave, verify output

**Dependencies:** P1.AUDIO.002 (WASAPI fallback)

**Blockers:** User must provide ASIO SDK (cannot redistribute)

**Note:** Optional - WASAPI is sufficient for OCC MVP. ASIO is for professional Windows users only.

---

#### P3.DOC.002: Platform-Specific Guides [MEDIUM]
**Owner:** SDK Team
**Timeline:** Week 23
**Estimated Effort:** 2 days

**Tasks:**
1. Create `docs/platform/CoreAudio_Integration.md`:
   - CoreAudio setup on macOS
   - Buffer size recommendations
   - Device enumeration
   - Troubleshooting (sample rate mismatches)
2. Create `docs/platform/ASIO_WASAPI_Integration.md`:
   - ASIO SDK installation on Windows
   - WASAPI Exclusive vs Shared Mode
   - Driver selection logic (ASIO if available, else WASAPI)
   - Troubleshooting ("No ASIO drivers found")

**Acceptance Criteria:**
- [ ] Documentation complete and reviewed
- [ ] Platform-specific examples compile and run
- [ ] OCC team confirms guides are clear

**Dependencies:** P1.AUDIO.001 (CoreAudio), P1.AUDIO.002 (WASAPI), P3.AUDIO.005 (ASIO)

**Blockers:** None

---

#### P3.TEST.001: 24-Hour Stability Test [HIGH]
**Owner:** SDK Team
**Timeline:** Week 24
**Estimated Effort:** 3 days (mostly automated)

**Tasks:**
1. Create stability test harness:
   - Run minhost adapter with 16 clips looping
   - Monitor for crashes, memory leaks, buffer underruns
2. Run test for 24 hours on macOS and Windows
3. Analyze results:
   - Memory usage should remain constant (no leaks)
   - Buffer underruns should be zero
   - No crashes or hangs
4. Target: >100 hours MTBF (Mean Time Between Failures)

**Acceptance Criteria:**
- [ ] Test runs for 24 hours without crashes
- [ ] No memory leaks detected (verified with sanitizers)
- [ ] Buffer underruns: 0
- [ ] CPU usage remains stable over time
- [ ] MTBF >100 hours projected

**Dependencies:** All Phase 3 tasks

**Blockers:** None (can run in background)

---

#### P3.TEST.002: Phase 3 Validation Checkpoint [HIGH]
**Owner:** SDK Team
**Timeline:** Week 24 (End of Month 6)
**Estimated Effort:** 1 day

**Tasks:**
1. Run full validation suite:
   - All Phase 1 tests pass
   - All Phase 2 tests pass
   - Performance monitor tests pass
   - ASIO tests pass (Windows, if applicable)
2. OCC MVP beta readiness check:
   - 10 users can install and run OCC
   - All critical features work (playback, routing, diagnostics)
   - No show-stopper bugs
3. Update `.claude/progress.md` with Phase 3 completion

**Acceptance Criteria:**
- [ ] All unit tests pass (C++ GoogleTest)
- [ ] 24-hour stability test passes
- [ ] OCC MVP beta deployed to 10 users
- [ ] OCC team confirms SDK is production-ready

**Dependencies:** All Phase 3 tasks

**Blockers:** OCC team readiness for beta deployment

---

## Timeline Summary

| Phase | Duration | Critical Tasks | Deliverable |
|-------|----------|----------------|-------------|
| **Phase 1** | Months 1-2 (Weeks 1-8) | CoreAudio, WASAPI, Integration Guide | Real audio output on macOS + Windows |
| **Phase 2** | Months 3-4 (Weeks 9-16) | Routing Matrix, 16-clip integration | 4 Clip Groups → Master routing |
| **Phase 3** | Months 5-6 (Weeks 17-24) | Performance Monitor, ASIO, Stability | Diagnostics, 24-hour stability |

**Parallel Work:**
- ORP068 continues in parallel (Phase 2: Adapter Development, Phase 3: Testing Infrastructure)
- OCC development proceeds alongside SDK work (OCC can start integration in Month 1)

---

## Risk Management

### Technical Risks

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| Buffer underruns on low-end hardware | Medium | High | Adaptive buffer sizing, performance profiling, test on reference hardware |
| Cross-platform audio driver issues | Medium | High | Dummy driver for testing, extensive platform testing, CI on macOS + Windows |
| Sample-accurate timing drift | Low | High | Already verified (±1 sample tolerance), continue testing in integration |
| ASIO SDK redistribution restrictions | High | Low | User must download separately, document clearly, WASAPI fallback |

### Schedule Risks

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| CoreAudio/WASAPI complexity | Low | High | Start early (Week 1), dummy driver proves architecture works |
| Routing matrix CPU overhead | Medium | High | Continuous profiling, optimize gain smoothing, double-precision accumulation |
| ASIO integration complexity | High | Low | Deferred to Month 5 (not blocking MVP), WASAPI is sufficient |
| 24-hour stability test failures | Medium | Medium | Address issues immediately, retest, delay beta if critical |

### Dependency Risks

| Dependency | Risk | Mitigation |
|------------|------|------------|
| libsndfile (LGPL) | License compliance | Already resolved (dynamic linking, LGPL-compliant) |
| ASIO SDK (proprietary) | User must download | Document clearly, provide WASAPI fallback |
| OCC team readiness | Integration delays | Weekly sync meetings, shared integration tests, tag GitHub issues `occ-blocker` |

---

## Success Metrics

### Phase 1 Success (End of Month 2)
- ✅ CoreAudio driver: Latency <10ms on macOS
- ✅ WASAPI driver: Latency <10ms on Windows (Exclusive Mode)
- ✅ OCC plays real audio on both platforms
- ✅ Integration guide complete and reviewed by OCC team

### Phase 2 Success (End of Month 4)
- ✅ Routing matrix: 4 Clip Groups → Master Output
- ✅ 16 simultaneous clips tested
- ✅ CPU usage <30% with 16 active clips
- ✅ No audible artifacts (clicks, pops, dropouts)
- ✅ OCC Clip Groups UI works correctly

### Phase 3 Success (End of Month 6)
- ✅ Performance monitor: CPU, latency, underruns tracked
- ✅ ASIO driver: Latency <5ms on Windows (optional)
- ✅ 24-hour stability test passes (no crashes, no leaks)
- ✅ OCC MVP beta deployed to 10 users
- ✅ SDK is production-ready for OCC v1.0

---

## Coordination with ORP068

**ORP068 (SDK Integration Infrastructure)** and **ORP069 (OCC-Aligned SDK Enhancements)** are **counterpart plans** that run in parallel.

### Shared Timeline

| Month | ORP068 Focus | ORP069 Focus | Coordination |
|-------|--------------|--------------|--------------|
| **1-2** | Driver architecture (Service, Native, Client, React) | Platform audio drivers (CoreAudio, WASAPI) | Weekly sync, shared CI |
| **3-4** | Adapter development (REAPER, standalone, plugin hosts) | Routing matrix implementation | Shared integration tests |
| **5-6** | Testing infrastructure, CI hardening | Performance monitor, stability testing | Joint validation checkpoints |

### Shared Deliverables

- **CI Infrastructure:** Both plans use same GitHub Actions, sanitizers, cross-platform builds
- **Documentation:** Architecture docs, integration guides, API references
- **Validation Scripts:** `./scripts/validate-phase{1,2,3}.sh` used by both plans

### Communication

- **Weekly Sync Meetings:** Fridays, 30 minutes (SDK team + OCC team)
- **Shared Slack Channel:** `#orpheus-sdk-integration`
- **GitHub Issues:** Tag with `orp068`, `orp069`, or `occ-blocker` as appropriate

---

## Appendix: Task Reference

### All ORP069 Tasks (18 Tasks Total)

**Phase 1 (6 tasks):**
- P1.AUDIO.001: CoreAudio Driver (macOS)
- P1.AUDIO.002: WASAPI Driver (Windows)
- P1.AUDIO.003: Platform Driver Factory
- P1.AUDIO.004: CMake Build Integration
- P1.DOC.001: OCC Integration Guide
- P1.TEST.001: Phase 1 Validation Checkpoint

**Phase 2 (5 tasks):**
- P2.ROUTE.001: Routing Matrix Interface
- P2.ROUTE.002: Routing Matrix Implementation
- P2.ROUTE.003: Integration with Transport Controller
- P2.ROUTE.004: CMake and CI Integration
- P2.TEST.001: Phase 2 Validation Checkpoint

**Phase 3 (7 tasks):**
- P3.PERF.001: Performance Monitor Interface
- P3.PERF.002: Performance Monitor Implementation
- P3.PERF.003: Integration with OCC UI
- P3.AUDIO.005: ASIO Driver (Windows Professional)
- P3.DOC.002: Platform-Specific Guides
- P3.TEST.001: 24-Hour Stability Test
- P3.TEST.002: Phase 3 Validation Checkpoint

**Total Effort Estimate:** ~40 days (8 weeks of full-time work, spread over 6 months)

---

## Document Maintenance

**Version History:**
- v1.0 (Oct 12, 2025) - Initial release, aligned with OCC029 analysis

**Review Schedule:**
- Weekly updates during active phases
- Monthly comprehensive review
- Final review at end of Month 6

**Stakeholders:**
- SDK Team (implementation)
- OCC Team (integration, requirements)
- QA Team (testing, validation)

**Related Documents:**
- ORP068 (SDK Integration Plan v2.0) - Counterpart plan
- OCC029 (SDK Enhancement Recommendations) - Requirements source
- OCC030 (SDK Status Report) - Current status snapshot
- OCC027 (API Contracts) - Interface specifications

---

**Document Status:** Authoritative - Approved by SDK Team
**Next Review Date:** October 19, 2025 (Weekly sync)
**Approved By:** SDK Team, [Date]

---

**End of Document**
