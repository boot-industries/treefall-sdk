# Troubleshooting Bootstrap Issues

Most setup issues stem from outdated toolchains or partial installs. Before filing a ticket:

1. Verify that your Node.js version is 18.x or newer.
2. Confirm PNPM 8.x is installed (`pnpm --version`).
3. Ensure CMake 3.20+ is available on your PATH.
4. Re-run `pnpm install` from the repository root.
5. Check `scripts/validate-phase0.sh` output for additional diagnostics.

If problems persist, consult ORP068 and the Phase 0 validation checklist in `docs/` for platform-specific guidance.
