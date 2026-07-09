<!-- SPDX-License-Identifier: MIT -->

# ORP137 — Hardening Program Completion & Downstream Follow-ups

**Document type:** Completion record + follow-on sprint scaffold
**Status:** ORP133 + ORP136 bootstrap + ORP134 EXECUTED (2026-07-09); follow-ups proposed
**Parent:** [[ORP132]] · **Executed plans:** [[ORP133]], [[ORP134]], [[ORP136]] (§5 items 1–2 + §2.1–2.3 gates)
**Deferred by design:** [[ORP135]] (directional bets — none implemented, per plan)

---

## 1. What landed (single branch, sequenced commits)

| Sprint | Delivered | Defining proof |
|--------|-----------|----------------|
| **ORP133 NOW** | POD `TransportEvent` ring (G1); `stopAllInGroup` deprecated → `NotSupported`, dead `StopGroup` arm deleted (G2); single-producer command contract documented + debug-asserted, all producers funneled through `postCommand()` (G3); version truth = **0.3.0** from CMake (G4); docs truth pass (G5); `tools/docs_path_audit.py` + ctest/CI gate (G6) | No `std::function` on any audio path; event-ordering test; producer death test green under TSAN |
| **ORP136 bootstrap** | Runtime realtime harness (`rt_guard.hpp` alloc hooks + `/proc/self/io`); golden transport render-hash gate; offline dither-seed determinism gate | Harness red/greens (detector self-test); render bit-identical across block sizes 256–2048 |
| **ORP134 NEXT** | **G1 streaming migration** (`clip_source.{h,cpp}`: `PreparedClipSource` + `StreamingClipSource` + `MediaStreamWorker`); G2 identity/time (`identity.h`, `time_domain.h`, `media_model.h`); G3 graph seam (`audio_graph.h`); G4 render-job determinism; G5 `IAudioFileWriter` (FTR007); G6 analysis facade (`audio_analysis.h`); G7 recorder plumbing (`audio_input.h`); G8 `ISceneManager::setRoutingMatrix` wiring + launcher-scale tests | `realtime_audit.py --fail-known-debt` **passes** (strict in CI); render-hash parity **bit-exact** vs the old reading path; file-backed callback: ~2,400 read syscalls → ~0 |

Full verification matrix: 132/132 ctest green (Debug + ASan/UBSan), transport/new suites green under ThreadSanitizer, clang-format-14 clean, docs-path gate green.

## 2. Reconciliation on record (ORP132 §3 rule)

[[ORP134]] names `--fail-known-debt --include-adjacent` as the finish line;
`REALTIME_AUDIT.md` (authoritative for contract facts) scopes the strict
cross-repo gate to "after the streaming-reader **and app telemetry**
migrations land". The SDK-side migration landed here and the SDK contributes
**zero** findings to the cross-repo run; the remaining `--include-adjacent`
findings are the two app repos' own debt (below). The in-repo strict gate
(`--fail-known-debt`) is CI-enforced now.

## 3. Downstream follow-up sprints (OUT of this repo — file in the app repos)

- **FourTrack (`~/dev/fourtrack`):** adopt SDK prepared/streaming sources in
  `Engine::processAudio()` (clears its two `readSamples` audit findings);
  delete local `WavWriter` for `IAudioFileWriter` (closes FTR007/FTR019);
  adopt `IAudioInputStream`/`AudioInputRing` for capture. Bump submodule pin.
- **Clip Composer (`~/dev/clip-composer`):** move in-callback analyzer work to
  a telemetry ring (clears its audit finding); wire
  `ISceneManager::setRoutingMatrix()` (now public) for tab/scene recall.
  Bump submodule pin.
- **FreqFinder (`~/dev/freqfinder`):** adopt `orpheus::analysis` (FFT/STFT,
  spectral features, onsets) and the graph `Tap` vocabulary.
- After both app realtime migrations land: run
  `realtime_audit.py --fail-known-debt --include-adjacent` as the standing
  cross-repo gate.

## 4. ORP135 bets — status of gating dependencies (nothing implemented)

- **B1 automation** — unblocked by G2 (`AutomationLaneId`, `TimePoint`); needs its own ORP doc.
- **B2 take/comping** — unblocked by G2 (`Take`, `MediaRegion`) + G5 + G7.
- **B3 spatial** — G3 seam exists; channel-layout governance still required.
- **B4 plugin processor API** — blocked on B1; `NodeKind::Processor` is reserved in the seam.
- **B5 MIDI/OSC + MPSC dispatcher** — G3 contract in place; dispatcher still future work.
- **B6/B7 ABI facades + migration tooling** — contracts newly settled; let them soak first.
- **B8/B9** — unchanged, dependency-gated.

Per [[ORP135]] §5, each bet still requires a concrete downstream pull and its
own ORP1NN doc before code starts. **Next free doc number: ORP138.**
