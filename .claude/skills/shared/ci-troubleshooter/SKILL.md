---
name: ci.troubleshooter
description: Shared skill for parsing CI/CD logs, identifying failure root causes, and suggesting fixes across GitHub Actions, Jenkins, and other CI systems.
---

# CI Troubleshooter (Shared Skill)

## Purpose

Cross-project CI/CD troubleshooting skill from SKIL003 framework. Parses build logs, identifies common failure patterns, and suggests targeted fixes.

## Configuration

Project-specific error patterns in `config.json`:

- Known error signatures
- Suggested fixes
- Skill cross-references
- Platform-specific patterns

## When to Use

After CI/CD failures, build errors, or deployment issues.

## Allowed Tools

- `read_file` - Read CI logs
- `bash` - Extract build artifacts

**Access Level:** 1 (Local Execution)

## Orpheus SDK Configuration

See `config.json` for Orpheus-specific error patterns and fixes.
