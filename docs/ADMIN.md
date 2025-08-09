# ADMIN: Branch Protection (manual apply required)
Go to: Settings → Branches → Add rule
- Branch name pattern: main
- Require a pull request before merging (1 approving review)
- Require conversation resolution before merging
- Require linear history: ON
- Allow force pushes: OFF
- Allow deletions: OFF
- Include administrators: ON
(We will add "Required status checks" after CI exists in Step 3.)
