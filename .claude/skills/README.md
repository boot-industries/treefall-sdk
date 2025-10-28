# Orpheus SDK Skills

Skills are automatically loaded into every Claude Code conversation. Just ask natural questions and the relevant skill will activate.

## Available Skills

### rt.safety.auditor (CRITICAL)

Analyze C++ code for real-time safety violations.

**How to use:**

- "Is this audio code real-time safe?"
- "Check for allocations in the audio thread"
- "Review this processBlock function for safety violations"
- "Are there any blocking calls in the render path?"

**Checks for:**

- No heap allocations (new, malloc, vector.push_back, std::make_unique)
- Sample-accurate timing (64-bit sample counts, never float seconds)
- Determinism (std::bit_cast for float, no UB)
- Broadcast-safe patterns (graceful degradation, lock-free structures)

**Critical violations:** allocations, crashes, non-determinism
**Warnings:** sub-optimal patterns, potential race conditions

### test.result.analyzer

Parse ctest and sanitizer output, identify root causes.

**How to use:**

- "Analyze the test failures"
- "What's causing the ASan error?"
- "Summarize the ctest output"
- "Why is coverage at 94%?"

**Analyzes:**

- ctest output from build/Testing/Temporary/
- AddressSanitizer (ASan) reports
- ThreadSanitizer (TSan) reports
- UndefinedBehaviorSanitizer (UBSan) reports
- Coverage reports (98% threshold)

### orpheus.doc.gen

Generate and maintain comprehensive documentation.

**How to use:**

- "Generate Doxygen comments for this class"
- "Create a progress report for this session"
- "Document this API with thread-safety notes"

**Generates:**

- Doxygen comments (Javadoc style)
- Progress reports
- Session notes
- API documentation with real-time safety annotations

### test.analyzer (Shared)

Cross-project test analysis.

**How to use:**

- "Analyze test results across the workspace"
- "Compare test coverage between repos"

### ci.troubleshooter (Shared)

Diagnose CI/CD failures.

**How to use:**

- "Why did the CI build fail?"
- "Debug this CMake configuration error"
- "Troubleshoot the GitHub Actions workflow"

## Critical Workflow: Adding Audio Code

1. **Write your audio processing code**
2. **Ask rt.safety.auditor**: "Is this audio thread safe?"
3. **Fix any violations** (use lock-free structures, pre-allocate)
4. **Build and test**: cmake --build build && ctest --test-dir build
5. **Ask test.result.analyzer**: "Analyze the sanitizer output"
6. **Run determinism test**: Check bit-identical output
7. **Document**: "Generate Doxygen for this API"

## How Skills Work

Skills enforce **professional-grade audio engineering standards** automatically:

- Broadcast-safe (24/7 reliability)
- Deterministic (same input → same output, always)
- Sample-accurate (64-bit counts, no drift)
- Host-neutral (works across DAWs, plugins, embedded systems)

**Think of skills as your audio safety checklist automation.**

For automated workflows (building, testing, deploying), use **Agents** (see `.claude/agents/`).
