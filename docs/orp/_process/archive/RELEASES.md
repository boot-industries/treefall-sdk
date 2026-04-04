# Release and Versioning Strategy

The Orpheus SDK monorepo uses [Changesets](https://github.com/changesets/changesets) with an **independent**
versioning strategy. Each published `@orpheus/*` package receives semver bumps based on the scope of the
changes that land in a changeset. This keeps SDK packages and the UI toolkit decoupled while still allowing
coordinated releases when needed.

## Managed Packages

Changesets is configured to track the following workspaces:

- `@orpheus/engine-native`
- `@orpheus/shmui`

New packages should be added to `.changeset/config.json` and the workspace manifest before publishing.

## Creating Release Notes

```bash
pnpm changeset
```

Running the command above starts an interactive prompt scoped to the package graph. Choose the affected
packages, provide the semver bump type, and add a short summary. Changesets stores the metadata under
`.changeset/` until you are ready to release.

## Versioning and Publishing

To preview the upcoming release plan without writing git tags, run:

```bash
pnpm changeset version --no-git-tag-version
```

When CI is configured, it will execute `pnpm changeset version` followed by
`pnpm changeset publish` as part of the release pipeline.

## Pre-release Channels

Pre-release channels are supported via Changesets' `pre` command. Use the following flow to enter and exit
channels:

```bash
# Enter beta channel
pnpm changeset pre enter beta

# Create changesets as usual, then generate prerelease versions
pnpm changeset version

# Publish to the beta tag
pnpm changeset publish --tag beta

# When ready for release candidates
pnpm changeset pre enter rc

# Exit prerelease mode before shipping stable versions
pnpm changeset pre exit
```

This workflow supports the `beta` and `rc` channels required in Phase 0. Document the channel you are using
in the PR description so reviewers know which pipeline to invoke.
