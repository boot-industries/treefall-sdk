<!-- SPDX-License-Identifier: MIT -->

# ORP140 — FreqFinder Architecture Revision: SDK Integration Confirmed

**Document type:** Correction record
**Status:** Supersedes [[archive/ORP107 FreqFinder Architecture Independence - ADR]] (2025-11-09)
**Supersedes:** [[archive/ORP107 FreqFinder Architecture Independence - ADR]] (now archived)
**Related:** [[ORP106 Wave Finder Architecture Assessment - JUCE vs SDK Integration]] · FreqFinder `docs/frq/FRQ032`, `docs/frq/FRQ033`

---

## 1. What changed

ORP107 (2025-11-09, *"FreqFinder Architecture Independence — ADR"*) recorded that
FreqFinder is a "JUCE-only" application with **NO Orpheus SDK integration**. That
conclusion is now **outdated and has been archived** at
`docs/orp/archive/ORP107 FreqFinder Architecture Independence - ADR.md`.

FreqFinder's requirements evolved past the 2025-11-09 assessment. It now depends on
**Orpheus SDK 0.3.1+** and consumes the SDK as a technical dependency via the
`Orpheus::core` and `Orpheus::audio_utils` targets.

## 2. Current confirmed FreqFinder SDK dependencies

- `orpheus::analysis` — the primary analysis facade: magnitude spectrum / FFT,
  spectral centroid, spectral rolloff, onset detection
- `orpheus::TransportController`
- `orpheus::RoutingMatrix`
- `orpheus::GainSmoother`
- `orpheus::TruePeakMeter`
- `orpheus::LoudnessMeter`
- `orpheus::IPerformanceMonitor`
- `orpheus::audio_graph` — via a FreqFinder-side `AnalysisTopology` module using
  `ConnectionKind::Tap` links

## 3. Source of truth going forward

FreqFinder's SDK relationship is owned and tracked in the FreqFinder repo:

- `docs/frq/FRQ032 Orpheus Analysis Facade Architecture Solidification.md`
  (status: Complete) — the analysis-facade integration.
- `docs/frq/FRQ033 Orpheus SDK Release Package Refresh for Analysis Facade.md`
  (status: Open) — the current SDK-team-facing packaging ask.

Those FRQ docs are authoritative for FreqFinder's SDK dependency surface. This
ORP doc exists only to retire the stale ORP107 claim, not to re-document the
integration.

## 4. ORP106 remains valid

[[ORP106 Wave Finder Architecture Assessment - JUCE vs SDK Integration]] is a
generic decision framework for evaluating JUCE-only vs JUCE+SDK for any app; it
is **still valid** and unchanged. Only its *specific application/conclusion for
FreqFinder in ORP107* was wrong — requirements evolved past the 2025-11-09
assessment. The framework itself was applied correctly against the information
available at the time; the inputs changed.
