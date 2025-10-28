---
name: test.analyzer
description: Shared skill for parsing test output across multiple frameworks (Jest, pytest, ctest, cargo test) and generating actionable summaries.
---

# Test Analyzer (Shared Skill)

## Purpose

Cross-project test analysis skill adapted from SKIL003 framework. Provides unified test result parsing for multiple test frameworks with project-specific configuration.

## Configuration

Project-specific settings in `config.json`:

- Test frameworks used
- Coverage thresholds
- Module mapping
- Test output paths

## When to Use

After test runs across any supported framework. See project-specific test.result.analyzer for Orpheus-specific implementation.

## Allowed Tools

- `read_file` - Read test output
- `python` - Parse structured output

**Access Level:** 1 (Local Execution)

## Orpheus SDK Configuration

See `config.json` for Orpheus-specific settings.
