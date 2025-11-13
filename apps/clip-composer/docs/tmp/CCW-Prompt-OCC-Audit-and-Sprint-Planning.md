# CCW Prompt: OCC Documentation Audit & Sprint Planning

**Created:** 2025-11-11
**Purpose:** Comprehensive audit of OCC090-OCC110 documentation to identify gaps and recommend sprints
**Expected Output:** OCC111 (Gap Audit Report), OCC112 (Sprint Roadmap)

---

## Mission

Conduct a comprehensive audit of Orpheus Clip Composer documentation (OCC090-OCC110) to identify missed tasks and recommend sprint plans for both gap closure and forward progress.

## Input Scope

Read and analyze all OCC documentation from **OCC090** through **OCC110** (21 documents):

**Location:** `/Users/chrislyons/dev/orpheus-sdk/apps/clip-composer/docs/occ/`

**Documents to analyze:**

```
OCC090 v020 Sprint.md
OCC091.md
OCC092.md
OCC093 v020 Sprint - Completion Report.md
OCC094.md
OCC095.md
OCC096.md
OCC097.md
OCC098.md
OCC099 Testing Strategy.md
OCC100 Performance Requirements and Optimization.md
OCC101 Troubleshooting Guide.md
OCC102 OCC Track v020 Release & v021 Planning.md
OCC103 QA v020 Tests.md
OCC104 v021 Sprint Plan.md
OCC105 QA v020 Manual Test Log.md
OCC106 v021 Sprint A - CCW Cloud Tasks.md
OCC107 v021 Sprint B - CLI Local Tasks.md
OCC108 v021 Sprint Report.md
OCC109 v022 Sprint Report.md
OCC110 SDK Integration Guide - Transport State and Loop Features.md
```

## Analysis Requirements

### A. Gap Audit

For each document, identify:

1. **Explicit Commitments**
   - "Next steps" sections not completed
   - "TODO" items still pending
   - "Deferred to vX.X.X" items that should now be addressed

2. **Acceptance Criteria Gaps**
   - Sprint plans with incomplete acceptance criteria
   - QA test results showing failures not addressed
   - Performance requirements not met

3. **Technical Debt**
   - Workarounds marked "temporary" still in place
   - Known issues deferred without resolution
   - Documentation gaps (missing diagrams, incomplete guides)

4. **Process Gaps**
   - QA test specs without corresponding test logs
   - Sprint plans without completion reports
   - Integration guides without verification docs

5. **Cross-Document Inconsistencies**
   - Version mismatches (doc says v0.2.1, code is v0.2.0)
   - API contracts not reflected in implementation guides
   - Sprint reports referencing non-existent follow-up docs

### B. Forward Progress Analysis

Based on the current state (as of OCC110), recommend:

1. **Logical Next Features**
   - What capabilities are natural extensions of completed work?
   - What MVP requirements (from OCC026) are still pending?
   - What user workflows (from OCC024) are incomplete?

2. **Infrastructure Needs**
   - What build/test/CI improvements are needed?
   - What documentation gaps must be filled before scaling?
   - What performance optimizations are critical for production?

3. **Integration Opportunities**
   - What SDK features (from ORP docs) can be leveraged?
   - What third-party integrations are mentioned but not implemented?
   - What cross-platform work is needed (macOS, Windows, Linux)?

## Output Requirements

Produce **TWO comprehensive OCC-prefix documents**:

---

### **OCC111 Gap Audit Report - vX.X.X Missed Tasks and Technical Debt**

**Required Sections:**

1. **Executive Summary**
   - Total gaps found (count by category)
   - Risk assessment (critical, high, medium, low)
   - Estimated effort to close all gaps (person-weeks)

2. **Gap Analysis by Document**
   - For each OCC090-OCC110:
     - Document name and purpose
     - Gaps identified (with line number citations)
     - Impact if not addressed
     - Recommended priority

3. **Gap Analysis by Category**
   - Explicit commitments (grouped by severity)
   - Acceptance criteria gaps (grouped by feature area)
   - Technical debt (grouped by module)
   - Process gaps (grouped by workflow)
   - Cross-document inconsistencies (grouped by impact)

4. **Critical Path Blockers**
   - Gaps that block forward progress
   - Dependencies between gaps (must fix X before Y)
   - User-facing vs. internal gaps

5. **Quick Wins**
   - Gaps requiring <4 hours to close
   - High-impact, low-effort fixes

6. **Recommended Prioritization**
   - P0: Critical (blocks release)
   - P1: High (degrades UX)
   - P2: Medium (technical debt)
   - P3: Low (nice-to-have)

**Format Requirements:**

- Use IEEE citation style for references
- Include line number citations: `OCC093.md:208` format
- Provide estimated effort for each gap (hours or days)
- Use markdown tables for categorization
- Include git commit SHAs where applicable

---

### **OCC112 Sprint Roadmap - Gap Closure and Forward Progress**

**Required Sections:**

1. **Sprint Architecture Overview**
   - How many sprints recommended (total count)
   - Split between gap-closing vs. forward-progress
   - Estimated calendar timeline (weeks)

2. **Gap-Closing Sprints** (Series A)
   - **Sprint A1: [Title]**
     - Objective
     - Gaps addressed (with OCC111 references)
     - Deliverables (concrete artifacts)
     - Acceptance criteria
     - Estimated effort (person-days)
     - Dependencies (what must complete first)
   - **Sprint A2: [Title]**
     - [Same structure]
   - ... (as many as needed)

3. **Forward-Progress Sprints** (Series B)
   - **Sprint B1: [Title]**
     - Objective
     - New features/capabilities
     - Deliverables
     - Acceptance criteria
     - Estimated effort
     - Dependencies
   - **Sprint B2: [Title]**
     - [Same structure]
   - ... (as many as needed)

4. **Sprint Sequencing**
   - Dependency graph (ASCII diagram or table)
   - Parallel vs. sequential work
   - Critical path analysis

5. **Resource Requirements**
   - Skills needed per sprint (C++, JUCE, QA, docs)
   - Tooling requirements (profilers, test frameworks)
   - External dependencies (SDK features, third-party libs)

6. **Risk Assessment**
   - Technical risks per sprint
   - Mitigation strategies
   - Contingency plans

7. **Success Metrics**
   - Definition of "done" for each sprint
   - KPIs for gap closure (% of P0/P1 gaps closed)
   - KPIs for forward progress (features shipped, tests passing)

**Format Requirements:**

- Each sprint should be independently executable
- Clear entry/exit criteria for each sprint
- Reference OCC111 gaps by section number
- Include estimated person-days per sprint
- Use markdown task lists (`- [ ]`) for deliverables

---

## Quality Standards

1. **Precision**
   - Cite specific line numbers from source docs
   - Provide exact git commit SHAs when referencing code
   - Quote verbatim from source docs to support claims

2. **Actionability**
   - Every gap must have a recommended fix
   - Every sprint must have concrete deliverables
   - Every acceptance criterion must be testable

3. **Completeness**
   - Cover ALL 21 documents (OCC090-OCC110)
   - Don't skip gaps marked "low priority"
   - Include both code and documentation gaps

4. **Consistency**
   - Use OCC naming conventions (OCC### Descriptive Title.md)
   - Follow IEEE citation style
   - Maintain consistent effort estimation units

## Deliverables Summary

- **OCC111 Gap Audit Report - vX.X.X Missed Tasks and Technical Debt.md**
  - Comprehensive gap analysis
  - Prioritized by risk/impact
  - Estimated effort per gap

- **OCC112 Sprint Roadmap - Gap Closure and Forward Progress.md**
  - Series A sprints: Close gaps from OCC111
  - Series B sprints: New features and capabilities
  - Sequencing, resources, risks, success metrics

## Notes for CCW

- **Do NOT modify any code** - this is analysis/planning only
- **Do NOT read archived docs** (OCC001-OCC089 in archive/)
- **Do reference foundational docs** when needed:
  - OCC021 (Product Vision)
  - OCC026 (MVP Plan)
  - OCC027 (API Contracts)
- **Do consider SDK docs** if cross-referenced in OCC docs:
  - Check `/Users/chrislyons/dev/orpheus-sdk/docs/orp/` for SDK context
- **Do identify version mismatches** - current app is v0.2.2, docs may reference older versions

---

## Execution Instructions

Save this prompt and invoke CCW with:

```bash
# Method 1: Task tool with general-purpose agent
Use Task tool with subagent_type: "general-purpose"
Prompt: [This entire prompt]

# Method 2: Direct CCW invocation (if configured)
ccw --task "occ-audit" --input "OCC090-OCC110" --output "OCC111,OCC112"
```

**Expected Runtime:** 30-60 minutes (comprehensive analysis of 21 documents)

---

**END OF PROMPT**
