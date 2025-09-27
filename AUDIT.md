<!-- SPDX-License-Identifier: MIT -->
# Repository Audit

Date: 2025-09-26 (UTC)

## Summary

An audit was performed to separate legacy, non-Orpheus assets from the active
codebase. The following actions were taken:

- Identified historical REAPER SDK samples and utilities that pre-date the
  Orpheus initiative.
- Moved the legacy trees into a quarantine directory for reference without
  impacting new development.

## Quarantined Assets

The items below were relocated to `backup/non_orpheus_20250926/`:

- `.clang-tidy`
- `.github/`
- `.gitmodules`
- `CMakeLists.txt`
- `DEVELOPER_ENV.md`
- `README`
- `docs/`
- `reaper-plugins/`
- `run-clang-tidy.py`
- `sdk/`
- `scripts/`
- `WDL/`

These directories and files contain legacy SDK samples, build scripts, and
third-party dependencies that are no longer part of the core Orpheus
initiative. They remain available in the backup directory for historical
reference.

## Next Steps

- Stand up a fresh Orpheus-oriented build system and source tree.
- Reintroduce components incrementally with appropriate ownership and tooling.
- Continue to track quarantined content until it can be archived or migrated.
