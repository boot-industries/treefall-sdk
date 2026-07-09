# Orpheus SDK - Codex CLI Agent Configuration

**Target Model:** GPT-5.1-Codex-Max (OpenAI, November 2025)
**CLI Tool:** OpenAI Codex CLI
**Repository:** orpheus-sdk (Professional C++20 Audio SDK)
**Documentation PREFIX:** ORP (SDK), OCC (Clip Composer app)

Professional audio SDK with host-neutral C++20 core for deterministic session, transport, and render management.

---

## Project Context

### Core Principles (NON-NEGOTIABLE)

1. **Offline-first** — No runtime network calls for core features
2. **Deterministic** — Same input → same output, always (sample-accurate, bit-identical)
3. **Host-neutral** — Core SDK works across REAPER, standalone apps, plugins, embedded
4. **Broadcast-safe** — 24/7 reliability, zero audio thread allocations

**Decision Framework:** Before suggesting any change:

- Will this work offline? (If no → wrong for core SDK)
- Is this deterministic? (If no → cannot be in render path)
- Is this host-neutral? (If no → belongs in adapter layer)
- For all applications? (If no → app-specific, goes in `apps/`)

---

## Audio Code Constraints (CRITICAL)

**ZERO on audio thread:**

- Allocations (`new`, `malloc`, `std::vector::push_back`, `std::string`)
- Locks (`std::mutex`, `std::lock_guard`)
- I/O (file reads, network calls)
- System calls (`time()`, `rand()`, non-deterministic sources)

**ONLY on audio thread:**

- Pre-allocated buffers
- Atomic operations (`std::atomic`)
- Lock-free data structures
- Fixed-size arrays
- Sample-accurate integer math

**Verification:**

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

**If you suggest code that allocates on audio thread → FLAG IT IMMEDIATELY and revise.**

---

## Repository Structure

```
orpheus-sdk/
├── src/, include/       # Core SDK (C++20)
├── adapters/            # Host integrations
├── apps/                # SDK-hosted applications (Clip Composer extracted — see below)
├── packages/
│   └── shmui-juce/      # UX package - JUCE audio visualization components
├── tests/               # GoogleTest suite
├── docs/orp/            # ORP-prefixed documentation
├── CLAUDE.md            # Claude Code config (reference)
└── .codex/              # This directory
    └── AGENTS.md        # This file
```

## UX Package: shmui-juce

**Location:** `packages/shmui-juce/` | **Purpose:** First-party JUCE UI components for Orpheus apps

The **shmui-juce** package provides audio visualization components for application-level UI. These components are maintained within orpheus-sdk but originate from the upstream shmui dual-stack library (`~/dev/shmui`).

**Components:**

- `AudioAnalyzer` - Thread-safe FFT/RMS analysis (audio thread compatible)
- `WaveformVisualizer`, `BarVisualizer`, `OrbVisualizer`, `MatrixDisplay` - Visual feedback components

**Usage Constraints:**

- ✅ Application layer (`apps/clip-composer`, `apps/wave-finder`, etc.)
- ❌ Core SDK (SDK remains UI-agnostic for host-neutrality)

**Threading Compatibility:**

- `AudioAnalyzer` is lock-free → safe for Orpheus broadcast-safe audio threads
- Visualization components require JUCE message thread

**See Also:**

- shmui docs: `~/dev/shmui/GEMINI.md`, `~/dev/shmui/.codex/AGENTS.md`
- Package source: `packages/shmui-juce/ShmUI.h`

---

## Quick Commands

```bash
# Build & Test
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure

# Run Clip Composer (ALWAYS use script)
./scripts/relaunch-occ.sh

# Incremental build
cmake --build build --target SpecificFile.cpp.o

# Format code (CI enforced)
clang-format -i src/**/*.{cpp,h}
```

**Full reference:** `docs/repo-commands.html`

---

## Codex CLI Workflows

### Long-Horizon Tasks (24+ Hours)

**Your compaction enables multi-day autonomous work.** Example:

```
Task: Implement complete IRoutingMatrix interface
Estimated: 18 hours

Phase 1 (6h): Core routing logic
- Implement routing matrix operations
- Pre-allocate all data structures
- Tests: Unit tests for routing

Phase 2 (4h): Integration with transport
- Wire to audio thread (lock-free)
- Verify real-time safety
- Tests: Integration tests

Phase 3 (4h): Documentation
- Update docs/ARCHITECTURE.md
- Create docs/orp/ORP### session report
- Add API examples

Phase 4 (4h): Performance validation
- Run sanitizers (ASan/UBSan)
- Profile CPU usage
- Verify determinism

Expected output:
- src/RoutingMatrix.cpp (fully implemented)
- tests/RoutingMatrix.test.cpp (100% coverage)
- docs/orp/ORP### Routing Matrix Implementation.md
```

### Code Review Mode

```bash
codex exec "Review PR #123 for:
1. Real-time safety (no allocations on audio thread)
2. C++20 compliance
3. Test coverage
4. Documentation (update ARCHITECTURE.md if needed)"
```

### Multi-Window Workflows

**Your compaction handles context efficiently:**

1. Read affected files in dependency order (headers → impl → tests)
2. Plan changes per file
3. Execute and verify with tests after each phase

**Compaction preserves:**

- Critical architecture decisions
- Threading model constraints
- Test coverage requirements

---

## Current Status

**ORP068 Progress:** 55/104 tasks (52.9%)

- ✅ Phase 0-3 (Repo, Driver, UI, CI)
- ⏳ Phase 4: Docs/productionization (0/14)

**Key Documents:**

- `.claude/implementation_progress.md` - Current sprint
- `docs/orp/ORP068 Implementation Plan (v2.0).md` - Master plan
- `docs/ARCHITECTURE.md` - System design

**Clip Composer:** extracted to its own repo (2026-07-09)

- Now at `~/dev/clip-composer` (GitHub `chrislyons/clip-composer`), consuming this SDK as a submodule. App-specific Codex/Claude guidance lives there. See `docs/orp/ORP131`.

---

## Documentation Standards

**PREFIX Naming:** `{PREFIX###} {Verbose Title}.md`

**Examples:**

- ✅ `ORP082 Session Report.md`
- ✅ `OCC103 QA v020 Results.md`
- ❌ `ORP082.md` (missing title)

**Find next number:**

```bash
ls -1 docs/orp/ | grep -E '^ORP[0-9]{3,4}\s+' | sort -V | tail -1
```

**Session report template:**

```markdown
# ORP### Session Title

[Date]: Brief summary

## Completed

- Task 1 (files: X, Y)
- Task 2 (added: description)

## Technical Decisions

- Decision 1: Rationale
- Decision 2: Rationale

## Next Steps

- [ ] Task 1
- [ ] Task 2

## References

[1] https://reference-url.com
```

---

## Git Workflow

**Before committing:**

```bash
git status
git diff
git log --oneline -5  # Check message style
```

**Commit format:**

```
type: brief summary

Detailed explanation and rationale.

🤖 Generated with GPT-5.1-Codex-Max (OpenAI Codex CLI)
```

**Types:** `feat`, `fix`, `refactor`, `test`, `docs`, `perf`, `chore`

**Verify:**

- [ ] Tests pass
- [ ] Code formatted (`.clang-format`)
- [ ] No audio thread violations (sanitizers)
- [ ] No secrets

---

## File Boundaries

**Never Read:**

- `build-*/`, `apps/*/build/`, `orpheus_clip_composer_app_artefacts/`
- `Third-party/*/build/`, `*.o`, `*.a`, `*.dylib`, `*.so`

**Read First (Priority):**

1. `.claude/implementation_progress.md` (current sprint)
2. `docs/ARCHITECTURE.md` (system design)
3. `docs/orp/ORP068 Implementation Plan (v2.0).md` (master plan)
4. This file

---

## Communication Patterns

**When suggesting changes:**

1. Explain reasoning (why this vs alternatives?)
2. Show code context (`file_path:line_number`)
3. Verify constraints (real-time safety, determinism)
4. Ask for approval (architectural changes)

**Immediately flag:**

- Audio thread violations (allocations, locks, I/O)
- Non-deterministic code (system clock, random)
- Network dependencies in core SDK
- Breaking API changes (without migration plan)

---

## Using Network Access

**C++ & Build:**

- https://en.cppreference.com/ - C++20 reference
- https://cmake.org/documentation/ - CMake docs
- https://google.github.io/googletest/ - GoogleTest docs

**Audio Engineering:**

- https://tech.ebu.ch/publications - EBU specs
- https://www.smpte.org/ - SMPTE standards

**When to use:**

- Uncertain about C++20 behavior
- Need audio formulas (gain curves, timecode)
- Looking up CMake patterns

---

## Additional Resources

**See `CLAUDE.md` for:**

- Detailed workflow conventions
- Cross-repo awareness patterns
- Skills loading configuration

**Key docs:**

- `docs/ARCHITECTURE.md` - Threading model
- `docs/ROADMAP.md` - Timeline
- `docs/repo-commands.html` - Full command reference

---

## Codex CLI Configuration

**Recommended config.toml:**

```toml
[codex]
model = "gpt-5.1-codex-max"

[codex.orpheus-sdk]
auto_test = true
format_on_save = true
sanitizers = ["address", "undefined"]
```

**Session init:**

```bash
cd ~/dev/orpheus-sdk
codex --model gpt-5.1-codex-max
```

---

**Version:** 2.0 (Lean, project-focused)
**Last Updated:** 2025-11-25
**Model:** GPT-5.1-Codex-Max (OpenAI, November 2025)
**SWE-Bench Verified:** 77.9% | **SWE-Lancer IC SWE:** 79.9%

**Remember:** Prioritize determinism, broadcast-safety, and offline-first design. ZERO allocations on audio thread. When uncertain about threading or audio implications, ask before implementing. Your 24+ hour capabilities are ideal for complex SDK development.
