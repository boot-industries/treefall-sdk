# Orpheus Reference Plans (ORP) - Index

This directory contains authoritative implementation plans for the Orpheus SDK ecosystem.

---

## Current Implementation Plans

### ORP070 - Orpheus Clip Composer MVP Sprint (Active Sprint)

**Status:** Authoritative (Active Execution)
**Timeline:** Months 1-6, 2025-2026
**Focus:** Complete SDK modules for OCC MVP (real-time audio infrastructure)

**Scope:**

- Phase 1: Platform audio drivers (CoreAudio, WASAPI) + real audio mixing (Months 1-2)
- Phase 2: Multi-channel routing matrix (4 Clip Groups → Master) (Months 3-4)
- Phase 3: Performance monitor, optimization, stability testing (Months 5-6)

**Key Deliverables:**

- CoreAudio driver (macOS, <10ms latency)
- WASAPI driver (Windows, <10ms latency)
- IRoutingMatrix (4 groups with gain smoothing)
- IPerformanceMonitor (CPU/latency diagnostics)
- Real audio mixing (16 clips, <30% CPU)
- 24-hour stability (>100hr MTBF)

**Progress Tracking:** See document for detailed phase breakdown
**Sprint Document:** `docs/ORP/ORP070 OCC MVP Sprint.md`

**Related Documents:**

- `apps/clip-composer/docs/OCC/OCC026.md` (MVP requirements)
- `apps/clip-composer/docs/OCC/OCC027.md` (API contracts)
- `apps/clip-composer/docs/OCC/OCC029.md` (SDK enhancements)
- `docs/ORP/ORP069.md` (parent plan)

---

### ORP068 - SDK Integration Plan v2.0 (Infrastructure)

**Status:** Active (Phase 3 Complete, Phase 4 In Progress)
**Timeline:** Months 1-6, 2025
**Focus:** Driver architecture, contract system, client integration

**Scope:**

- Phase 0: Repository consolidation (COMPLETE ✅)
- Phase 1: Driver development (Service, Native, WASM) (COMPLETE ✅)
- Phase 2: Expanded contract + UI components (COMPLETE ✅)
- Phase 3: Testing infrastructure and CI hardening (COMPLETE ✅)
- Phase 4: Documentation and productionization (IN PROGRESS ⏳)

**Key Deliverables:**

- @orpheus/contract (JSON schema system)
- @orpheus/engine-service (Node.js service driver)
- @orpheus/engine-native (N-API native driver)
- @orpheus/client (unified client broker)
- @orpheus/react (React integration hooks)

**Progress Tracking:** `.claude/progress.md`

**Related Documents:**

- `docs/ORP/ORP068.md` (full plan)
- `docs/ARCHITECTURE.md` (system architecture)
- `docs/DRIVER_INTEGRATION_GUIDE.md` (integration guide)
- `docs/CONTRACT_DEVELOPMENT.md` (contract guide)

---

### ORP069 - OCC-Aligned SDK Enhancements v1.0 (Core Audio)

**Status:** Superseded by ORP070 (use ORP070 for execution)
**Timeline:** Months 1-6, 2025
**Focus:** Platform audio drivers, routing, performance monitoring

**Scope:**

- Phase 1: Platform audio drivers (CoreAudio, WASAPI) - Months 1-2 ✅ **COMPLETE**
- Phase 2: Routing matrix (4 Clip Groups → Master) - Months 3-4 ✅ **COMPLETE**
- Phase 3: Performance monitor + ASIO + stability - Months 5-6 ⏳ In Progress

**Key Deliverables:**

- ITransportController (COMPLETE ✅)
- IAudioFileReader (COMPLETE ✅)
- IAudioDriver (CoreAudio ✅, WASAPI pending)
- IRoutingMatrix (COMPLETE ✅ - 64ch → 16 groups → 32 outputs)
- IPerformanceMonitor (Months 4-5 ⏳)

**Progress Tracking:** `.claude/orp070-progress.md` (Phase 1 & 2 complete)

**Related Documents:**

- `docs/ORP/ORP069.md` (full plan)
- `docs/ORP/ORP070 OCC MVP Sprint.md` (detailed sprint plan)
- `apps/clip-composer/docs/OCC/OCC029.md` (requirements)
- `apps/clip-composer/docs/OCC/OCC030.md` (current status)
- `apps/clip-composer/docs/OCC/OCC027.md` (interface specs)

---

### ORP071 - AES67 Network Audio Driver Integration (Optional)

**Status:** Planning Complete (Implementation Optional)
**Timeline:** 9 days (if prioritized)
**Focus:** Network audio infrastructure for professional installations

**Scope:**

- RTP transport layer (RFC 3550, L16/L24 payloads)
- PTP clock synchronization (IEEE 1588 PTP slave)
- AES67 driver implementation (IAudioDriver interface)
- Interoperability with Dante, Ravenna, Q-LAN

**Key Features:**

- Up to 128 network input channels (16 streams × 8 channels)
- Sample-accurate timing (PTP sync, ±4.8 samples @ 48kHz)
- Broadcast-safe design (zero allocations, packet loss concealment)
- SDP session management (manual configuration + future SAP discovery)

**Implementation Approach:** From scratch (MIT licensed, ~2,300 LOC)

**Progress Tracking:** Not yet started (awaiting priority decision)

**Related Documents:**

- `docs/ORP/ORP071.md` (full integration plan with IEEE citations)

---

## Relationship Between Plans

**ORP068** and **ORP069** are **counterpart plans** that run in parallel:

| Aspect                  | ORP068 (Infrastructure)                            | ORP069 (Core Audio)                        |
| ----------------------- | -------------------------------------------------- | ------------------------------------------ |
| **Focus**               | Driver architecture, contracts, client integration | Platform audio, routing, diagnostics       |
| **Timeline**            | Months 1-6 (Phases 0-4)                            | Months 1-6 (Phases 1-3)                    |
| **Primary Application** | All SDK consumers (REAPER, standalone, web)        | Orpheus Clip Composer (OCC)                |
| **Key Technologies**    | TypeScript, N-API, WebSocket, React                | C++20, CoreAudio, WASAPI, ASIO             |
| **Deliverables**        | @orpheus/\* npm packages                           | C++ SDK modules (transport, routing, etc.) |

**Coordination:**

- Weekly sync meetings (Fridays, 30 minutes)
- Shared CI infrastructure (GitHub Actions)
- Shared validation checkpoints (end of each phase)
- Joint documentation efforts

---

## Quick Reference

### For OCC Team (Clip Composer Integration)

**Start Here:**

1. Read `apps/clip-composer/docs/OCC/OCC030 SDK Status Report.md` (current status)
2. Review `docs/ORP/ORP069 Implementation Plan v1.0...md` (audio modules)
3. Check `apps/clip-composer/docs/OCC/OCC027 API Contracts.md` (interface specs)
4. Begin integration with existing modules (ITransportController, IAudioFileReader)

**What's Ready NOW:**

- ✅ ITransportController (real-time clip playback)
- ✅ IAudioFileReader (audio file decoding)
- ✅ IAudioDriver (dummy driver for testing)

**What's Coming:**

- ⏳ Platform drivers (Month 2)
- ⏳ Routing matrix (Months 3-4)
- ⏳ Performance monitor (Months 4-5)

### For REAPER Adapter Team

**Start Here:**

1. Read `docs/ORP/ORP068 Implementation Plan v2.0...md` (driver architecture)
2. Review `docs/DRIVER_INTEGRATION_GUIDE.md` (integration guide)
3. Check `docs/ADAPTERS.md` (adapter examples)

**What's Ready NOW:**

- ✅ @orpheus/contract (schema system)
- ✅ @orpheus/engine-service (service driver)
- ✅ @orpheus/engine-native (native driver)
- ✅ @orpheus/client (client broker)

**What's Coming:**

- ⏳ REAPER adapter (Phase 2)
- ⏳ Plugin host adapters (Phase 2)

### For SDK Core Team

**Active Tasks:**

- ORP068 Phase 2: Adapter development
- ORP069 Phase 1: Platform audio drivers (CoreAudio, WASAPI)

**Next Milestones:**

- End of Month 2: Platform drivers complete
- End of Month 4: Routing matrix complete
- End of Month 6: OCC MVP ready for beta

**Tracking:**

- Progress: `.claude/progress.md`
- Issues: GitHub with tags `orp068`, `orp069`, `occ-blocker`
- Slack: `#orpheus-sdk-integration`

---

## Document Conventions

### Naming Convention

**ORPxxx Format:**

- ORP001-ORP050: Reserved for strategic vision documents (not yet used)
- ORP051-ORP100: Implementation plans (ORP068, ORP069, etc.)
- ORP101+: Future expansion

### Status Values

- **Draft** - Work in progress, not yet reviewed
- **Under Review** - Ready for team review
- **Authoritative** - Approved, active implementation
- **Complete** - Implementation finished, archived for reference
- **Superseded** - Replaced by newer document

### Version Numbers

- **v1.0** - Initial release
- **v1.1, v1.2** - Minor updates (clarifications, corrections)
- **v2.0** - Major updates (scope changes, restructuring)

---

## Related Documentation

### SDK Architecture

- `docs/ARCHITECTURE.md` - System architecture overview
- `docs/AGENTS.md` - Agent/orchestration philosophy
- `docs/CLAUDE.md` - AI assistant guidelines

### Integration Guides

- `docs/DRIVER_INTEGRATION_GUIDE.md` - Driver integration guide
- `docs/CONTRACT_DEVELOPMENT.md` - Contract schema guide
- `docs/ADAPTERS.md` - Adapter development guide

### Application Design (OCC)

- `apps/clip-composer/docs/OCC/OCC021` - Product vision (authoritative)
- `apps/clip-composer/docs/OCC/OCC026` - MVP milestone definition
- `apps/clip-composer/docs/OCC/OCC027` - API contracts
- `apps/clip-composer/docs/OCC/OCC029` - SDK enhancement recommendations
- `apps/clip-composer/docs/OCC/OCC030` - SDK status report
- `apps/clip-composer/docs/OCC/CLAUDE.md` - OCC documentation governance

### Progress Tracking

- `.claude/progress.md` - Implementation progress (ORP068)
- `.claude/session-notes.md` - Session summaries

---

## Getting Help

**For Implementation Questions:**

- Post in Slack: `#orpheus-sdk-integration`
- Create GitHub issue with appropriate tag (`orp068`, `orp069`, `occ-blocker`)

**For Urgent Blockers:**

- Tag GitHub issue with `occ-blocker`
- Mention in weekly sync meeting
- Direct message SDK team lead

**For Documentation Issues:**

- Create GitHub issue with `documentation` tag
- Suggest edits via pull request

---

**Last Updated:** October 13, 2025
**Maintained By:** SDK Core Team
