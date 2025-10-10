AGENTS.md — Coding Assistant Guidelines for Orpheus SDK

Audience: AI coding assistants (e.g. ChatGPT, Codex, Copilot) and human contributors using such assistants.
Mission: Maintain the Orpheus SDK as a professional-grade audio engine — a host-neutral C++ core for deterministic session, transport, and render management — while keeping all “agent” integrations strictly optional.

⸻

🎯 Core Technical Objectives

1. Primary Target
	•	Language: C++20
	•	Scope: Portable across Windows/macOS/Linux
	•	Build System: CMake + ctest; CI uses clang-format, clang-tidy, AddressSanitizer
	•	Core Features: SessionGraph (tracks, clips, tempo, transport), deterministic render path, ABI negotiation (AbiVersion)
	•	Adapters: Thin layers only (REAPER extension, minhost CLI, WASM/Electron bindings)

2. Professional Audio Context
	•	Aligns with REAPER SDK and Pro Tools parity goals: deterministic editing, punch/loop record semantics, render contracts, ADM/OTIO interchange  ￼
	•	Emphasizes broadcast-safe determinism, sample-accurate rendering, and time-base isolation across hosts
	•	Future expansion includes ADM authoring, OSC control, and clip-grid scheduling

3. UI Layer (Shmui Integration)
	•	Shmui (Next.js/Turbopack + pnpm workspace) exists to visualize sessions, transport, and audio players, not to host cloud agents
	•	Components: Orb (metering/transport), AudioPlayer (local PCM playback), waveform/timeline prototypes, docs tooling
	•	No dependencies on ElevenLabs APIs; external “agent” endpoints remain opt-in, local-mocked, or disabled by default. `pnpm --filter www dev` launches the mock site on localhost:4000.

⸻

🚫 Out-of-Scope by Default
	•	Embedding third-party SaaS/voice runtimes (e.g. ElevenLabs, OpenAI, Cartesia)
	•	Hard-coded analytics or telemetry
	•	Non-deterministic “agent orchestration” inside the core engine
	•	Modifying CMake or CI to require network access

All “agent” examples must compile without network access and pass CI in offline mode.

⸻

🧭 Repository Signals for AI Assistants

Layer	Language	Purpose	Notes
/src, /include	C++20	Audio session core	Deterministic, portable
/adapters/reaper_orpheus	C++	REAPER extension (DLL)	Follows REAPER ABI
/adapters/minhost	C++	CLI host	Testing reference
/packages/shmui	TypeScript/React	UI demos	Local-mocked
/packages/engine-wasm, /engine-electron	C++/JS bridge	Optional bindings	Isolated builds


⸻

🧩 Safe Implementation Patterns

C++ Core

cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure

	•	Enforce .clang-format + .clang-tidy
	•	Keep adapters ≤300 LOC and optional via BUILD_ADAPTER_* flags
	•	Preserve float determinism across OSes

UI (Shmui)
	•	Examples live under packages/shmui/apps/www/ (Next.js on port 4000)
        •       Bootstrapped with pnpm (`pnpm install` at repo root). Keep scripts compatible with `pnpm --filter www …` invocation
	•	Always guard networked features behind env vars (e.g. VITE_ENABLE_AGENT=0)
	•	Use mock data for voice or waveform demos; no hard dependencies on SaaS audio APIs
	•	Avoid adding npm deps that bind directly to audio cloud APIs

⸻

🔐 Security Defaults
	•	Localhost bind only (127.0.0.1); never expose 0.0.0.0
	•	Tokens only via .env.local; never committed
	•	Prefer unsigned local ports for demo servers

⸻

🎚 “Agent” Semantics in This Repo
	•	“Agent” = optional orchestration layer external to the Orpheus core
	•	Acceptable uses:
	•	Mocking transport state (thinking/talking Orb demo)
	•	Demonstrating local voice playback with Orpheus render output
	•	Unacceptable uses:
	•	Replacing Orpheus render pipeline with SaaS audio streams
	•	Committing API credentials or agent models

⸻

🧱 File Placement

Type	Location	Example
Core / adapters	src/, adapters/	SessionGraph.cpp, reaper_adapter.cpp
UI demos	packages/shmui/apps/www/	DemoSession.tsx
Docs	docs/	ORP068 Implementation Plan.md


⸻

🧠 Guidance for Coding Assistants

Request Type	Expected Output
Add core feature	C++20 code + CMake + unit tests (ctest)
Create UI demo	Next.js/React mock using pnpm workspace, no external API
Add background service	Localhost-only, behind flag
Update docs	Link back to README + Integration Plan (docs should reflect the repository setup described in README)


⸻

✅ Do / ❌ Don’t Checklist

Do
	•	Maintain host-neutral determinism
	•	Respect sample-accurate rendering and clock domain isolation
	•	Keep UI examples self-contained and mockable
	•	Document all flags, ports, and env vars
	•	Keep CI/build green across OSes

Don’t
	•	Break core/adapter abstraction
	•	Depend on SaaS runtimes for functionality
	•	Add unverified external libraries
	•	Bypass style or test gates

⸻
