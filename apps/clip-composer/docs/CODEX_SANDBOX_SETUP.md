# Codex CLI Sandboxed Installation

## Overview

This directory contains **sandboxed wrapper scripts** for Anthropic's Codex CLI that restrict access to `/Users/chrislyons/dev/orpheus-sdk/apps/clip-composer` only. Codex CLI **cannot** access any other files or directories on your system.

## Security Features

- ✅ **Directory Isolation**: Codex can only see this directory
- ✅ **macOS Seatbelt**: Uses built-in macOS sandboxing
- ✅ **Global CLI**: Uses existing global codex installation
- ✅ **Wrapper Scripts**: Enforce sandbox restrictions automatically
- 🚫 **Cannot Access**: Any other directory on the system

## Available Launcher Scripts

| Script | Mode | Best For |
|--------|------|----------|
| `scripts/codex-sandbox.sh` | Interactive | General development, back-and-forth conversation |
| `scripts/codex-exec.sh` | Non-interactive | Single prompts, automation |
| `scripts/codex-auto.sh` | Full-auto | Low-friction automated execution |

## Usage

### Interactive Mode (Recommended)

```bash
cd /Users/chrislyons/dev/orpheus-sdk/apps/clip-composer
./scripts/codex-sandbox.sh
# Then type prompts in the interactive session
```

### Exec Mode (One-Shot)

```bash
cd /Users/chrislyons/dev/orpheus-sdk/apps/clip-composer
./scripts/codex-exec.sh "Write a hello world function"
./scripts/codex-exec.sh "Review the code in src/"
```

### Full-Auto Mode (Automatic Execution)

```bash
cd /Users/chrislyons/dev/orpheus-sdk/apps/clip-composer
./scripts/codex-auto.sh "Refactor this component"
```

### Authentication

Codex CLI uses your existing authentication from:
`~/.codex/auth.json`

If not authenticated, run:

```bash
codex login
```

## Model Configuration

Default model is `gpt-5.1-codex`. To override:

```bash
# Edit ~/.codex/config.toml
model = "gpt-5.1-codex"
model_reasoning_effort = "medium"

# Or override per-command
./scripts/codex-sandbox.sh -c model="o3"
```

## Trust Level

This project can be configured as "trusted" in `~/.codex/config.toml`:

```toml
[projects."/Users/chrislyons/dev/orpheus-sdk/apps/clip-composer"]
trust_level = "trusted"
```

This reduces confirmation prompts for file operations.

## Mode Comparison

**Interactive Mode** (`codex-sandbox.sh`):
- 💬 Back-and-forth conversation
- 🔄 Multi-turn sessions
- 📝 Best for: Development workflow, exploration

**Exec Mode** (`codex-exec.sh`):
- ⚡ Single prompt execution
- 🤖 Non-interactive
- 📝 Best for: Scripts, automation, CI/CD

**Full-Auto Mode** (`codex-auto.sh`):
- 🚀 Low-friction automatic execution
- 🔒 Network-disabled sandbox
- 📝 Best for: Quick refactors, isolated tasks

## Testing Isolation

Verify sandboxing works:

```bash
# This should work (current directory)
./scripts/codex-exec.sh "List files in current directory"

# This should FAIL or have limited access (other directories blocked)
./scripts/codex-exec.sh "List files in ~/Documents"
```

## Session Management

Codex stores sessions in `~/.codex/sessions/`.

To resume a previous session:

```bash
codex resume  # Picker to select session
codex resume --last  # Resume most recent
```

Note: Resume feature works globally, not directory-restricted.

## Advanced: Manual Sandbox Control

If you need fine-grained control, use `codex sandbox macos` directly:

```bash
cd /Users/chrislyons/dev/orpheus-sdk/apps/clip-composer
codex sandbox macos --log-denials codex "Your prompt"
```

The `--log-denials` flag shows what file access attempts are blocked.

## Removal

To remove sandbox wrappers:

```bash
cd /Users/chrislyons/dev/orpheus-sdk/apps/clip-composer
rm -rf scripts/codex-*.sh
rm docs/CODEX_SANDBOX_SETUP.md
```

The global `codex` installation remains unchanged.

---

**Installed**: Sun 16 Nov 2025 17:48:45 EST
**Codex CLI Version**: codex-cli 0.58.0
**Sandbox Method**: macOS Seatbelt
**Project**: clip-composer
