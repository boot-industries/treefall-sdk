# Clip Composer - Agent Definitions

**Purpose:** Specialized AI agent configurations for common Clip Composer development tasks

---

## Available Agents

### 1. `build-and-test.md`

**Use case:** Build the application, run tests, and verify compilation
**Triggers:**

- After significant code changes
- Before creating pull requests
- When testing new features

### 2. `waveform-optimization.md`

**Use case:** Profile and optimize waveform rendering performance
**Triggers:**

- Waveform display is slow (>200ms generation)
- UI feels sluggish during clip editing
- Need to benchmark downsampling algorithms

### 3. `session-validator.md`

**Use case:** Validate session JSON files for correctness and completeness
**Triggers:**

- Session fails to load
- Need to verify backward compatibility
- Creating test fixtures

### 4. `release-builder.md`

**Use case:** Build, package, and prepare releases (DMG, installer, etc.)
**Triggers:**

- Ready to create a new release
- Need to test release builds
- Creating distribution packages

---

## How to Use Agents

Agents are specialized prompts that Claude Code can use to perform complex, multi-step tasks autonomously. To invoke an agent:

1. **Direct invocation:** "Use the build-and-test agent"
2. **Task-based:** "Build and test the application" (Claude will select appropriate agent)
3. **Context-aware:** Claude may proactively suggest agents when relevant

---

## Creating New Agents

When creating a new agent definition:

1. **Clear purpose:** Single, well-defined responsibility
2. **Entry conditions:** When should this agent be used?
3. **Exit criteria:** How do we know the task is complete?
4. **Tool usage:** Which tools does this agent need?
5. **Error handling:** What happens if something fails?

**Template:**

```markdown
# Agent: [Name]

## Purpose

[1-2 sentence description of what this agent does]

## Triggers

- [Condition 1]
- [Condition 2]

## Process

1. [Step 1]
2. [Step 2]
3. [Step 3]

## Success Criteria

- [Criterion 1]
- [Criterion 2]

## Tools Required

- [Tool 1]
- [Tool 2]

## Error Handling

- [Error scenario 1]: [How to handle]
- [Error scenario 2]: [How to handle]
```

---

**Last Updated:** October 22, 2025
**Agent Count:** 4
**Next Review:** After v0.2.0 planning or when new common workflows emerge
