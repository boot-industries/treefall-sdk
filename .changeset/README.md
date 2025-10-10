# Changesets Workflow

This repository uses [`@changesets/cli`](https://github.com/changesets/changesets) to manage versioning across the pnpm workspace.

## Release expectations for contributors

1. **Create a changeset for user-facing work.**
   - Run `pnpm changeset` and follow the prompts to select affected packages.
   - Commit the generated markdown file in `.changeset/` alongside your code changes.
2. **Keep the base branch in sync.**
   - The release process targets the `main` branch. Make sure your feature branch stays rebased on `main` before opening a PR.
3. **Check pending releases locally.**
   - Use `pnpm changeset status --since=origin/main` to preview the release summary that will be produced by CI.
4. **Let automation publish.**
   - Merges to `main` trigger the release pipeline. Do not manually publish packages; the CI workflow handles version bumps and changelog updates.

For more details, review the [Changesets documentation](https://github.com/changesets/changesets).
