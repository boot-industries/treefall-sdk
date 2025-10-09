# Branch Protection Configuration

Branch protection rules must be configured manually in GitHub repository settings.

## Required Configuration

Navigate to: **Settings > Branches > Branch protection rules**

### Main Branch Protection

Create rule for branch: `main`

**Required settings:**

✅ **Require a pull request before merging**
- Require approvals: 1
- Dismiss stale pull request approvals when new commits are pushed

✅ **Require status checks to pass before merging**
- Require branches to be up to date before merging
- Required status checks:
  - `build-cpp (ubuntu-latest)`
  - `build-cpp (windows-latest)`
  - `build-cpp (macos-latest)`
  - `build-ui`
  - `lint-cpp`
  - `status-check`

✅ **Require conversation resolution before merging**

✅ **Require linear history**

✅ **Do not allow bypassing the above settings**

❌ **Do not allow force pushes**

❌ **Do not allow deletions**

## Verification

After configuration, test by:
1. Creating a test branch
2. Making a trivial change
3. Opening a PR
4. Verifying all status checks are required
5. Attempting to merge without approval (should fail)

## Notes

- These settings require repository admin access
- Rules apply to all contributors, including admins
- Bypass permissions should not be granted
