# Integration Tests

This directory contains integration smoke tests for the Orpheus SDK packages.

## Purpose

These tests verify:
1. Package builds are successful and importable
2. OrpheusClient can be instantiated with various configurations
3. Basic API surface is available
4. Cross-package dependencies work correctly

## Running Tests

### Locally

```bash
# From repository root
pnpm run test:integration

# Or run directly
./tests/integration/run-tests.sh

# Or run smoke tests only
node --test tests/integration/smoke.test.mjs
```

### In CI

Integration tests run automatically in the `integration-tests` job of `.github/workflows/interim-ci.yml` after the `build-ui` job completes.

## Test Files

### `smoke.test.mjs`

Basic smoke tests using Node's built-in test runner:

- **Package Imports**: Verifies @orpheus/contract, @orpheus/client, and @orpheus/react can be imported
- **OrpheusClient Instantiation**: Tests client creation with default and custom configs
- **API Surface**: Validates expected methods and properties exist
- **Contract Schemas**: Basic schema package validation

**Requirements:**
- Packages must be built (`pnpm run build`)
- Node.js 18+ (uses `node:test` module)

### `run-tests.sh`

Test runner script that:
1. Verifies package builds exist
2. Runs smoke tests
3. Reports on service/native driver availability

## Adding New Tests

### Smoke Tests

Add new test cases to `smoke.test.mjs`:

```javascript
describe('New Feature', () => {
  test('feature works correctly', async () => {
    const clientPath = resolve(rootDir, 'packages/client/dist/index.js');
    const { OrpheusClient } = await import(clientPath);

    // Your test here
    assert.ok(true);
  });
});
```

### Integration Tests with Service Driver

For tests requiring orpheusd:

```javascript
// tests/integration/service-driver.test.mjs
import { test, describe, before, after } from 'node:test';
import { spawn } from 'node:child_process';

let serviceProcess;

before(async () => {
  // Start orpheusd
  serviceProcess = spawn('node', ['packages/engine-service/dist/index.js']);
  await new Promise(resolve => setTimeout(resolve, 1000));
});

after(() => {
  // Stop orpheusd
  serviceProcess.kill();
});

describe('Service Driver Integration', () => {
  test('can connect to service', async () => {
    // Test here
  });
});
```

## CI Integration

The `.github/workflows/interim-ci.yml` workflow includes:

```yaml
integration-tests:
  name: Integration Smoke Tests
  runs-on: ubuntu-latest
  needs: [build-ui]
  steps:
    - uses: actions/checkout@v4
    - name: Set up Node
    - name: Install dependencies
    - name: Build packages
    - name: Run integration tests
      run: pnpm run test:integration
```

Test failures will:
- Block PR merge (via `status-check` job)
- Show in CI logs
- Upload test artifacts on failure

## Troubleshooting

### "Cannot find package" errors

Ensure packages are built:
```bash
pnpm run build
```

### "Module not found" errors

Check that file paths in tests use `resolve(rootDir, 'packages/...')` pattern.

### Service driver tests skipped

Service driver tests require:
1. Service driver built (`pnpm --filter @orpheus/engine-service build`)
2. orpheusd running (`node packages/engine-service/dist/index.js`)

### Native driver tests skipped

Native driver tests require:
1. C++ SDK built (`cmake --build build-release`)
2. Native addon built (`pnpm --filter @orpheus/engine-native build:native`)

## Future Enhancements

- [ ] Service driver integration tests (requires orpheusd startup)
- [ ] Native driver integration tests (requires C++ build)
- [ ] Contract schema validation tests (with AJV)
- [ ] React hook rendering tests (with @testing-library/react)
- [ ] End-to-end workflow tests (load session → execute command → verify event)

## Related Documentation

- [Driver Integration Guide](../../docs/DRIVER_INTEGRATION_GUIDE.md)
- [Contract Development Guide](../../docs/CONTRACT_DEVELOPMENT.md)
- [ORP068 Implementation Plan](../../docs/ORP/ORP068%20Implementation%20Plan%20v2.0_%20Orpheus%20SDK%20×%20Shmui%20Integration%20.md)
