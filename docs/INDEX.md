# Orpheus SDK Documentation Index

> Home › Documentation › Index

## 🚀 Start Here

**New to Orpheus SDK?** Begin with these:

1. **[Repository README](../README.md)** – Build the SDK and run the tests in minutes
2. **[Architecture Overview](../ARCHITECTURE.md)** – Layers, transport, threading model
3. **[Roadmap](../ROADMAP.md)** – Milestones and the ORP132 hardening program
4. **[API Surface Index](API_SURFACE_INDEX.md)** – Catalog of public C++ headers

**For specific tasks:**

- Realtime safety rules → [Realtime Audio Audit](REALTIME_AUDIT.md)
- Position-tracking semantics (settled, ORP089) → [SDK Position Tracking](SDK_POSITION_TRACKING.md)
- App-side realtime debt guidance → [App Realtime Debt Remediation](APP_REALTIME_DEBT_REMEDIATION.md)
- Migrating old integrations → [Migration Guide](MIGRATION_v0_to_v1.md) (historical)

---

## ORP Implementation Plans & Technical Library

**📘 Current active program (2026): SDK Hardening & Platform Roadmap**

| Document | Role |
| -------- | ---- |
| **[ORP132 – Master Sprint Index](orp/ORP132%20SDK%20Hardening%20and%20Platform%20Roadmap%20-%20Master%20Sprint%20Index.md)** | Read first: scope, sequencing, boundaries |
| **[ORP133 – NOW Sprint](orp/ORP133%20NOW%20Sprint%20-%20Realtime%20Callback%20and%20Contract%20Truth.md)** | Realtime callback safety & contract truth |
| **[ORP134 – NEXT Sprint](orp/ORP134%20NEXT%20Sprint%20-%20Streaming%20Reader%20and%20Platform%20Primitives.md)** | Streaming reader & platform primitives |
| **[ORP135 – LATER Sprint](orp/ORP135%20LATER%20Sprint%20-%20Platform%20Leadership%20Bets.md)** | Speculative 2026+ platform bets |
| **[ORP136 – Verification & CI](orp/ORP136%20Verification%20and%20CI%20Framework.md)** | Cross-cutting verification framework |
| **[ORP150 – Atomic Clip-Group Choke Admission](orp/ORP150%20Atomic%20Clip-Group%20Choke%20Admission.md)** | Public one-command start/choke contract, failure atomicity, and package handoff |
| **[ORP154 – Sequencer Trigger Voice Primitive](orp/ORP154%20Sequencer%20Trigger%20Voice%20Primitive.md)** | Public RT-safe one-shot voice contract and FourTrack Seq handoff |
| **[ORP155 – Bounded Per-Clip DSP and Output Telemetry](orp/ORP155%20Bounded%20Per-Clip%20DSP%20and%20Output%20Telemetry.md)** | Validated fixed-order clip DSP, per-voice realtime state, and physical-output meters |

**📂 [ORP Directory Index](orp/README.md)** – Guide to all implementation plans
(ORP001–ORP136), including the completed ORP125/126/127 transport sprints and the
historical Shmui/TypeScript-era plans (ORP060–ORP068).

---

## Historical Records

These documents describe the repository as it was; paths and features they
mention may no longer exist in the tree:

- [SDK Team Handoff](SDK_TEAM_HANDOFF.md) | [Sprint Summary](SDK_SPRINT_SUMMARY.md) – ORP074 trim/fade metadata sprint records
- [Upgrading to 1.0](UPGRADING_TO_1.0.md) – TypeScript-driver-era upgrade guide
- [Migration v0 → v1](MIGRATION_v0_to_v1.md) – v1.0.0-rc.1-era migration guide
- [`orp/_process/archive/`](orp/_process/archive/) – archived process docs
  (GETTING_STARTED, DRIVER_ARCHITECTURE, CONTRACT_DEVELOPMENT, DECISION_PACKAGES, ADAPTERS, …)
- [`archive/`](archive/) – archived marketing/spec material and AGENTS.md

**Clip Composer (OCC) documentation** lives in the external
[`chrislyons/clip-composer`](https://github.com/chrislyons/clip-composer)
repository (`docs/occ/`) — extracted 2026-07-09, see
[ORP131](orp/ORP131%20Clip%20Composer%20Subdirectory%20Archival.md).

## Navigation

- ▲ [Back to Repository Overview](../README.md)
