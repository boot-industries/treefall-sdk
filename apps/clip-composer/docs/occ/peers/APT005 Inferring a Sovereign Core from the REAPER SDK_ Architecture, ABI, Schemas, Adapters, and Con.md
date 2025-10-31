# **PT005 Inferring a Sovereign Core from the REAPER SDK: Architecture, ABI, Schemas, Adapters, and Conformance**

## **Abstract**

This paper specifies a complete strategy for achieving **long-term sovereignty** for an audio SDK/DAW by **inferring a host-neutral core** while shipping today as a REAPER extension. We formalize a four-layer architecture (Adapters → Core Libraries → Interchange Models → Presentation Shells), define **stable C ABIs** for each domain, introduce neutral **data contracts** (Session Graph, BounceSpec, ReconformPlan, ADMDoc), and describe **adapter implementations** for REAPER now and a minimal JUCE host later. We add rigorous **conformance testing**, **performance budgets**, **undo atomicity**, **IPC fallbacks**, **licensing guardrails**, and a **five-milestone migration roadmap** to a standalone host. The approach is standards-first (OTIO, ADM, OSC) with optional plugin+device facades (VST3/CLAP/AU) kept thin behind adapters [1], [2], [3], [6], [7], [9], [11].

---

## **1. Problem Statement and Premise**

- **Constraint**: REAPER’s core is not open-source; only its **Extension SDK** is public [1], [8].
- **Goal**: Deliver professional features now (inside REAPER), while building a **portable core** that can power an independent DAW later.
- **Method**: Treat REAPER as a **disposable adapter**. Put durable logic into host-neutral libraries with **versioned C ABIs** and **open interchange** (OTIO, ADM, OSC) [6], [12], [11].
- **Plugin/Device ecosystems**: support via **optional adapters** (VST3 now; CLAP/LV2/AU later) to avoid vendor lock-in [2], [17], [25], [turn0search6], [turn0search18].

---

## **2. System Architecture (Four Layers)**

### **2.1 Host Adapters (thin, disposable)**

Responsibilities: call REAPER SDK functions, translate project state to/from core types, surface UI via ReaImGui panels, bridge transport/timeline/control surface events [1], [8].

Non-responsibilities: **no** business logic, **no** domain algorithms.

### **2.2 Core Domain Libraries (durable IP)**

- **/session**: timebase, tracks, buses, items, automation; JSON/FlatBuffers I/O.
- **/clipgrid**: non-linear slots/scenes; tempo/quantize; arrangement commit (linearization).
- **/otio_conf**: OTIO import/diff/plan/apply; atomic reconform [11].
- **/adm_doc**: ADM entities (beds/objects, gain/pos envelopes), BW64/ADM writer, thinning [12], [20].
- **/midi**: pre-instrument MIDI-FX chains, comping playlists, focus views (drums/orchestral).
- **/render**: declarative BounceSpec (stems, formats, SRC/dither), **FFmpeg passthrough** for “same-as-source” video re-mux [6], [14].
- **/abi**: stable C ABIs per subsystem with feature/struct versioning.

### **2.3 Interchange Models (open standards)**

- **OTIO JSON** for picture/edit authority and reconform [11].
- **ADM (BS.2076-3)** for immersive audio metadata with BW64 carriage [12], [20].
- **OSC 1.1** profiles & discovery for controllers/surfaces [7], [11].

### **2.4 Presentation Shells (current/future)**

- **Today** (in REAPER): ReaImGui docked panels and actions; SWS interop where helpful [8], [turn0search1].
- **Tomorrow** (standalone): minimal **JUCE** harness for transport, load/play, and UI; used to prove ABI isolation before scaling [2], [3].

---

## **3. Stable C ABIs (namespacing, negotiation, safety)**

### **3.1 Versioned ABI Contracts**

Each domain exports a C ABI with semantic versioning and negotiation:

```javascript
// /core/abi/clipgrid_v1.h
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define CLIPGRID_ABI_VERSION_MAJOR 1
#define CLIPGRID_ABI_VERSION_MINOR 0

typedef struct cg_time_sig { int32_t num, den; } cg_time_sig;
typedef struct cg_uuid { uint64_t hi, lo; } cg_uuid;

typedef enum cg_err {
  CG_OK = 0, CG_ERR_INVALID, CG_ERR_OOM, CG_ERR_UNSUPPORTED, CG_ERR_IO
} cg_err;

typedef struct cg_slot_id { cg_uuid track; int32_t row; int32_t col; } cg_slot_id;

typedef struct cg_quant {
  double grid_beats;    // e.g., 0.25=16th notes
  double launch_lat_ms; // max jitter allowance
} cg_quant;

typedef struct cg_api_v1 {
  uint32_t (*get_version_major)();
  uint32_t (*get_version_minor)();

  cg_err (*create_session)(void** out);
  cg_err (*destroy_session)(void* s);

  cg_err (*set_time_sig)(void* s, cg_time_sig ts, double bpm);
  cg_err (*set_quant)(void* s, cg_quant q);

  cg_err (*trigger_slot)(void* s, cg_slot_id id, double when_qtr);
  cg_err (*stop_slot)(void* s, cg_slot_id id);

  cg_err (*commit_scene_to_arrangement)(void* s, int32_t scene, double dst_beats);
} cg_api_v1;

// Factory symbol exposed by the shared library:
const cg_api_v1* clipgrid_request_api(uint32_t want_major, uint32_t* got_major,
                                      uint32_t* got_minor);
#ifdef __cplusplus
}
#endif
```

**Principles**:

- **C, not C++** for binary stability.
- **Function tables** per major version; feature detection via minor version.
- **Error codes** only; host wraps errors with logs/telemetry.
- **No global state**; all handles explicit for reentrancy/thread safety.

### **3.2 Feature Flags and Capabilities**

Each ABI includes get_capabilities() returning bitmasks, e.g., ADM_CAP_OBJECT_BINAURAL, OTIO_CAP_RETIME, RENDER_CAP_FFMPEG_PASSTHRU. Hosts adapt UI based on present capabilities.

---

## **4. Neutral Data Contracts**

### **4.1 Session Graph (JSON; FlatBuffers optional)**

```javascript
{
  "version": "1.0",
  "timebase": { "bpm": 120.0, "ts": {"num":4,"den":4}, "sr_hz": 48000 },
  "tracks": [{
    "uuid":"a3d2-...-9f",
    "name":"DX Main",
    "type":"audio",
    "color":"#22AAFF",
    "items":[{
      "uuid":"i-001",
      "src":{"path":"DX_001.wav","start":0.0,"len":12.345},
      "start": 100.000, "len": 12.345,
      "clipGain":{"points":[[0.0, -1.2],[4.0, 0.0],[9.0, -3.0]]}
    }]
  }],
  "automation":[{
    "target":{"track":"a3d2-...-9f","param":"vol"},
    "points":[ [98.000, -3.0], [110.000, -6.0] ]
  }]
}
```

FlatBuffers IDL is provided for deterministic schemas and zero-copy reads in the standalone host.

### **4.2 BounceSpec (declarative renders)**

```javascript
{
  "version":"1.0",
  "sr_hz": 48000, "bitdepth": 24, "dither":"TPDF",
  "src": {"mode":"stems", "selection":["DX","MUS","SFX"]},
  "channels": {"layout":"stereo", "map":[0,1]},
  "naming": "{proj}_{stem}_{sr}k_{bd}b",
  "video": {"mode":"passthrough", "ffmpeg_args":"-c copy"}
}
```

### **4.3 ReconformPlan (atomic edits from OTIO)**

```javascript
{
  "version":"1.0",
  "edits":[
    {"op":"insert", "at": 103.000, "dur": 2.000, "src_timeline_id":"revB"},
    {"op":"delete", "from": 215.000, "to": 217.000},
    {"op":"retime", "from": 300.000, "to": 330.000, "stretch": 0.98}
  ],
  "preconditions":{"project_hash":"...","frame_rate": 23.976},
  "postconditions":{"crc":"..."}
}
```

### **4.4 ADMDoc (beds/objects)**

```javascript
{
  "version":"1.0",
  "programme":{"id":"prg-0001","name":"EP101"},
  "content":[{"id":"cnt-01","name":"DX"}],
  "objects":[{
    "id":"obj-01","name":"DX_Obj_1",
    "gain":{"points":[[0.0, -1.0],[5.0, 0.0]]},
    "position":{"az":[[0.0,0.0],[5.0,15.0]],"el":[[0.0,0.0]],"dist":[[0.0,1.0]]},
    "binaural":"Off"
  }],
  "beds":[{"id":"bed-51","layout":"5.1","assign":["DX","MUS","SFX"]}]
}
```

---

## **5. REAPER Adapter (today)**

### **5.1 Translation Responsibilities**

- **Project State ↔ Session Graph**: Read/write RPP via SDK calls, never by scraping files; maintain UUID lineage.
- **Timeline Ops**: map Core edits to item/region operations; coalesce into single undo points.
- **Markers/Memory Locations**: implement multi-ruler track markers, scope recall in chunk namespace (see §8).
- **Automation Preview/Glide**: shadow envelopes with commit/match logic.
- **Clip Launching**: transport-synchronized triggers; commit to arrangement.
- **ADR/Reconform**: apply ReconformPlan atomically; video overlays via REAPER video window + external monitors.
- **Immersive**: manage ADMDoc, bed/object routing metadata; export BW64/ADM via core writer.
- **Render**: fulfill BounceSpec; video passthrough with ffmpeg -c copy [6], [14].

### **5.2 Adapter Skeleton (C++)**

```javascript
bool reaper_plugin_entry(HINSTANCE hInstance, reaper_plugin_info_t* rec) {
  if (!rec || rec->caller_version < REAPER_PLUGIN_VERSION) return false;

  const auto* cg = clipgrid_request_api(1, &maj, &min);
  if (!cg || cg->get_version_major() != 1) return false;

  // Register actions/panels
  plugin_register("command_id", (void*)cmd_clipgrid_panel);
  plugin_register("accel", (void*)&g_shortcuts);

  // On transport start, arm clip triggers due this block:
  g_on_audio_block = [](double projpos, double blocklen) {
    cg->trigger_slot(g_session, next_slot, quantize_to(projpos, blocklen));
  };

  return true;
}
```

> The adapter uses documented **REAPER Extensions SDK** registration and lifecycle entry points; actions/panels use the same published API surfaces [1], [8].

---

## **6. Minimal JUCE Host (tomorrow)**

Purpose: **prove independence** without feature bloat—load core libs, open Session Graph, play beep/tones/audio region, basic transport, basic UI. Later, integrate plugin hosting (VST3/CLAP), surfaces, media graph. JUCE is used solely for windowing/audio/MIDI scaffolding and cross-platform build [2], [3].

**Boot flow**:

1. Load each core ABI (dlopen/LoadLibrary) → negotiate versions.
2. Create basic **AudioDeviceManager**/**AudioSourcePlayer**; pump processBlock.
3. Map transport and timeline to /clipgrid and /session calls.
4. Render/export via /render to verify BounceSpec determinism.

---

## **7. IPC Fallbacks (when SDK ceilings appear)**

If the host lacks hooks (e.g., deep edit interceptors or video overlays), adapters can **fork a sidecar** process (gRPC/Cap’n Proto/zeromq) exposing core services. The adapter proxies UI and timeline events, the sidecar executes heavy logic (reconform planning, ADM writing). This preserves UX while containing realtime risk.

---

## **8. REAPER Chunk Namespacing**

All extension state lives in **<PTBRIDGE\_\*>** blocks; versioned; CRC-guarded; safe to ignore if unknown.

```javascript
<PTBRIDGE_CONFIG
  VERSION 1.1
  FEATURES 0x0000_004F
  CRC 0xA1B2C3D4
>
<PTBRIDGE_MEMORYLOC ... >
<PTBRIDGE_PLAYLISTS ... >
<PTBRIDGE_AUTOMATION ... >
<PTBRIDGE_WARP ... >
<PTBRIDGE_GROUPS ... >
```

Back-compat rule: if a parser hits an unknown field, **skip**; never fail load. (REAPER’s extension model allows safely storing custom chunk data alongside native state [8].)

---

## **9. UI Surfaces (REAPER Today; Shell Tomorrow)**

- **ReaImGui** docked panels: Memory Locations, ClipGrid, Automation Preview, Groups Matrix, Reconform Preview. (High-frequency UI runs on UI thread; audio-thread-safe mailboxes ferry commands) [8].
- **Controller Surfaces**: OSC 1.1 profiles published as JSON descriptors; discoverable via OSC query; map to capability registry [7], [11].
- **Visual Overlays**: ADR streamers/wipes/countdowns on REAPER’s video window; external monitor via host video output.

---

## **10. Performance & Realtime Discipline**

- **Budgets**: maxCPU 80%, maxLatency 3 ms budget per block; **side-thread** all heavy ops; **zero** blocking allocations on audio thread.
- **Large Sessions**: lazy hydrate item/automation data; tile waveforms; batch envelope ops.
- **ClipGrid**: schedule on **transport tick**; ensure launch jitter <1 ms @120 BPM; pre-roll quantization windows.
- **ADM/Render**: stream writer uses buffered I/O; thinning policies reduce metadata size without semantic loss (per BS.2076-3) [12], [20].
- **Passthrough**: video re-mux uses **FFmpeg stream copy** to keep frames byte-identical [6], [14].

---

## **11. Undo/Redo Atomicity**

Compound operations (e.g., reconform) wrap in transactional guards:

```javascript
Undo_BeginBlock2(proj, 'Apply ReconformPlan');
apply_all_edits_atomic();
Undo_EndBlock2(proj, 'Apply ReconformPlan', UNDO_STATE_ALL);
```

Adapter records one **atomic** undo point per high-level action; crash-safe temp files for media ops.

---

## **12. Conformance & Test Suite**

### **12.1 Golden Fixtures**

- **OTIO**: editorial diffs (insert/delete/move/retime) across fps variants (23.976/24/25/29.97).
- **ADM**: bed/object permutations; binaural flags; automation envelopes; output validated against ADM XSD [12], [20].
- **Render**: BounceSpec matrix (rates/bitdepths/dither/layouts), deterministic filename contracts.
- **ClipGrid**: quantization matrices (1/4..1/64), scene juggling, arrangement commit correctness.
- **Video Passthrough**: assert sha256(video_in) == sha256(video_out_video_stream).

### **12.2 Metrics**

```javascript
Audio Correlation (ρ ≥ 0.999)
Launch Jitter (μ ≤ 0.5 ms; p99 ≤ 1.0 ms @120 BPM)
Reconform Success (≥ 99.9% across corpus)
ADM Schema Valid (100%); Renderer ingest no warnings
Passthrough Integrity (video CRC equal; audio replaced)
Crash Rate (≤ 1 per 200 hrs stress)
```

### **12.3 Automation**

- **Headless CI** triggers per commit (Linux/macOS/Windows) building core libs + adapters; runs unit+integration; produces HTML report artifacts.
- **Compliance runners**: validate ADM against ITU schema; OTIO structure sanity; FFmpeg integrity checks [11], [12], [6].

---

## **13. CI/CD and Packaging**

- **Build**: CMake superbuild; artifacts:
  - libclipgrid_v1.(dll|so|dylib) and peers
  - reaper_ptbridge.(dll|dylib)
  - pt_minhost.(exe|app) (JUCE harness)
- **Sign/Notarize** macOS; Authenticode Windows.
- **Cache** toolchains; pin dependencies (VST3 SDK, JUCE hash, FFmpeg version) [2], [3], [6].
- **Nightly**: conformance + fuzz (chunk fuzzing, BounceSpec arg fuzz).
- **Release**: Semantic versions for ABIs; adapters encode compatible ranges in PTBRIDGE_CONFIG.

---

## **14. Licensing & Legal Guardrails**

- Use **published** REAPER SDK only; no scraping of internals [1], [8].
- **VST3** license terms apply to hosting or facades [2], [25].
- **CLAP** is MIT-licensed; leverage for vendor-neutral plugin hosting where desired [turn0search2], [turn0search6].
- **ARA** integration requires vendor permission/SDK terms (Celemony) [4], [26].
- **Dolby Atmos Renderer** embedding/control governed by Dolby terms; **prefer ADM export** + external renderer IPC where licensing is constrained [21], [13].

---

## **15. Failure Modes & Mitigations**

| **Failure Mode**            | **Symptom**                                  | **Mitigation**                                                                |
| --------------------------- | -------------------------------------------- | ----------------------------------------------------------------------------- |
| SDK ceiling                 | Missing hooks (e.g., tool-time interceptors) | IPC sidecar + best-effort UX; keep core logic outside host                    |
| Plugin latency misreport    | ADC drift/phase issues                       | Active latency probe per plugin chain; reconcile vs reported values           |
| Chunk drift                 | REAPER update breaks custom sections         | Pinned namespaces, schema versions, **ignore-unknown** parsing; backups + CRC |
| Renderer/licensing surprise | Feature gap                                  | Ship ADM first; optional renderer IPC; keep core writer authoritative         |
| Realtime overload           | Glitches                                     | Budget monitors; circuit-breaker to defer non-critical ops; telemetry         |

---

## **16. Roadmap to Sovereignty (Milestones)**

- **M1 — Adapter Foundations (Now)**: Memory Locations (multi-ruler + scope), Automation Preview/Glide, Playlists/Comping, Clip Gain curves, BounceSpec & Passthrough.
- **M2 — Non-Linear & ADR**: ClipGrid (quantized scenes), Reconform (OTIO import/diff/atomic apply), ADR overlays.
- **M3 — Immersive**: ADM entity graph, BW64/ADM export, external renderer IPC.
- **M4 — Minimal Host**: JUCE harness loads core libs, plays/records, commits ClipGrid; basic surface via OSC.
- **M5 — Host Graduation**: Add plugin hosting (VST3/CLAP), media graph, project persistence; REAPER adapter remains as a **first-class adapter** for migration users.

---

## **17. Repository Topology (explicit and boring by design)**

```javascript
/core
  /session         # graph + JSON/FlatBuffers I/O
  /clipgrid        # slots/scenes/quantize/commit
  /otio_conf       # OTIO read/diff/plan/apply
  /adm_doc         # ADM entities + BW64 writer
  /render          # BounceSpec, SRC/dither, ffmpeg passthrough
  /midi            # MIDI-FX chains, comping, focus views
  /abi             # public C headers + version negotiation
/adapters
  /reaper_ext      # REAPER adapter (Extension SDK)
  /juce_host_min   # minimal sovereign host harness
/tools
  /conformance     # golden fixtures, validators, perf
  /fixtures        # OTIO/ADM/video ref sets
  /scripts         # CI, packaging, notarization
```

---

## **18. Security, Telemetry, and Privacy**

- **Opt-in** anonymous telemetry: performance counters, error codes, version compatibility; **no media content** ever.
- **Crash dumps**: symbolized server-side; redaction before upload.
- **Integrity**: all chunk writes double-buffered with checksums; recovery on mismatch.
- **Supply chain**: pin SDK/dep hashes (VST3, JUCE, FFmpeg), verify signatures [2], [3], [6].

---

## **19. What “Done” Looks Like**

- Running inside REAPER with professional parity (markers/memory, comping, clip gain/warp, automation preview, ADR/reconform, immersive export, declarative renders).
- Core libs pass conformance with deterministic outputs and ADM/OTIO validation.
- Minimal host proves **ABI sufficiency**: load session, trigger scenes, commit, render.
- Clear runway to evolve the sovereign shell without touching the core.

---

## **References**

[1] Cockos Inc., “REAPER | Extensions SDK,” 2025. [Online]. Available: https://www.reaper.fm/sdk/plugin/plugin.php

[2] Steinberg Media Technologies, “VST 3 SDK Documentation,” 2025. [Online]. Available: https://steinbergmedia.github.io/vst3_doc/vstsdk/index.html

[3] JUCE, “Documentation,” 2025. [Online]. Available: https://juce.com/learn/documentation/

and https://docs.juce.com/

[4] Celemony, “ARA Audio Random Access,” 2025. [Online]. Available: https://www.celemony.com/en/service1/about-celemony/technologies/ara

[5] Ableton AG, “Ableton Live 12 Reference Manual,” 2024. [Online]. Available: https://www.ableton.com/manual

[6] FFmpeg Developers, “ffmpeg Documentation (stream copy),” 2025. [Online]. Available: https://ffmpeg.org/ffmpeg.html

[7] Open Sound Control Working Group, “OpenSoundControl 1.1 (overview),” 2021–2025. [Online]. Available: https://opensoundcontrol.stanford.edu/spec-1_1.html

[8] Cockos Inc., “REAPER ReaScript and Extension Surfaces (SDK entry points),” 2025. [Online]. Available: https://www.reaper.fm/sdk/reascript/reascripthelp.html

[9] Steinberg, “Third-Party Developer Portal,” 2025. [Online]. Available: https://www.steinberg.net/developers/

[10] SWS Project, “SWS / S&M Extension for REAPER,” 2025. [Online]. Available: https://www.sws-extension.org/

[11] OpenTimelineIO, “Documentation & File Format Specification,” 2025. [Online]. Available: https://opentimelineio.readthedocs.io/en/latest/

and https://opentimelineio.readthedocs.io/en/latest/tutorials/otio-file-format-specification.html

[12] ITU-R, “BS.2076-3: Audio Definition Model (ADM),” 2025. [Online]. Available: https://www.itu.int/rec/R-REC-BS.2076-3-202502-I/en

and PDF: https://www.itu.int/dms_pubrec/itu-r/rec/bs/R-REC-BS.2076-3-202502-I%21%21PDF-E.pdf

[13] Dolby Laboratories, “Dolby Atmos Renderer Documentation (Landing),” 2025. [Online]. Available: https://professionalsupport.dolby.com/s/article/Dolby-Atmos-Renderer-Documentation?language=en_US

[14] FFmpeg Developers, “ffmpeg-all Manual (stream copy flags),” 2025. [Online]. Available: https://ffmpeg.org/ffmpeg-all.html

[15] Wikipedia, “REAPER,” 2025. [Online]. Available: https://en.wikipedia.org/wiki/REAPER

[16] Ultraschall Project, “REAPER API (searchable mirror),” 2025. [Online]. Available: https://mespotin.uber.space/Ultraschall/Reaper_Api_Documentation.html

[17] free-audio, “CLAP — Audio Plugin API (MIT),” 2025. [Online]. Available: https://github.com/free-audio/clap

and overview: https://cleveraudio.org/

---

### **Appendix A — CMake Superbuild Sketch**

```javascript
cmake_minimum_required(VERSION 3.24)
project(ptbridge LANGUAGES C CXX)

option(PT_WITH_VST3 "Build VST3 adapter" ON)
option(PT_WITH_CLAP "Build CLAP adapter" OFF)

add_subdirectory(core/session)
add_subdirectory(core/clipgrid)
add_subdirectory(core/otio_conf)
add_subdirectory(core/adm_doc)
add_subdirectory(core/render)
add_subdirectory(adapters/reaper_ext)
add_subdirectory(adapters/juce_host_min)
add_subdirectory(tools/conformance)
```

### **Appendix B — FFmpeg Passthrough (library call or CLI)**

- Library: set codec to copy for video streams; re-mux audio with new PCM/encoded streams.
- CLI parity: ffmpeg -i input.mov -map 0:v -map 1:a -c:v copy -c:a pcm_s24le output.mov [6], [14].

### **Appendix C — OSC Profile Descriptor (snippet)**

```javascript
{
  "device":"PT-Controller-X",
  "endpoints":[
    {"path":"/transport/play","type":"bang"},
    {"path":"/fader/{track}","type":"float","range":[0.0,1.0]},
    {"path":"/meter/{track}","type":"float","rate_hz":30}
  ],
  "discovery":{"service":"_osc._udp","port":9000}
}
```
