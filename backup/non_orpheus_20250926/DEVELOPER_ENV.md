# Developer Environment Baseline
- CI runner: GitHub Actions `ubuntu-latest`.
- Formatting: use language-standard tools (clang-format for C/C++; black/ruff for Python; eslint/prettier for JS/TS).
- Static analysis: clang-tidy / ruff / eslint as applicable.
- Phase 1 policy: non-functional refactor only — public APIs/exports MUST NOT change.
- After Step 3 (CI), enable required status checks on `main`.
