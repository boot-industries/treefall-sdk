# Pull Request

## Description

<!-- Provide a brief description of the changes in this PR -->

## Type of Change

<!-- Check all that apply -->

- [ ] Bug fix (non-breaking change which fixes an issue)
- [ ] New feature (non-breaking change which adds functionality)
- [ ] Breaking change (fix or feature that would cause existing functionality to not work as expected)
- [ ] Documentation update
- [ ] Performance improvement
- [ ] Code refactoring
- [ ] CI/CD update
- [ ] Test coverage improvement

## Related Issues

<!-- Link to related issues using #issue_number -->

Fixes #
Relates to #

## Testing

<!-- Describe the tests you ran to verify your changes -->

- [ ] Unit tests pass locally (`pnpm run test`)
- [ ] Integration tests pass locally
- [ ] Manual testing completed
- [ ] New tests added for new functionality

## C++ Changes (if applicable)

<!-- For changes to C++ code -->

- [ ] Compiles on Linux, macOS, and Windows
- [ ] All CTest tests pass
- [ ] AddressSanitizer and UBSan pass (Debug builds)
- [ ] Code follows `.clang-format` style
- [ ] Changes preserve determinism (same input → same output)
- [ ] No allocations in audio thread
- [ ] Sample-accurate timing maintained

## TypeScript/JavaScript Changes (if applicable)

<!-- For changes to TypeScript/JavaScript code -->

- [ ] TypeScript compilation passes (`pnpm run build`)
- [ ] ESLint passes (`pnpm run lint:js`)
- [ ] Code is formatted with Prettier
- [ ] No circular dependencies introduced
- [ ] Bundle size within budget (`pnpm run perf:validate`)

## Checklist

<!-- Complete before requesting review -->

- [ ] My code follows the style guidelines of this project
- [ ] I have performed a self-review of my own code
- [ ] I have commented my code, particularly in hard-to-understand areas
- [ ] I have made corresponding changes to the documentation
- [ ] My changes generate no new warnings
- [ ] I have added tests that prove my fix is effective or that my feature works
- [ ] New and existing unit tests pass locally with my changes
- [ ] Any dependent changes have been merged and published in downstream modules
- [ ] I have checked my code and corrected any misspellings
- [ ] Commit messages follow [Conventional Commits](https://www.conventionalcommits.org/) format

## Performance Impact

<!-- Describe any performance implications -->

- [ ] No performance impact
- [ ] Performance improved (provide metrics)
- [ ] Performance degraded (provide justification)

## Breaking Changes

<!-- If this PR introduces breaking changes, describe them here -->

## Screenshots (if applicable)

<!-- Add screenshots to help explain your changes -->

## Additional Context

<!-- Add any other context about the PR here -->

## Reviewer Notes

<!-- Any specific areas you'd like reviewers to focus on? -->
