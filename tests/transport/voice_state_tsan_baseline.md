# ORP127 T2 — ThreadSanitizer Baseline (pre-T3)

Recorded 2026-07-07 on the branch tip immediately after adding
`voice_state_tsan_test.cpp`, **before** the T3 command-queue refactor.

## How to reproduce

```bash
cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=Debug \
  -DORP_ENABLE_ASAN=OFF -DORP_ENABLE_UBSAN=OFF \
  -DCMAKE_CXX_FLAGS="-fsanitize=thread -g" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread"
cmake --build build-tsan --target voice_state_tsan_test
TSAN_OPTIONS="halt_on_error=0" \
  ./build-tsan/tests/transport/voice_state_tsan_test
```

## Baseline races (F-SDK-1)

The harness surfaces 9 distinct data-race sites in three families. All are
mutations of `ActiveClip` / `m_activeClips` / `m_activeClipCount` from the UI
thread that collide with audio-thread reads/writes in `processAudio` and its
`processCommands` sub-step:

| Family | UI-thread site | Audio-thread site | Field |
|--------|----------------|-------------------|-------|
| Direct mutation | `restartClip:1568` | `processAudio:328` | `isStopping` |
| Direct mutation | `seekClip:1628` | `processAudio:366` | `currentSample` |
| Query iteration | `getClipState:264,267` | `addActiveClip:823-826` | `handle` / `isStopping` / count |
| Query iteration | `getClipPosition:1321-1325` | `addActiveClip:823-826` | `handle` / `startSample` / `currentSample` |
| Voice compaction | `getClipPosition` read | `removeActiveVoice:884,906` | slot fields during compaction |

(Line numbers are pre-T3 and drift as the refactor lands.)

## Exit criterion (post-T3)

After T3 routes every UI-thread mutation through the command queue and serves
queries from atomically-published snapshots, this same harness must report
**zero** ThreadSanitizer warnings. That clean run is the T3 Definition-of-Done
gate for G1.
