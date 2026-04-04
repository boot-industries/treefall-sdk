# Decision: Archive TypeScript Packages (Option A)

**Date:** 2025-11-05
**Status:** Approved
**Decision:** Archive `packages/` directory, focus on C++ SDK

---

## Context

The Orpheus SDK repository exhibited a dual identity with misaligned components:

**Core SDK (C++20):**
- Production-ready professional audio SDK
- Target: DAWs, plugins, broadcast tools, standalone applications
- Status: 58/58 tests passing, fully functional
- Consumer: Clip Composer (JUCE desktop app) ✅

**TypeScript Packages (8 packages):**
- Originally built for shmui web application (Phase 1-2)
- Components: `@orpheus/client`, `@orpheus/engine-*`, `@orpheus/react`
- Status: Built and functional
- **Consumer: NONE** (shmui removed in PR #129, ORP098)

## Problem Statement

### Maintenance Burden

- CI builds TypeScript packages on every push
- Weekly security audits for npm dependencies
- Package version coordination across 8 packages
- Documentation maintenance for unused APIs
- No downstream consumers after shmui removal

### Strategic Ambiguity

- Unclear if Orpheus is a web SDK or C++ SDK
- New contributors confused about focus area
- Token cost: ~20K in AI context, reviewed in every code session
- CI time: TypeScript builds add ~2 minutes to pipeline

## Options Considered

### Option A: C++ SDK Focus (CHOSEN)

**Actions:**
- Archive `packages/` directory (preserve in git history)
- Update README to clarify C++ SDK focus
- Remove package-related CI jobs
- Focus development on C++ API, JUCE applications

**Benefits:**
- ✅ Clear strategic direction
- ✅ Reduced CI/CD time (faster feedback)
- ✅ Lower maintenance burden
- ✅ Simplified onboarding for C++ developers
- ✅ Aligns with current reality (OCC uses JUCE, not web)

**Risks:**
- ⚠️ Loses future web UI option (mitigated: can restore from git history)
- ⚠️ Discards Phase 1-2 work (mitigated: fully documented, can reference)

**Target Audience:** Professional audio developers (C++, JUCE, REAPER plugins)

### Option B: Multi-Platform SDK (NOT CHOSEN)

**Actions:**
- Keep packages, complete web integration
- Build example web applications (session browser, remote control)
- Document web SDK use cases
- Create `packages/orpheus-ui` component library

**Benefits:**
- Serves both C++ and web developers
- Enables web-based tools
- Expands SDK reach beyond C++

**Risks:**
- Doubles maintenance surface area
- Requires dedicated web developer resources
- OCC doesn't benefit (uses JUCE, not web)

**Rejected Because:**
- No current use case for web packages
- Small team should focus on one platform
- Can revisit post-v1.0 if web demand emerges

### Option C: Hybrid Approach (NOT CHOSEN)

**Actions:**
- Maintain packages minimally (security updates only)
- Document as "future use, not actively developed"
- Prioritize C++ SDK for v1.0-v1.5

**Benefits:**
- Preserves future web option
- Focuses resources on primary use case

**Risks:**
- Packages may become outdated (dependency drift)
- Requires periodic maintenance to prevent bitrot
- Ambiguity remains about SDK purpose

**Rejected Because:**
- Half-measure doesn't resolve strategic ambiguity
- Still incurs maintenance cost
- Better to make clean decision now

## Decision

**We choose Option A: C++ SDK Focus**

### Rationale

1. **Current Reality:** Clip Composer (only consumer) uses JUCE, not web technologies
2. **Resource Efficiency:** Small team should focus on one platform excellently
3. **Market Positioning:** Competing with established C++ audio SDKs (JUCE, iPlug2, Tracktion Engine)
4. **Reversibility:** Packages preserved in git history, can restore if web use case emerges
5. **Strategic Clarity:** Clear positioning as "C++ SDK for professional audio applications"

### Implementation

**Completed Actions:**
- [x] Created `docs/DECISION_PACKAGES.md` documenting choice
- [ ] Moved `packages/` to `archive/packages/` (preserves structure)
- [ ] Updated `.claudeignore` to exclude archived packages
- [ ] Removed TypeScript CI jobs from `.github/workflows/`
- [ ] Removed chaos tests workflow (packages-only, no C++ consumers)
- [ ] Updated `README.md` with "C++ SDK for professional audio" positioning
- [ ] Updated `.claude/implementation_progress.md` (mark Phase 1-2 as archived)

## Consequences

### Positive

- **Faster CI:** ~2 minutes saved per pipeline run (TypeScript builds removed)
- **Clearer Positioning:** README explicitly states "C++ SDK"
- **Reduced Context:** ~20K tokens saved in AI-assisted development
- **Focus:** Development resources concentrated on core C++ SDK and Clip Composer

### Negative

- **Web Features Unavailable:** No web-based session browser, remote control, or React components
- **Limited Reach:** SDK only accessible to C++ developers (not web developers)

### Neutral

- **Reversible:** Can restore packages from git history if web use case emerges
- **Documentation Preserved:** Phase 1-2 implementation reports remain in `docs/orp/`
- **Learning Captured:** Web driver architecture documented in ORP068, ORP070-ORP072

## Future Considerations

### When to Revisit This Decision

Consider restoring packages if:
- User demand for web-based Orpheus tools emerges
- External contributor offers to maintain web packages
- Product roadmap shifts to include web applications
- Competitive landscape shows web integration as differentiator

### How to Restore (If Needed)

```bash
# Restore from archive
mv archive/packages packages

# Restore TypeScript CI jobs
git checkout <pre-archive-commit> -- .github/workflows/typescript-*.yml

# Update .claudeignore (remove archive exclusions)
# Update README.md (add web SDK documentation)

# Rebuild packages
pnpm install
pnpm build
```

## References

[1] `docs/orp/ORP102 Repository Analysis and Sprint Recommendations.md` - Analysis leading to this decision
[2] `docs/orp/ORP098 Shmui Rebuild.md` - Web UI removal rationale
[3] `docs/orp/ORP068 Implementation Plan (v2.0).md` - Phase 1-2 implementation
[4] `.claude/implementation_progress.md` - Current progress tracking

---

**Decision Made By:** Repository maintainer (based on ORP102 analysis)
**Implementation Date:** 2025-11-05
**Review Date:** Post-v1.0 release (estimated 2026-Q1)
